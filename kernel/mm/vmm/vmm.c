#include "kernel/mm/vmm/vmm.h"
#include "kernel/mm/pmm/pmm.h"
#include "kernel/boot/boot.h"

#include <stddef.h>

#include "../../../limine.h"

/*
 * x86-64 4-level paging
 *
 * PML4
 *  ↓
 * PDPT
 *  ↓
 * PD
 *  ↓
 * PT
 *  ↓
 * Physical frame
 */

#define PAGE_TABLE_ENTRIES 512
#define ADDRESS_MASK 0x000FFFFFFFFFF000ULL

/*
 * x86 hardware-managed page-table bits.
 *
 * The CPU may update these bits while an address space
 * is actively being used:
 *
 *     bit 5 = Accessed
 *     bit 6 = Dirty
 *
 * Recursive-clone verification must not treat hardware
 * updates to these bits as a clone failure.
 */
#define VMM_HARDWARE_MANAGED_BITS \
    ((1ULL << 5) | (1ULL << 6))

/*
 * VMM-3.2A:
 *
 * Track only page-table frames created by the VMM.
 *
 * We intentionally do NOT create per-physical-frame metadata
 * inside the PMM. The PMM remains responsible for physical
 * frame availability, while the VMM owns page-table lifecycle
 * metadata.
 *
 * This registry is deliberately bounded for the current
 * kernel-development stage. It can be replaced by a dynamically
 * managed structure when the kernel gains a general-purpose
 * allocator.
 */
#define VMM_MAX_PAGE_TABLES 4096

enum vmm_page_table_level
{
    VMM_LEVEL_PT = 0,
    VMM_LEVEL_PD = 1,
    VMM_LEVEL_PDPT = 2,
    VMM_LEVEL_PML4 = 3
};

struct vmm_page_table_record
{
    uint64_t physical_address;
    uint64_t owner_pml4;
    uint64_t validation_epoch;
    uint8_t level;
    uint8_t in_use;
};

static struct vmm_page_table_record
    page_table_records[VMM_MAX_PAGE_TABLES];

static uint64_t page_table_record_count = 0;
static uint64_t validation_epoch = 0;

static uint64_t kernel_pml4 = 0;

#ifdef BATOS_VMM_TEST
/*
 * Private test-only failure injection.
 *
 * This symbol is intentionally not declared in vmm.h.
 * Production VMM users therefore cannot depend on it.
 */
int vmm_test_fail_pt_allocation = 0;
#endif

/*
 * Standalone root returned by the most recent
 * recursive clone operation.
 *
 * Retained for VMM structural verification until
 * formal page-table ownership and cleanup exist.
 */
static uint64_t last_cloned_pml4 = 0;

/*
 * VMM-3.2B:
 *
 * Track the lifecycle of address-space roots.
 *
 * VMM owns the address-space lifecycle while PMM owns
 * physical-frame availability.
 *
 * A newly created address space starts in CREATED state.
 * Only a registered address space may become ACTIVE.
 */
#define VMM_MAX_ADDRESS_SPACES 256

struct vmm_address_space_record
{
    uint64_t pml4_physical;
    uint8_t state;
    uint8_t in_use;
};

static struct vmm_address_space_record
    address_space_records[VMM_MAX_ADDRESS_SPACES];

static uint64_t address_space_record_count = 0;

/*
 * Physical PML4 currently known to be ACTIVE.
 */
static uint64_t active_address_space_pml4 = 0;

/*
 * Convert a physical address into a virtual address
 * using Limine's Higher Half Direct Map.
 */
static uint64_t *physical_to_virtual(uint64_t physical)
{
    return (uint64_t *)(physical + pmm_get_hhdm_offset());
}

/*
 * Zero one 4 KiB page.
 */
static void zero_page(uint64_t physical)
{
    uint64_t *page =
        physical_to_virtual(physical);

    for (uint64_t i = 0;
         i < PAGE_TABLE_ENTRIES;
         i++)
    {
        page[i] = 0;
    }
}

/*
 * Register one VMM-owned page-table frame.
 */
static int register_page_table(
    uint64_t physical_address,
    int level,
    uint64_t owner_pml4
)
{
    if (physical_address == 0)
        return -1;

    if (level < VMM_LEVEL_PT ||
        level > VMM_LEVEL_PML4)
        return -1;

    struct vmm_page_table_record *record = NULL;

    /*
     * Reuse an inactive ownership slot before consuming
     * a new append-only slot.
     */
    for (uint64_t i = 0;
         i < page_table_record_count;
         i++)
    {
        if (!page_table_records[i].in_use)
        {
            record = &page_table_records[i];
            break;
        }
    }

    if (record == NULL)
    {
        if (page_table_record_count >=
            VMM_MAX_PAGE_TABLES)
        {
            return -1;
        }

        record =
            &page_table_records[
                page_table_record_count
            ];

        page_table_record_count++;
    }

    record->physical_address = physical_address;
    record->owner_pml4 = owner_pml4;
    record->level = (uint8_t)level;
    record->in_use = 1;

    return 0;
}

/*
 * Allocate and initialize one page-table page.
 *
 * The caller specifies:
 *
 *     level      = PML4 / PDPT / PD / PT
 *     owner_pml4 = address-space root
 *
 * VMM-3.2A establishes explicit VMM ownership metadata
 * for every page-table frame created by the VMM.
 */
static uint64_t allocate_page_table(
    int level,
    uint64_t owner_pml4
)
{
#ifdef BATOS_VMM_TEST
    /*
     * VMM transactional rollback negative-path test.
     *
     * The test deliberately fails PT allocation after
     * PDPT and PD creation. This lets vmm_map_page()
     * exercise its complete rollback path.
     */
    extern int vmm_test_fail_pt_allocation;

    if (level == VMM_LEVEL_PT &&
        vmm_test_fail_pt_allocation)
    {
        return 0;
    }
#endif

    uint64_t physical =
        pmm_alloc_frame();

    if (physical == 0)
        return 0;

    zero_page(physical);

    /*
     * A PML4 is the root owner of its address space.
     */
    if (level == VMM_LEVEL_PML4 &&
        owner_pml4 == 0)
    {
        owner_pml4 = physical;
    }

    if (register_page_table(
            physical,
            level,
            owner_pml4
        ) != 0)
    {
        pmm_free_frame(physical);
        return 0;
    }

    return physical;
}

/*
 * Find the ownership record for a page-table frame.
 */
static struct vmm_page_table_record *
find_page_table_record(uint64_t physical_address)
{
    if (physical_address == 0)
        return NULL;

    for (uint64_t i = 0;
         i < page_table_record_count;
         i++)
    {
        struct vmm_page_table_record *record =
            &page_table_records[i];

        if (record->in_use &&
            record->physical_address ==
                physical_address)
        {
            return record;
        }
    }

    return NULL;
}

/*
 * Get an existing child page table or create one.
 *
 * If created is non-NULL:
 *
 *     *created = 0  -> existing table
 *     *created = 1  -> newly allocated table
 *
 * The caller can therefore roll back only the table
 * created by its current operation.
 */
/*
 * Verify one VMM page-table ownership record.
 */
static int verify_page_table_record(
    uint64_t physical_address,
    int expected_level,
    uint64_t expected_owner_pml4
);

/*
 * Verify that an existing child page-table entry is a
 * registered VMM-owned table of the expected level.
 *
 * This is intentionally path-local validation. It does
 * not perform a full address-space traversal.
 */
static uint64_t verify_existing_child_table(
    uint64_t entry,
    int expected_level,
    uint64_t owner_pml4
)
{
    if (!(entry & VMM_PRESENT))
        return 0;

    if (entry & VMM_HUGE)
        return 0;

    uint64_t child =
        entry & ADDRESS_MASK;

    if (verify_page_table_record(
            child,
            expected_level,
            owner_pml4
        ) != 0)
    {
        return 0;
    }

    return child;
}

