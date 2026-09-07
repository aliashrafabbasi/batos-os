#include "vmm.h"
#include "pmm.h"

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
    uint8_t level;
    uint8_t in_use;
};

static struct vmm_page_table_record
    page_table_records[VMM_MAX_PAGE_TABLES];

static uint64_t page_table_record_count = 0;

static uint64_t kernel_pml4 = 0;

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

    if (page_table_record_count >= VMM_MAX_PAGE_TABLES)
        return -1;

    struct vmm_page_table_record *record =
        &page_table_records[page_table_record_count];

    record->physical_address = physical_address;
    record->owner_pml4 = owner_pml4;
    record->level = (uint8_t)level;
    record->in_use = 1;

    page_table_record_count++;

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
 */
static uint64_t get_or_create_table(
    uint64_t *parent,
    uint64_t index,
    uint64_t flags,
    int child_level,
    uint64_t owner_pml4
)
{
    uint64_t entry =
        parent[index];

    if (entry & VMM_PRESENT)
    {
        return entry & ADDRESS_MASK;
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

    return child;
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

    if (pdpt_entry & VMM_HUGE)
        return -1;

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

    if (pd_entry & VMM_HUGE)
        return -1;

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

    if (address_space_record_count >=
        VMM_MAX_ADDRESS_SPACES)
    {
        return -1;
    }

    if (find_address_space_record(
            pml4_physical) != NULL)
    {
        return -1;
    }

    struct vmm_address_space_record *record =
        &address_space_records[
            address_space_record_count
        ];

    record->pml4_physical =
        pml4_physical;

    record->state =
        VMM_ADDRESS_SPACE_CREATED;

    record->in_use = 1;

    address_space_record_count++;

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
        struct vmm_page_table_record *record =
            find_page_table_record(
                pml4_physical
            );

        if (record != NULL)
            record->in_use = 0;

        pmm_free_frame(
            pml4_physical
        );

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

    __asm__ volatile ("sti");

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
     * Both addresses must be page aligned.
     */
    if (virtual_address & (VMM_PAGE_SIZE - 1))
        return -1;

    if (physical_address & (VMM_PAGE_SIZE - 1))
        return -1;

    uint64_t *pml4 =
        physical_to_virtual(pml4_physical);

    /*
     * PML4 → PDPT
     */
    uint64_t pdpt_physical =
        get_or_create_table(
            pml4,
            pml4_index(virtual_address),
            flags,
            VMM_LEVEL_PDPT,
            pml4_physical
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
            pml4_physical
        );

    if (pd_physical == 0)
        return -1;

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
            pml4_physical
        );

    if (pt_physical == 0)
        return -1;

    uint64_t *pt =
        physical_to_virtual(pt_physical);

    uint64_t index =
        pt_index(virtual_address);

    /*
     * Refuse to silently overwrite an existing mapping.
     */
    if (pt[index] & VMM_PRESENT)
        return -1;

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

    /*
     * PML4
     */
    uint64_t *pml4 =
        physical_to_virtual(pml4_physical);

    uint64_t pml4_entry =
        pml4[pml4_index(virtual_address)];

    if (!(pml4_entry & VMM_PRESENT))
        return -1;

    /*
     * PDPT
     */
    uint64_t pdpt_physical =
        pml4_entry & ADDRESS_MASK;

    uint64_t *pdpt =
        physical_to_virtual(pdpt_physical);

    uint64_t pdpt_entry =
        pdpt[pdpt_index(virtual_address)];

    if (!(pdpt_entry & VMM_PRESENT))
        return -1;

    /*
     * 1 GiB huge pages are not implemented yet.
     */
    if (pdpt_entry & VMM_HUGE)
        return -1;

    /*
     * PD
     */
    uint64_t pd_physical =
        pdpt_entry & ADDRESS_MASK;

    uint64_t *pd =
        physical_to_virtual(pd_physical);

    uint64_t pd_entry =
        pd[pd_index(virtual_address)];

    if (!(pd_entry & VMM_PRESENT))
        return -1;

    /*
     * 2 MiB huge pages are not implemented yet.
     */
    if (pd_entry & VMM_HUGE)
        return -1;

    /*
     * PT
     */
    uint64_t pt_physical =
        pd_entry & ADDRESS_MASK;

    uint64_t *pt =
        physical_to_virtual(pt_physical);

    uint64_t pte =
        pt[pt_index(virtual_address)];

    if (!(pte & VMM_PRESENT))
        return -1;

    /*
     * Physical page base + virtual page offset.
     */
    uint64_t physical_base =
        pte & ADDRESS_MASK;

    uint64_t page_offset =
        virtual_address & (VMM_PAGE_SIZE - 1);

    *physical_address =
        physical_base | page_offset;

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
static uint64_t clone_page_table(
    uint64_t source_physical,
    int level,
    uint64_t owner_pml4
)
{
    if (source_physical == 0)
        return 0;

    if (level < 0 || level > 3)
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
     * The newly allocated level-3 table becomes the
     * root owner of this cloned address space.
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
        return 0;

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
         * PT entries point directly to physical
         * frames. Copy them without recursion.
         */
        if (level == 0)
        {
            destination[i] = entry;
            continue;
        }

        /*
         * Huge pages terminate the hierarchy.
         *
         * Preserve them exactly.
         */
        if (entry & VMM_HUGE)
        {
            destination[i] = entry;
            continue;
        }

        /*
         * Recursively clone the child table.
         */
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
            /*
             * Partial rollback is intentionally deferred
             * until formal VMM ownership management exists.
             */
            return 0;
        }

        /*
         * Preserve all original entry flags while
         * replacing only the physical child address.
         */
        destination[i] =
            destination_child |
            (entry & ~ADDRESS_MASK);
    }

    return destination_physical;
}