/*
 * Get an existing child page table or create one.
 *
 * If created is non-NULL:
 *
 *     *created = 0  -> existing table
 *     *created = 1  -> newly allocated table
 *
 * Existing tables are accepted only when their VMM
 * ownership metadata matches the requested level and
 * address-space owner.
 */
static uint64_t get_or_create_table(
    uint64_t *parent,
    uint64_t index,
    uint64_t flags,
    int child_level,
    uint64_t owner_pml4,
    int *created
)
{
    if (created != NULL)
        *created = 0;

    uint64_t entry =
        parent[index];

    if (entry & VMM_PRESENT)
    {
        return verify_existing_child_table(
            entry,
            child_level,
            owner_pml4
        );
    }

    uint64_t child =
        allocate_page_table(
            child_level,
            owner_pml4
        );

    if (child == 0)
        return 0;

    parent[index] =
        child |
        VMM_PRESENT |
        VMM_WRITABLE |
        (flags & VMM_USER);

    if (created != NULL)
        *created = 1;

    return child;
}

/*
 * Release one VMM-owned page-table frame.
 *
 * This is used for transactional rollback of a page
 * table created by the current mapping operation.
 *
 * The ownership registry remains append-only. The
 * corresponding record is marked inactive instead of
 * decreasing page_table_record_count.
 */
static int release_page_table(
    uint64_t physical_address
)
{
    struct vmm_page_table_record *record =
        find_page_table_record(
            physical_address
        );

    if (record == NULL)
        return -1;

    if (!record->in_use)
        return -1;

    record->in_use = 0;

    pmm_free_frame(
        physical_address
    );

    return 0;
}

/*
 * Roll back one newly-created child page table.
 *
 * The parent entry must still reference exactly this
 * child before it can be detached and released.
 */
static int rollback_page_table(
    uint64_t *parent,
    uint64_t index,
    uint64_t child_physical
)
{
    if (parent == NULL ||
        child_physical == 0)
    {
        return -1;
    }

    uint64_t entry =
        parent[index];

    if (!(entry & VMM_PRESENT))
        return -1;

    if ((entry & ADDRESS_MASK) !=
        child_physical)
    {
        return -1;
    }

    parent[index] = 0;

    return release_page_table(
        child_physical
    );
}

/*
 * x86-64 page-table indexes.
 */
static uint64_t pml4_index(uint64_t virtual_address)
{
    return (virtual_address >> 39) & 0x1FF;
}

static uint64_t pdpt_index(uint64_t virtual_address)
{
    return (virtual_address >> 30) & 0x1FF;
}

static uint64_t pd_index(uint64_t virtual_address)
{
    return (virtual_address >> 21) & 0x1FF;
}

static uint64_t pt_index(uint64_t virtual_address)
{
    return (virtual_address >> 12) & 0x1FF;
}


#ifdef BATOS_VMM_TEST
/*
 * Verify that the PML4 slot selected by a test virtual
 * address is genuinely unused before a transactional
 * rollback test begins.
 *
 * Test-only helper. Not part of the production VMM API.
 */
int vmm_test_is_pml4_slot_empty(
    uint64_t pml4_physical,
    uint64_t virtual_address
)
{
    if (pml4_physical == 0)
        return 0;

    uint64_t *pml4 =
        physical_to_virtual(pml4_physical);

    return pml4[pml4_index(virtual_address)] == 0;
}
#endif

/*
 * Verify one VMM page-table ownership record.
 */
static int verify_page_table_record(
    uint64_t physical_address,
    int expected_level,
    uint64_t expected_owner_pml4
)
{
    struct vmm_page_table_record *record =
        find_page_table_record(
            physical_address
        );

    if (record == NULL)
        return -1;

    if (!record->in_use)
        return -1;

    if (record->level != expected_level)
        return -1;

    if (record->owner_pml4 != expected_owner_pml4)
        return -1;

    return 0;
}

/*
 * Verify that a PML4 owns itself.
 */
int vmm_verify_page_table_root(
    uint64_t pml4_physical
)
{
    if (pml4_physical == 0)
        return -1;

    return verify_page_table_record(
        pml4_physical,
        VMM_LEVEL_PML4,
        pml4_physical
    );
}

/*
 * Verify ownership of the complete page-table path:
 *
 *     PML4 -> PDPT -> PD -> PT
 */
int vmm_verify_page_table_ownership(
    uint64_t pml4_physical,
    uint64_t virtual_address
)
{
    if (pml4_physical == 0)
        return -1;

    /*
     * Verify root ownership first.
     */
    if (verify_page_table_record(
            pml4_physical,
            VMM_LEVEL_PML4,
            pml4_physical
        ) != 0)
    {
        return -1;
    }

    uint64_t *pml4 =
        physical_to_virtual(
            pml4_physical
        );

    uint64_t pml4_entry =
        pml4[pml4_index(virtual_address)];

    if (!(pml4_entry & VMM_PRESENT))
        return -1;

    /*
     * PML4-level huge mappings are not part of the current
     * four-level BATOS address-space contract.
     */
    if (pml4_entry & VMM_HUGE)
        return -1;

    uint64_t pdpt_physical =
        pml4_entry & ADDRESS_MASK;

    if (verify_page_table_record(
            pdpt_physical,
            VMM_LEVEL_PDPT,
            pml4_physical
        ) != 0)
    {
        return -1;
    }

    uint64_t *pdpt =
        physical_to_virtual(
            pdpt_physical
        );

    uint64_t pdpt_entry =
        pdpt[pdpt_index(virtual_address)];

    if (!(pdpt_entry & VMM_PRESENT))
        return -1;

    /*
     * A present huge PDPT entry is a valid 1 GiB leaf.
     * Ownership terminates at the PDPT table itself.
     */
    if (pdpt_entry & VMM_HUGE)
        return 0;

    uint64_t pd_physical =
        pdpt_entry & ADDRESS_MASK;

    if (verify_page_table_record(
            pd_physical,
            VMM_LEVEL_PD,
            pml4_physical
        ) != 0)
    {
        return -1;
    }

    uint64_t *pd =
        physical_to_virtual(
            pd_physical
        );

    uint64_t pd_entry =
        pd[pd_index(virtual_address)];

    if (!(pd_entry & VMM_PRESENT))
        return -1;

    /*
     * A present huge PD entry is a valid 2 MiB leaf.
     * No PT page exists below this mapping.
     */
    if (pd_entry & VMM_HUGE)
        return 0;

    uint64_t pt_physical =
        pd_entry & ADDRESS_MASK;

    if (verify_page_table_record(
            pt_physical,
            VMM_LEVEL_PT,
            pml4_physical
        ) != 0)
    {
        return -1;
    }

    return 0;
}

/*
 * Find a registered address-space record.
 */
static struct vmm_address_space_record *
find_address_space_record(uint64_t pml4_physical)
{
    if (pml4_physical == 0)
        return NULL;

    for (uint64_t i = 0;
         i < address_space_record_count;
         i++)
    {
        struct vmm_address_space_record *record =
            &address_space_records[i];

        if (record->in_use &&
            record->pml4_physical ==
                pml4_physical)
        {
            return record;
        }
    }

    return NULL;
}

/*
 * Register one newly created address space.
 */
static int register_address_space(
    uint64_t pml4_physical
)
{
    if (pml4_physical == 0)
        return -1;

    if (find_address_space_record(
            pml4_physical) != NULL)
    {
        return -1;
    }

    struct vmm_address_space_record *record = NULL;

    /*
     * Reuse an inactive lifecycle slot before consuming
     * a new append-only slot.
     */
    for (uint64_t i = 0;
         i < address_space_record_count;
         i++)
    {
        if (!address_space_records[i].in_use)
        {
            record = &address_space_records[i];
            break;
        }
    }

    if (record == NULL)
    {
        if (address_space_record_count >=
            VMM_MAX_ADDRESS_SPACES)
        {
            return -1;
        }

        record =
            &address_space_records[
                address_space_record_count
            ];

        address_space_record_count++;
    }

    record->pml4_physical =
        pml4_physical;

    record->state =
        VMM_ADDRESS_SPACE_CREATED;

    record->in_use = 1;

    return 0;
}

/*
 * Validate one complete VMM-owned page-table hierarchy.
 *
 * Only page-table frames belonging to the supplied root may
 * participate in this hierarchy. Leaf physical frames are
 * deliberately not ownership-managed here.
 *
 * validation_epoch also enforces tree uniqueness. A VMM-owned
 * page-table frame may appear only once in one address-space
 * hierarchy. This rejects both duplicate references and cycles
 * before destruction begins.
 */
static int validate_address_space_tree(
    uint64_t physical,
    int level,
    uint64_t owner_pml4,
    uint64_t epoch
)
{
    if (physical == 0)
        return -1;

    struct vmm_page_table_record *record =
        find_page_table_record(
            physical
        );

    if (record == NULL ||
        !record->in_use ||
        record->level != level ||
        record->owner_pml4 != owner_pml4)
    {
        return -1;
    }

    if (record->validation_epoch == epoch)
        return -1;

    record->validation_epoch = epoch;

    uint64_t *table =
        physical_to_virtual(physical);

    if (level == VMM_LEVEL_PT)
        return 0;

    for (uint64_t index = 0;
         index < PAGE_TABLE_ENTRIES;
         index++)
    {
        uint64_t entry = table[index];

        if (!(entry & VMM_PRESENT))
            continue;

        /*
         * Huge entries at PDPT/PD levels are mapping leaves,
         * not references to child page-table frames.
         *
         * A PML4-level huge mapping remains outside the
         * current four-level BATOS contract.
         */
        if (entry & VMM_HUGE)
        {
            if (level == VMM_LEVEL_PML4)
                return -1;

            continue;
        }

        uint64_t child =
            entry & ADDRESS_MASK;

        if (validate_address_space_tree(
                child,
                level - 1,
                owner_pml4,
                epoch
            ) != 0)
        {
            return -1;
        }
    }

    return 0;
}

/*
 * Reclaim one previously validated VMM-owned page-table
 * hierarchy.
 *
 * This function is entered only after complete structural
 * validation has succeeded. Therefore every reachable page-table
 * frame is registered, live, correctly owned, correctly leveled,
 * and unique within the hierarchy.
 *
 * Reclamation itself has no expected failure path.
 */
static void destroy_address_space_tree(
    uint64_t physical,
    int level,
    uint64_t owner_pml4
)
{
    uint64_t *table =
        physical_to_virtual(physical);

    if (level != VMM_LEVEL_PT)
    {
        for (uint64_t index = 0;
             index < PAGE_TABLE_ENTRIES;
             index++)
        {
            uint64_t entry = table[index];

            if (!(entry & VMM_PRESENT))
                continue;

            /*
             * Huge PDPT/PD entries are mapping leaves, not
             * page-table references. Remove only the mapping
             * entry; never release the mapped physical frame
             * as a page table.
             *
             * PML4-level huge entries are rejected by complete
             * validation before destruction begins.
             */
            if (entry & VMM_HUGE)
            {
                table[index] = 0;
                continue;
            }

            uint64_t child =
                entry & ADDRESS_MASK;

            table[index] = 0;

            destroy_address_space_tree(
                child,
                level - 1,
                owner_pml4
            );
        }
    }

    release_page_table(physical);
}

/*
 * Destroy a registered, inactive address space.
 *
 * Validation is performed before mutation so malformed
 * ownership state cannot produce partial reclamation.
 */
int vmm_destroy_address_space(
    uint64_t pml4_physical
)
{
    if (pml4_physical == 0)
        return -1;

    if (pml4_physical ==
        active_address_space_pml4)
    {
        return -1;
    }

    struct vmm_address_space_record *record =
        find_address_space_record(
            pml4_physical
        );

    if (record == NULL)
        return -1;

    if (record->state ==
        VMM_ADDRESS_SPACE_ACTIVE)
    {
        return -1;
    }

    if (vmm_verify_page_table_root(
            pml4_physical
        ) != 0)
    {
        return -1;
    }

    /*
     * Start a fresh structural-validation epoch.
     *
     * A wrapped epoch is cleared from all records before reuse,
     * preserving the uniqueness invariant indefinitely.
     */
    validation_epoch++;

    if (validation_epoch == 0)
    {
        validation_epoch = 1;

        for (uint64_t i = 0;
             i < page_table_record_count;
             i++)
        {
            page_table_records[i].validation_epoch = 0;
        }
    }

    if (validate_address_space_tree(
            pml4_physical,
            VMM_LEVEL_PML4,
            pml4_physical,
            validation_epoch
        ) != 0)
    {
        return -1;
    }

    destroy_address_space_tree(
        pml4_physical,
        VMM_LEVEL_PML4,
        pml4_physical
    );

    record->state =
        VMM_ADDRESS_SPACE_INACTIVE;

    record->in_use = 0;

    if (last_cloned_pml4 ==
        pml4_physical)
    {
        last_cloned_pml4 = 0;
    }

    return 0;
}

/*
 * Initialize BATOS Virtual Memory Manager.
 */
void vmm_init(void)
{
    kernel_pml4 =
        vmm_create_address_space();

    last_cloned_pml4 = 0;
}

/*
 * Create a new empty 4-level address space.
 *
 * Returns the physical address of the PML4.
 */
uint64_t vmm_create_address_space(void)
{
    uint64_t pml4_physical =
        allocate_page_table(
            VMM_LEVEL_PML4,
            0
        );

    if (pml4_physical == 0)
        return 0;

    /*
     * Every VMM-created address space must be
     * registered before it can be activated.
     */
    if (register_address_space(
            pml4_physical
        ) != 0)
    {
        release_page_table(
            pml4_physical
        );

        return 0;
    }

    return pml4_physical;
}

/*
 * Map one contiguous kernel-image section.
 */
static int map_kernel_section(
    uint64_t pml4_physical,
    uint64_t virtual_base,
    uint64_t physical_base,
    uint64_t image_end,
    uint64_t section_start,
    uint64_t section_end,
    uint64_t flags
)
{
    if (section_start >= section_end)
        return 0;

    if (section_start < virtual_base ||
        section_end > image_end)
    {
        return -1;
    }

    uint64_t start =
        section_start &
        ~(VMM_PAGE_SIZE - 1);

    uint64_t end =
        (section_end +
         (VMM_PAGE_SIZE - 1)) &
        ~(VMM_PAGE_SIZE - 1);

    if (end < section_end ||
        end > image_end)
    {
        return -1;
    }

    for (uint64_t virtual_address = start;
         virtual_address < end;
         virtual_address += VMM_PAGE_SIZE)
    {
        uint64_t offset =
            virtual_address - virtual_base;

        if (offset > UINT64_MAX - physical_base)
            return -1;

        uint64_t physical_address =
            physical_base + offset;

        if (vmm_map_page(
                pml4_physical,
                virtual_address,
                physical_address,
                flags
            ) != 0)
        {
            return -1;
        }
    }

    return 0;
}

/*
 * Create a registered address space containing the
 * supervisor-only BATOS kernel image.
 *
 * The kernel image's existing physical frames are shared.
 * Only the new page-table hierarchy is owned and reclaimed
 * by this address-space lifecycle.
 */