/*
 * Prepare BATOS's address space.
 *
 * Current Limine hierarchy
 *          ↓
 *   recursive deep clone
 *          ↓
 * standalone cloned PML4
 *          ↓
 * merge into BATOS-owned PML4
 *
 * The standalone cloned root is retained so that the
 * clone can be structurally verified before activation.
 */
int vmm_prepare_address_space(void)
{
    if (kernel_pml4 == 0)
        return -1;

    /*
     * Read currently active address space.
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

    /*
     * BATOS already has its own PML4.
     */
    uint64_t *batos_pml4 =
        physical_to_virtual(
            kernel_pml4
        );

    /*
     * Recursively clone the complete active hierarchy.
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
     * Retain the standalone clone root for structural
     * verification.
     */
    last_cloned_pml4 =
        cloned_pml4;

    uint64_t *cloned_pml4_table =
        physical_to_virtual(
            cloned_pml4
        );

    /*
     * Merge the cloned hierarchy into BATOS's PML4.
     *
     * Existing BATOS mappings have priority.
     */
    for (uint64_t i = 0;
         i < PAGE_TABLE_ENTRIES;
         i++)
    {
        if (batos_pml4[i] & VMM_PRESENT)
            continue;

        if (!(cloned_pml4_table[i] & VMM_PRESENT))
            continue;

        batos_pml4[i] =
            cloned_pml4_table[i];
    }

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

    if (!(source_pml4_entry & VMM_PRESENT))
        return -1;

    if (!(cloned_pml4_entry & VMM_PRESENT))
        return -1;

    /*
     * Huge page at PML4 is not valid for our
     * current 4-level hierarchy.
     */
    if (source_pml4_entry & VMM_HUGE)
        return -1;

    if (cloned_pml4_entry & VMM_HUGE)
        return -1;

    /*
     * PML4 → PDPT
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

    if (source_pdpt_entry & VMM_HUGE)
        return -1;

    if (cloned_pdpt_entry & VMM_HUGE)
        return -1;

    /*
     * PDPT → PD
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

    if (source_pd_entry & VMM_HUGE)
        return -1;

    if (cloned_pd_entry & VMM_HUGE)
        return -1;

    /*
     * PD → PT
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