uint64_t vmm_create_kernel_address_space(void)
{
    uint64_t virtual_base =
        boot_get_kernel_virtual_base();

    uint64_t physical_base =
        boot_get_kernel_physical_base();

    uint64_t image_size =
        boot_get_kernel_image_size();

    if (virtual_base == 0 ||
        physical_base == 0 ||
        image_size == 0)
    {
        return 0;
    }

    if ((virtual_base &
         (VMM_PAGE_SIZE - 1)) != 0 ||
        (physical_base &
         (VMM_PAGE_SIZE - 1)) != 0)
    {
        return 0;
    }

    if (image_size >
        UINT64_MAX - virtual_base)
    {
        return 0;
    }

    uint64_t image_end =
        virtual_base + image_size;

    if (image_end >
        UINT64_MAX - (VMM_PAGE_SIZE - 1))
    {
        return 0;
    }

    uint64_t image_mapping_end =
        (image_end +
         (VMM_PAGE_SIZE - 1)) &
        ~(VMM_PAGE_SIZE - 1);

    if (image_mapping_end < image_end)
        return 0;

    uint64_t pml4_physical =
        vmm_create_address_space();

    if (pml4_physical == 0)
        return 0;

    /*
     * .limine_requests is part of the current RW kernel
     * segment and therefore remains supervisor-writable.
     */
    if (map_kernel_section(
            pml4_physical,
            virtual_base,
            physical_base,
            image_mapping_end,
            virtual_base,
            boot_get_kernel_text_start(),
            VMM_WRITABLE
        ) != 0)
    {
        vmm_destroy_address_space(pml4_physical);
        return 0;
    }

    if (map_kernel_section(
            pml4_physical,
            virtual_base,
            physical_base,
            image_mapping_end,
            boot_get_kernel_text_start(),
            boot_get_kernel_text_end(),
            0
        ) != 0 ||
        map_kernel_section(
            pml4_physical,
            virtual_base,
            physical_base,
            image_mapping_end,
            boot_get_kernel_rodata_start(),
            boot_get_kernel_rodata_end(),
            0
        ) != 0 ||
        map_kernel_section(
            pml4_physical,
            virtual_base,
            physical_base,
            image_mapping_end,
            boot_get_kernel_data_start(),
            boot_get_kernel_data_end(),
            VMM_WRITABLE
        ) != 0 ||
        map_kernel_section(
            pml4_physical,
            virtual_base,
            physical_base,
            image_mapping_end,
            boot_get_kernel_bss_start(),
            boot_get_kernel_bss_end(),
            VMM_WRITABLE
        ) != 0)
    {
        vmm_destroy_address_space(pml4_physical);
        return 0;
    }

    return pml4_physical;
}

/*
 * Get the lifecycle state of a registered address space.
 */
int vmm_get_address_space_state(
    uint64_t pml4_physical,
    uint8_t *state
)
{
    if (pml4_physical == 0 ||
        state == NULL)
    {
        return -1;
    }

    struct vmm_address_space_record *record =
        find_address_space_record(
            pml4_physical
        );

    if (record == NULL)
        return -1;

    *state = record->state;

    return 0;
}

/*
 * Activate a registered address space.
 *
 * State changes happen only after the CPU has
 * successfully loaded the requested CR3 value.
 */
int vmm_activate_address_space(
    uint64_t pml4_physical
)
{
    if (pml4_physical == 0)
        return -1;

    struct vmm_address_space_record *target =
        find_address_space_record(
            pml4_physical
        );

    if (target == NULL)
        return -1;

    /*
     * The root must be a VMM-owned PML4.
     */
    if (vmm_verify_page_table_root(
            pml4_physical
        ) != 0)
    {
        return -1;
    }

    /*
     * Idempotent activation.
     */
    if (active_address_space_pml4 ==
            pml4_physical &&
        target->state ==
            VMM_ADDRESS_SPACE_ACTIVE)
    {
        return 0;
    }

    /*
     * Interrupt-state management belongs to the caller.
     *
     * The current BATOS bootstrap path disables interrupts
     * before calling this function.
     */
    vmm_write_cr3(
        pml4_physical
    );

    uint64_t activated_cr3 =
        vmm_read_cr3();

    uint64_t activated_pml4 =
        activated_cr3 &
        ADDRESS_MASK;

    /*
     * Do not modify lifecycle metadata unless
     * the hardware transition succeeded.
     */
    if (activated_pml4 !=
        pml4_physical)
    {
        return -1;
    }

    /*
     * The previous active address space becomes
     * inactive only after the new CR3 is confirmed.
     */
    if (active_address_space_pml4 != 0 &&
        active_address_space_pml4 !=
            pml4_physical)
    {
        struct vmm_address_space_record *previous =
            find_address_space_record(
                active_address_space_pml4
            );

        if (previous != NULL)
        {
            previous->state =
                VMM_ADDRESS_SPACE_INACTIVE;
        }
    }

    target->state =
        VMM_ADDRESS_SPACE_ACTIVE;

    active_address_space_pml4 =
        pml4_physical;

    /*
     * Interrupt-state ownership remains with the caller.
     * This function never enables or disables interrupts.
     */

    return 0;
}

/*
 * Verify the lifecycle state of a registered
 * address space.
 */
int vmm_verify_address_space_state(
    uint64_t pml4_physical,
    uint8_t expected_state
)
{
    uint8_t state = 0;

    if (vmm_get_address_space_state(
            pml4_physical,
            &state
        ) != 0)
    {
        return -1;
    }

    if (state != expected_state)
        return -1;

    return 0;
}

/*
 * Map one 4 KiB virtual page to one physical frame.
 *
 * Intermediate page-table creation is transactional:
 * any page tables created by this mapping attempt are
 * released again if a later step fails.
 */
int vmm_map_page(
    uint64_t pml4_physical,
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t flags
)
{
    if (pml4_physical == 0)
        return -1;

    /*
     * Only leaf permission bits are accepted as input.
     * VMM_PRESENT is established internally and huge-page
     * mappings are not supported by this 4 KiB primitive.
     */
    if (flags &
        ~(VMM_WRITABLE | VMM_USER))
    {
        return -1;
    }

    /*
     * The supplied root must be a registered VMM-owned
     * PML4 before it is dereferenced.
     */
    if (vmm_verify_page_table_root(
            pml4_physical
        ) != 0)
    {
        return -1;
    }

    /*
     * Both addresses must be page aligned.
     */
    if (virtual_address & (VMM_PAGE_SIZE - 1))
        return -1;

    if (physical_address & (VMM_PAGE_SIZE - 1))
        return -1;

    uint64_t *pml4 =
        physical_to_virtual(pml4_physical);

    int pdpt_created = 0;
    int pd_created = 0;
    int pt_created = 0;

    /*
     * PML4 → PDPT
     */
    uint64_t pdpt_physical =
        get_or_create_table(
            pml4,
            pml4_index(virtual_address),
            flags,
            VMM_LEVEL_PDPT,
            pml4_physical,
            &pdpt_created
        );

    if (pdpt_physical == 0)
        return -1;

    uint64_t *pdpt =
        physical_to_virtual(pdpt_physical);

    /*
     * PDPT → PD
     */
    uint64_t pd_physical =
        get_or_create_table(
            pdpt,
            pdpt_index(virtual_address),
            flags,
            VMM_LEVEL_PD,
            pml4_physical,
            &pd_created
        );

    if (pd_physical == 0)
    {
        if (pdpt_created)
        {
            rollback_page_table(
                pml4,
                pml4_index(virtual_address),
                pdpt_physical
            );
        }

        return -1;
    }

    uint64_t *pd =
        physical_to_virtual(pd_physical);

    /*
     * PD → PT
     */
    uint64_t pt_physical =
        get_or_create_table(
            pd,
            pd_index(virtual_address),
            flags,
            VMM_LEVEL_PT,
            pml4_physical,
            &pt_created
        );

    if (pt_physical == 0)
    {
        if (pd_created)
        {
            rollback_page_table(
                pdpt,
                pdpt_index(virtual_address),
                pd_physical
            );
        }

        if (pdpt_created)
        {
            rollback_page_table(
                pml4,
                pml4_index(virtual_address),
                pdpt_physical
            );
        }

        return -1;
    }

    uint64_t *pt =
        physical_to_virtual(pt_physical);

    uint64_t index =
        pt_index(virtual_address);

    /*
     * Refuse to silently overwrite an existing mapping.
     *
     * If this PT was newly created, it cannot normally
     * contain a present PTE. Keep the check defensive.
     */
    if (pt[index] & VMM_PRESENT)
    {
        if (pt_created)
        {
            rollback_page_table(
                pd,
                pd_index(virtual_address),
                pt_physical
            );
        }

        if (pd_created)
        {
            rollback_page_table(
                pdpt,
                pdpt_index(virtual_address),
                pd_physical
            );
        }

        if (pdpt_created)
        {
            rollback_page_table(
                pml4,
                pml4_index(virtual_address),
                pdpt_physical
            );
        }

        return -1;
    }

    /*
     * Create final PTE.
     */
    pt[index] =
        (physical_address & ADDRESS_MASK) |
        VMM_PRESENT |
        (flags & (VMM_WRITABLE | VMM_USER));

    return 0;
}

/*
 * Unmap one 4 KiB virtual page.
 *
 * This removes only the final PTE mapping.
 *
 * Intermediate page tables are intentionally retained.
 * Reclaiming empty page-table pages is a separate lifecycle
 * operation and is not part of this primitive.
 *
 * The physical frame is returned to the caller when requested,
 * but is never freed here because PMM owns physical-frame
 * lifecycle.
 */
int vmm_unmap_page(
    uint64_t pml4_physical,
    uint64_t virtual_address,
    uint64_t *physical_address
)
{
    if (pml4_physical == 0)
        return -1;

    if (vmm_verify_page_table_root(
            pml4_physical
        ) != 0)
    {
        return -1;
    }

    if (virtual_address &
        (VMM_PAGE_SIZE - 1))
        return -1;

    uint64_t *pml4 =
        physical_to_virtual(
            pml4_physical
        );

    uint64_t pml4_entry =
        pml4[pml4_index(virtual_address)];

    uint64_t pdpt_physical =
        verify_existing_child_table(
            pml4_entry,
            VMM_LEVEL_PDPT,
            pml4_physical
        );

    if (pdpt_physical == 0)
        return -1;

    uint64_t *pdpt =
        physical_to_virtual(
            pdpt_physical
        );

    uint64_t pdpt_entry =
        pdpt[pdpt_index(virtual_address)];

    uint64_t pd_physical =
        verify_existing_child_table(
            pdpt_entry,
            VMM_LEVEL_PD,
            pml4_physical
        );

    if (pd_physical == 0)
        return -1;

    uint64_t *pd =
        physical_to_virtual(
            pd_physical
        );

    uint64_t pd_entry =
        pd[pd_index(virtual_address)];

    uint64_t pt_physical =
        verify_existing_child_table(
            pd_entry,
            VMM_LEVEL_PT,
            pml4_physical
        );

    if (pt_physical == 0)
        return -1;

    uint64_t *pt =
        physical_to_virtual(
            pt_physical
        );

    uint64_t index =
        pt_index(virtual_address);

    uint64_t pte =
        pt[index];

    if (!(pte & VMM_PRESENT))
        return -1;

    uint64_t mapped_physical =
        pte & ADDRESS_MASK;

    /*
     * Clear the leaf mapping.
     */
    pt[index] = 0;

    /*
     * If this address space is currently active,
     * invalidate the CPU's cached translation.
     */
    if (active_address_space_pml4 ==
        pml4_physical)
    {
        __asm__ volatile (
            "invlpg (%0)"
            :
            : "r"(virtual_address)
            : "memory"
        );
    }

    if (physical_address != NULL)
        *physical_address = mapped_physical;

    return 0;
}

/*
 * Software page-table walker.
 *
 * Walks:
 *
 *     PML4 → PDPT → PD → PT → PTE
 *
 * and translates the supplied virtual address
 * into its corresponding physical address.
 *
 * IMPORTANT:
 *
 * This does NOT modify CR3.
 */
int vmm_translate(
    uint64_t pml4_physical,
    uint64_t virtual_address,
    uint64_t *physical_address
)
{
    if (pml4_physical == 0)
        return -1;

    if (physical_address == NULL)
        return -1;

    if (vmm_verify_page_table_root(
            pml4_physical
        ) != 0)
    {
        return -1;
    }

    /*
     * PML4
     */
    uint64_t *pml4 =
        physical_to_virtual(pml4_physical);

    uint64_t pml4_entry =
        pml4[pml4_index(virtual_address)];

    if (!(pml4_entry & VMM_PRESENT))
        return -1;

    if (pml4_entry & VMM_HUGE)
        return -1;

    /*
     * PDPT
     */
    uint64_t pdpt_physical =
        verify_existing_child_table(
            pml4_entry,
            VMM_LEVEL_PDPT,
            pml4_physical
        );

    if (pdpt_physical == 0)
        return -1;

    uint64_t *pdpt =
        physical_to_virtual(pdpt_physical);

    uint64_t pdpt_entry =
        pdpt[pdpt_index(virtual_address)];

    if (!(pdpt_entry & VMM_PRESENT))
        return -1;

    /*
     * A huge PDPTE is a 1 GiB leaf.
     */
    if (pdpt_entry & VMM_HUGE)
    {
        uint64_t physical_base =
            pdpt_entry & ADDRESS_MASK;

        uint64_t page_offset =
            virtual_address & ((1ULL << 30) - 1);

        *physical_address =
            physical_base | page_offset;

        return 0;
    }

    uint64_t pd_physical =
        verify_existing_child_table(
            pdpt_entry,
            VMM_LEVEL_PD,
            pml4_physical
        );

    if (pd_physical == 0)
        return -1;

    uint64_t *pd =
        physical_to_virtual(pd_physical);

    uint64_t pd_entry =
        pd[pd_index(virtual_address)];

    if (!(pd_entry & VMM_PRESENT))
        return -1;

    /*
     * A huge PDE is a 2 MiB leaf.
     */
    if (pd_entry & VMM_HUGE)
    {
        uint64_t physical_base =
            pd_entry & ADDRESS_MASK;

        uint64_t page_offset =
            virtual_address & ((1ULL << 21) - 1);

        *physical_address =
            physical_base | page_offset;

        return 0;
    }

    uint64_t pt_physical =
        verify_existing_child_table(
            pd_entry,
            VMM_LEVEL_PT,
            pml4_physical
        );

    if (pt_physical == 0)
        return -1;

    /*
     * PT
     */
    uint64_t *pt =
        physical_to_virtual(pt_physical);

    uint64_t pte =
        pt[pt_index(virtual_address)];

    if (!(pte & VMM_PRESENT))
        return -1;

    uint64_t physical_base =
        pte & ADDRESS_MASK;

    uint64_t page_offset =
        virtual_address & (VMM_PAGE_SIZE - 1);

    *physical_address =
        physical_base | page_offset;

    return 0;
}


/*
 * Inspect one mapped 4 KiB page.
 *
 * This performs the same software page-table walk as
 * vmm_translate(), but additionally returns the final
 * PTE's relevant permission/presence flags.
 *
 * IMPORTANT:
 *
 * The inspection is read-only. It never modifies page
 * tables, CR3, lifecycle state, or mappings.
 */
int vmm_inspect_mapping(
    uint64_t pml4_physical,
    uint64_t virtual_address,
    uint64_t *physical_address,
    uint64_t *flags
)
{
    if (pml4_physical == 0 ||
        physical_address == NULL ||
        flags == NULL)
    {
        return -1;
    }

    if (vmm_verify_page_table_root(
            pml4_physical
        ) != 0)
    {
        return -1;
    }

    uint64_t *pml4 =
        physical_to_virtual(
            pml4_physical
        );

    uint64_t pml4_entry =
        pml4[pml4_index(virtual_address)];

    if (!(pml4_entry & VMM_PRESENT))
        return -1;

    if (pml4_entry & VMM_HUGE)
        return -1;

    uint64_t pdpt_physical =
        verify_existing_child_table(
            pml4_entry,
            VMM_LEVEL_PDPT,
            pml4_physical
        );

    if (pdpt_physical == 0)
        return -1;

    uint64_t *pdpt =
        physical_to_virtual(
            pdpt_physical
        );

    uint64_t pdpt_entry =
        pdpt[pdpt_index(virtual_address)];

    if (!(pdpt_entry & VMM_PRESENT))
        return -1;

    if (pdpt_entry & VMM_HUGE)
    {
        *physical_address =
            (pdpt_entry & ADDRESS_MASK) |
            (virtual_address & ((1ULL << 30) - 1));

        *flags =
            pdpt_entry &
            (VMM_PRESENT |
             VMM_WRITABLE |
             VMM_USER |
             VMM_HUGE);

        return 0;
    }

    uint64_t pd_physical =
        verify_existing_child_table(
            pdpt_entry,
            VMM_LEVEL_PD,
            pml4_physical
        );

    if (pd_physical == 0)
        return -1;

    uint64_t *pd =
        physical_to_virtual(
            pd_physical
        );

    uint64_t pd_entry =
        pd[pd_index(virtual_address)];

    if (!(pd_entry & VMM_PRESENT))
        return -1;

    if (pd_entry & VMM_HUGE)
    {
        *physical_address =
            (pd_entry & ADDRESS_MASK) |
            (virtual_address & ((1ULL << 21) - 1));

        *flags =
            pd_entry &
            (VMM_PRESENT |
             VMM_WRITABLE |
             VMM_USER |
             VMM_HUGE);

        return 0;
    }

    uint64_t pt_physical =
        verify_existing_child_table(
            pd_entry,
            VMM_LEVEL_PT,
            pml4_physical
        );

    if (pt_physical == 0)
        return -1;

    uint64_t *pt =
        physical_to_virtual(
            pt_physical
        );

    uint64_t pte =
        pt[pt_index(virtual_address)];

    if (!(pte & VMM_PRESENT))
        return -1;

    *physical_address =
        pte & ADDRESS_MASK;

    *flags =
        pte &
        (VMM_PRESENT |
         VMM_WRITABLE |
         VMM_USER);

    return 0;
}

/*
 * Return BATOS kernel PML4 physical address.
 */
uint64_t vmm_get_pml4(void)
{
    return kernel_pml4;
}

/*
 * Return the standalone root produced by the
 * most recent recursive clone.
 */
uint64_t vmm_get_last_cloned_pml4(void)
{
    return last_cloned_pml4;
}

/*
 * Read the current CPU CR3 register.
 */
uint64_t vmm_read_cr3(void)
{
    uint64_t cr3;

    __asm__ volatile (
        "mov %%cr3, %0"
        : "=r"(cr3)
    );

    return cr3;
}

/*
 * Write a physical PML4 address into CR3.
 */
void vmm_write_cr3(uint64_t pml4_physical)
{
    __asm__ volatile (
        "mov %0, %%cr3"
        :
        : "r"(pml4_physical)
        : "memory"
    );
}

/*
 * Clone one page-table level recursively.
 *
 * level:
 *     3 = PML4
 *     2 = PDPT
 *     1 = PD
 *     0 = PT
 *
 * For levels above PT, present entries point to
 * another page-table page and are recursively cloned.
 *
 * At PT level, entries point directly to physical
 * data frames and are copied as-is.
 *
 * Returns:
 *     physical address of cloned table
 *     0 on failure
 */
/*
 * Discard a page-table hierarchy that is known to have been
 * allocated by one transactional operation.
 *
 * Unlike destroy_address_space_tree(), this helper does not
 * require a complete pre-validation pass. It is used only for
 * rollback of a partially constructed hierarchy.
 *
 * Every reachable child page-table in this tree was allocated
 * by the current operation.
 */
static void discard_page_table_tree(
    uint64_t physical,
    int level
)
{
    if (physical == 0)
        return;

    uint64_t *table =
        physical_to_virtual(
            physical
        );

    if (level != VMM_LEVEL_PT)
    {
        for (uint64_t index = 0;
             index < PAGE_TABLE_ENTRIES;
             index++)
        {
            uint64_t entry =
                table[index];

            if (!(entry & VMM_PRESENT))
                continue;

            /*
             * Huge mappings terminate the hierarchy and do not
             * reference another page-table frame.
             */
            if (entry & VMM_HUGE)
            {
                table[index] = 0;
                continue;
            }

            uint64_t child =
                entry & ADDRESS_MASK;

            table[index] = 0;

            discard_page_table_tree(
                child,
                level - 1
            );
        }
    }

    release_page_table(
        physical
    );
}

/*
 * Recursively clone one page-table hierarchy.
 *
 * Page-table frames are fully independent from the source.
 * Leaf physical frames are intentionally shared by copying
 * the original PTEs.
 *
 * Failure is transactional: the complete partial hierarchy
 * created by this invocation is discarded before returning 0.
 */
static uint64_t clone_page_table(
    uint64_t source_physical,
    int level,
    uint64_t owner_pml4
)
{
    if (source_physical == 0)
        return 0;

    if (level < VMM_LEVEL_PT ||
        level > VMM_LEVEL_PML4)
        return 0;

    uint64_t destination_physical =
        allocate_page_table(
            level,
            owner_pml4
        );

    if (destination_physical == 0)
        return 0;

    uint64_t *source =
        physical_to_virtual(
            source_physical
        );

    uint64_t *destination =
        physical_to_virtual(
            destination_physical
        );

    /*
     * The newly allocated PML4 becomes the root owner
     * of the standalone cloned address space.
     */
    uint64_t clone_owner_pml4 =
        owner_pml4;

    if (level == VMM_LEVEL_PML4)
        clone_owner_pml4 =
            destination_physical;

    struct vmm_page_table_record *record =
        find_page_table_record(
            destination_physical
        );

    if (record == NULL)
    {
        discard_page_table_tree(
            destination_physical,
            level
        );

        return 0;
    }

    record->owner_pml4 =
        clone_owner_pml4;

    for (uint64_t i = 0;
         i < PAGE_TABLE_ENTRIES;
         i++)
    {
        uint64_t entry =
            source[i];

        if (!(entry & VMM_PRESENT))
            continue;

        /*
         * PT entries point directly to physical data frames.
         */
        if (level == VMM_LEVEL_PT)
        {
            destination[i] = entry;
            continue;
        }

        /*
         * Huge pages terminate the hierarchy.
         * Preserve them exactly in the standalone clone.
         */
        if (entry & VMM_HUGE)
        {
            destination[i] = entry;
            continue;
        }

        uint64_t source_child =
            entry & ADDRESS_MASK;

        uint64_t destination_child =
            clone_page_table(
                source_child,
                level - 1,
                clone_owner_pml4
            );

        if (destination_child == 0)
        {
            discard_page_table_tree(
                destination_physical,
                level
            );

            return 0;
        }

        /*
         * Preserve all entry flags while replacing only
         * the child physical address.
         */
        destination[i] =
            destination_child |
            (entry & ~ADDRESS_MASK);
    }

    return destination_physical;
}

/*
 * Recursively copy one source page-table subtree into a newly
 * allocated hierarchy owned by an existing PML4.
 *
 * This is intentionally separate from clone_page_table():
 *
 *     clone_page_table()
 *         -> creates a standalone address-space root
 *
 *     copy_page_table_subtree()
 *         -> creates a subtree owned by kernel_pml4
 *
 * Page-table frames are never shared. Leaf physical frames are
 * shared by copying the original leaf entries.
 *
 * The returned subtree remains unpublished until the caller
 * explicitly installs it into its parent.
 */
static uint64_t copy_page_table_subtree(
    uint64_t source_physical,
    int level,
    uint64_t owner_pml4
)
{
    if (source_physical == 0 ||
        owner_pml4 == 0)
        return 0;

    if (level < VMM_LEVEL_PT ||
        level > VMM_LEVEL_PDPT)
        return 0;

    uint64_t destination_physical =
        allocate_page_table(
            level,
            owner_pml4
        );

    if (destination_physical == 0)
        return 0;

    uint64_t *source =
        physical_to_virtual(
            source_physical
        );

    uint64_t *destination =
        physical_to_virtual(
            destination_physical
        );

    for (uint64_t i = 0;
         i < PAGE_TABLE_ENTRIES;
         i++)
    {
        uint64_t entry =
            source[i];

        if (!(entry & VMM_PRESENT))
            continue;

        /*
         * Huge mappings at PDPT/PD levels terminate the
         * hierarchy. Preserve them exactly as mapping leaves.
         *
         * This helper is never called at PML4 level.
         */
        if (level != VMM_LEVEL_PT &&
            (entry & VMM_HUGE))
        {
            destination[i] = entry;
            continue;
        }

        /*
         * At PT level, entries directly reference physical
         * data frames and can be copied unchanged.
         */
        if (level == VMM_LEVEL_PT)
        {
            destination[i] = entry;
            continue;
        }

        uint64_t source_child =
            entry & ADDRESS_MASK;

        uint64_t destination_child =
            copy_page_table_subtree(
                source_child,
                level - 1,
                owner_pml4
            );

        if (destination_child == 0)
        {
            discard_page_table_tree(
                destination_physical,
                level
            );

            return 0;
        }

        destination[i] =
            destination_child |
            (entry & ~ADDRESS_MASK);
    }

    return destination_physical;
}

/*
 * Prepare BATOS's address space.
 *
 * The active Limine hierarchy is used to produce two distinct
 * results:
 *
 *     1. A standalone recursive clone retained for VMM-3.1.
 *     2. An independently allocated BATOS-owned hierarchy.
 *
 * Page-table frames are never shared between these trees.
 * Leaf physical mappings remain shared.
 *
 * Preparation is transactional:
 *
 *     - no BATOS PML4 slot is published until every required
 *       subtree has been constructed successfully;
 *     - any failure discards every temporary BATOS subtree;
 *     - the standalone clone is also discarded on failure;
 *     - last_cloned_pml4 is updated only after success.
 *
 * Existing BATOS mappings always have priority.
 */
int vmm_prepare_address_space(void)
{
    if (kernel_pml4 == 0)
        return -1;

    /*
     * Read the currently active address space.
     */
    uint64_t current_cr3 =
        vmm_read_cr3();

    uint64_t current_pml4_physical =
        current_cr3 & ADDRESS_MASK;

    if (current_pml4_physical == 0)
        return -1;

    /*
     * Already active.
     */
    if (current_pml4_physical == kernel_pml4)
        return 0;

    uint64_t *batos_pml4 =
        physical_to_virtual(
            kernel_pml4
        );

    uint64_t *source_pml4 =
        physical_to_virtual(
            current_pml4_physical
        );

    /*
     * Create the standalone verification clone first.
     *
     * Do not publish it through last_cloned_pml4 yet.
     */
    uint64_t cloned_pml4 =
        clone_page_table(
            current_pml4_physical,
            VMM_LEVEL_PML4,
            0
        );

    if (cloned_pml4 == 0)
        return -1;

    /*
     * Hold newly-created BATOS PML4 children here until the
     * complete preparation transaction has succeeded.
     */
    uint64_t new_batos_roots[PAGE_TABLE_ENTRIES] = {0};

    /*
     * Build every missing BATOS PML4 subtree without modifying
     * the BATOS root itself.
     */
    for (uint64_t i = 0;
         i < PAGE_TABLE_ENTRIES;
         i++)
    {
        /*
         * Existing BATOS mappings have priority.
         */
        if (batos_pml4[i] & VMM_PRESENT)
            continue;

        uint64_t source_entry =
            source_pml4[i];

        if (!(source_entry & VMM_PRESENT))
            continue;

        /*
         * The current lifecycle contract supports ordinary
         * four-level page-table traversal, not a huge PML4 leaf.
         */
        if (source_entry & VMM_HUGE)
        {
            for (uint64_t rollback_index = 0;
                 rollback_index < PAGE_TABLE_ENTRIES;
                 rollback_index++)
            {
                if (new_batos_roots[rollback_index] == 0)
                    continue;

                discard_page_table_tree(
                    new_batos_roots[rollback_index],
                    VMM_LEVEL_PDPT
                );
            }

            discard_page_table_tree(
                cloned_pml4,
                VMM_LEVEL_PML4
            );

            return -1;
        }

        uint64_t source_child =
            source_entry & ADDRESS_MASK;

        uint64_t batos_child =
            copy_page_table_subtree(
                source_child,
                VMM_LEVEL_PDPT,
                kernel_pml4
            );

        if (batos_child == 0)
        {
            /*
             * No BATOS root entry has been published yet.
             * Discard every temporary subtree constructed so far.
             */
            for (uint64_t rollback_index = 0;
                 rollback_index < PAGE_TABLE_ENTRIES;
                 rollback_index++)
            {
                if (new_batos_roots[rollback_index] == 0)
                    continue;

                discard_page_table_tree(
                    new_batos_roots[rollback_index],
                    VMM_LEVEL_PDPT
                );
            }

            discard_page_table_tree(
                cloned_pml4,
                VMM_LEVEL_PML4
            );

            return -1;
        }

        new_batos_roots[i] =
            batos_child;
    }

    /*
     * All required subtrees now exist and are registered as
     * owned by kernel_pml4. Publish them atomically at the
     * PML4-entry level.
     */
    for (uint64_t i = 0;
         i < PAGE_TABLE_ENTRIES;
         i++)
    {
        if (new_batos_roots[i] == 0)
            continue;

        uint64_t source_entry =
            source_pml4[i];

        batos_pml4[i] =
            new_batos_roots[i] |
            (source_entry & ~ADDRESS_MASK);
    }

    /*
     * Only after the entire BATOS hierarchy has been prepared
     * successfully is the standalone clone made externally
     * visible for VMM-3.1 verification.
     */
    last_cloned_pml4 =
        cloned_pml4;

    return 0;
}

/*
 * Inspect one PML4.
 *
 * This function is read-only.
 *
 * It does not modify:
 *     - CR3
 *     - page tables
 *     - mappings
 *     - physical memory state
 */
int vmm_inspect_address_space(
    uint64_t pml4_physical,
    uint64_t *present_entries
)
{
    if (pml4_physical == 0)
        return -1;

    if (present_entries == NULL)
        return -1;

    uint64_t *pml4 =
        physical_to_virtual(
            pml4_physical
        );

    uint64_t count = 0;

    for (uint64_t i = 0;
         i < PAGE_TABLE_ENTRIES;
         i++)
    {
        if (pml4[i] & VMM_PRESENT)
            count++;
    }

    *present_entries = count;

    return 0;
}

/*
 * Verify one known virtual-address path through
 * a recursively cloned hierarchy.
 *
 * This verifies:
 *
 *     source PML4  != cloned PML4
 *     source PDPT  != cloned PDPT
 *     source PD    != cloned PD
 *     source PT    != cloned PT
 *
 * while:
 *
 *     source PTE physical frame == cloned PTE frame
 *     source PTE flags          == cloned PTE flags
 */
int vmm_verify_recursive_clone(
    uint64_t source_pml4_physical,
    uint64_t cloned_pml4_physical,
    uint64_t virtual_address
)
{
    if (source_pml4_physical == 0 ||
        cloned_pml4_physical == 0)
        return -1;

    if (source_pml4_physical ==
        cloned_pml4_physical)
        return -1;

    /*
     * PML4
     */
    uint64_t *source_pml4 =
        physical_to_virtual(
            source_pml4_physical
        );

    uint64_t *cloned_pml4 =
        physical_to_virtual(
            cloned_pml4_physical
        );

    uint64_t index =
        pml4_index(
            virtual_address
        );

    uint64_t source_pml4_entry =
        source_pml4[index];

    uint64_t cloned_pml4_entry =
        cloned_pml4[index];

    if (!(source_pml4_entry & VMM_PRESENT) ||
        !(cloned_pml4_entry & VMM_PRESENT))
        return -1;

    /*
     * PML4-level huge mappings are outside the current
     * four-level BATOS hierarchy contract.
     */
    if (source_pml4_entry & VMM_HUGE)
        return -1;

    if (cloned_pml4_entry & VMM_HUGE)
        return -1;

    /*
     * PML4 -> PDPT
     */
    uint64_t source_pdpt_physical =
        source_pml4_entry & ADDRESS_MASK;

    uint64_t cloned_pdpt_physical =
        cloned_pml4_entry & ADDRESS_MASK;

    if (source_pdpt_physical == 0 ||
        cloned_pdpt_physical == 0)
        return -1;

    if (source_pdpt_physical ==
        cloned_pdpt_physical)
        return -1;

    /*
     * PDPT
     */
    uint64_t *source_pdpt =
        physical_to_virtual(
            source_pdpt_physical
        );

    uint64_t *cloned_pdpt =
        physical_to_virtual(
            cloned_pdpt_physical
        );

    index =
        pdpt_index(
            virtual_address
        );

    uint64_t source_pdpt_entry =
        source_pdpt[index];

    uint64_t cloned_pdpt_entry =
        cloned_pdpt[index];

    if (!(source_pdpt_entry & VMM_PRESENT) ||
        !(cloned_pdpt_entry & VMM_PRESENT))
        return -1;

    /*
     * A huge PDPTE is a terminal 1 GiB mapping.
     * Both clone entries must therefore be identical.
     */
    if (source_pdpt_entry & VMM_HUGE)
    {
        if (!(cloned_pdpt_entry & VMM_HUGE))
            return -1;

        if (cloned_pdpt_entry != source_pdpt_entry)
            return -1;

        return 0;
    }

    /*
     * A huge destination where the source has a normal
     * table entry is structurally invalid.
     */
    if (cloned_pdpt_entry & VMM_HUGE)
        return -1;

    /*
     * PDPT -> PD
     */
    uint64_t source_pd_physical =
        source_pdpt_entry & ADDRESS_MASK;

    uint64_t cloned_pd_physical =
        cloned_pdpt_entry & ADDRESS_MASK;

    if (source_pd_physical == 0 ||
        cloned_pd_physical == 0)
        return -1;

    if (source_pd_physical ==
        cloned_pd_physical)
        return -1;

    /*
     * PD
     */
    uint64_t *source_pd =
        physical_to_virtual(
            source_pd_physical
        );

    uint64_t *cloned_pd =
        physical_to_virtual(
            cloned_pd_physical
        );

    index =
        pd_index(
            virtual_address
        );

    uint64_t source_pd_entry =
        source_pd[index];

    uint64_t cloned_pd_entry =
        cloned_pd[index];

    if (!(source_pd_entry & VMM_PRESENT) ||
        !(cloned_pd_entry & VMM_PRESENT))
        return -1;

    /*
     * A huge PDE is a terminal 2 MiB mapping.
     * Both clone entries must therefore be identical.
     */
    if (source_pd_entry & VMM_HUGE)
    {
        if (!(cloned_pd_entry & VMM_HUGE))
            return -1;

        if (cloned_pd_entry != source_pd_entry)
            return -1;

        return 0;
    }

    /*
     * A huge destination where the source has a normal
     * PT child is structurally invalid.
     */
    if (cloned_pd_entry & VMM_HUGE)
        return -1;

    /*
     * PD -> PT
     */
    uint64_t source_pt_physical =
        source_pd_entry & ADDRESS_MASK;

    uint64_t cloned_pt_physical =
        cloned_pd_entry & ADDRESS_MASK;

    if (source_pt_physical == 0 ||
        cloned_pt_physical == 0)
        return -1;

    if (source_pt_physical ==
        cloned_pt_physical)
        return -1;

    /*
     * PT
     */
    uint64_t *source_pt =
        physical_to_virtual(
            source_pt_physical
        );

    uint64_t *cloned_pt =
        physical_to_virtual(
            cloned_pt_physical
        );

    index =
        pt_index(
            virtual_address
        );

    uint64_t source_pte =
        source_pt[index];

    uint64_t cloned_pte =
        cloned_pt[index];

    if (!(source_pte & VMM_PRESENT) ||
        !(cloned_pte & VMM_PRESENT))
        return -1;

    /*
     * The mapped physical frame must be preserved.
     */
    if ((source_pte & ADDRESS_MASK) !=
        (cloned_pte & ADDRESS_MASK))
        return -1;

    /*
     * The complete PTE flags must be preserved.
     */
    if ((source_pte & ~ADDRESS_MASK) !=
        (cloned_pte & ~ADDRESS_MASK))
        return -1;

    return 0;
}

/*
 * Recursively verify the complete page-table hierarchy.
 *
 * Every present source mapping must be represented
 * identically in the destination hierarchy.
 *
 * Empty source entries must remain empty.
 *
 * Page-table pages must be independent.
 *
 * Huge-page entries are preserved exactly.
 */

static int verify_clone_level(
    uint64_t source_physical,
    uint64_t destination_physical,
    int level
)
{
    if (source_physical == 0 ||
        destination_physical == 0)
        return -1000000;

    if (level < 0 || level > 3)
        return -1000001;

    if (source_physical == destination_physical)
        return -1000002;

    uint64_t *source =
        physical_to_virtual(source_physical);

    uint64_t *destination =
        physical_to_virtual(destination_physical);

    for (uint64_t i = 0;
         i < PAGE_TABLE_ENTRIES;
         i++)
    {
        uint64_t source_entry = source[i];
        uint64_t destination_entry = destination[i];

        if (!(source_entry & VMM_PRESENT))
        {
            if (destination_entry & VMM_PRESENT)
            {
                return -(
                    1000000 +
                    ((level + 1) * 10000) +
                    (int)i
                );
            }

            continue;
        }

        if (source_entry & VMM_HUGE)
        {
            if (destination_entry != source_entry)
            {
                return -(
                    2000000 +
                    ((level + 1) * 10000) +
                    (int)i
                );
            }

            continue;
        }

        if (!(destination_entry & VMM_PRESENT))
        {
            return -(
                3000000 +
                ((level + 1) * 10000) +
                (int)i
            );
        }

        if (level == 0)
        {
            uint64_t source_compare =
                source_entry &
                ~VMM_HARDWARE_MANAGED_BITS;

            uint64_t destination_compare =
                destination_entry &
                ~VMM_HARDWARE_MANAGED_BITS;

            if (destination_compare != source_compare)
            {
                return -(
                    4000000 +
                    ((level + 1) * 10000) +
                    (int)i
                );
            }

            continue;
        }

        uint64_t source_child =
            source_entry & ADDRESS_MASK;

        uint64_t destination_child =
            destination_entry & ADDRESS_MASK;

        if (source_child == 0 ||
            destination_child == 0)
        {
            return -(
                5000000 +
                ((level + 1) * 10000) +
                (int)i
            );
        }

        if (source_child == destination_child)
        {
            return -(
                6000000 +
                ((level + 1) * 10000) +
                (int)i
            );
        }

        int child_result =
            verify_clone_level(
                source_child,
                destination_child,
                level - 1
            );

        if (child_result != 0)
            return child_result;
    }

    return 0;
}

/*
 * VMM-3.1:
 *
 * Verify the complete recursive clone.
 */
int vmm_verify_clone(
    uint64_t source_pml4_physical,
    uint64_t cloned_pml4_physical
)
{
    if (source_pml4_physical == 0 ||
        cloned_pml4_physical == 0)
        return -1;

    if (source_pml4_physical ==
        cloned_pml4_physical)
        return -1;

    return verify_clone_level(
        source_pml4_physical,
        cloned_pml4_physical,
        3
    );
}
