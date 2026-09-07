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

static uint64_t kernel_pml4 = 0;

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
 * Allocate and initialize one page-table page.
 */
static uint64_t allocate_page_table(void)
{
    uint64_t physical =
        pmm_alloc_frame();

    if (physical == 0)
        return 0;

    zero_page(physical);

    return physical;
}

/*
 * Get an existing child page table or create one.
 */
static uint64_t get_or_create_table(
    uint64_t *parent,
    uint64_t index,
    uint64_t flags
)
{
    uint64_t entry =
        parent[index];

    if (entry & VMM_PRESENT)
    {
        return entry & ADDRESS_MASK;
    }

    uint64_t child =
        allocate_page_table();

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
 * Initialize BATOS Virtual Memory Manager.
 */
void vmm_init(void)
{
    kernel_pml4 =
        vmm_create_address_space();
}

/*
 * Create a new empty 4-level address space.
 *
 * Returns the physical address of the PML4.
 */
uint64_t vmm_create_address_space(void)
{
    return allocate_page_table();
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
            flags
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
            flags
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
            flags
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
 * Prepare BATOS's address space for safe activation.
 *
 * The current CPU address space was initially prepared
 * by Limine.
 *
 * BATOS already owns a fresh PML4. We copy the current
 * PML4's top-level entries into BATOS's PML4.
 *
 * Existing BATOS mappings are preserved.
 *
 * This is intentionally a shallow clone:
 *
 *     current PML4
 *          │
 *          ├── existing lower-level tables
 *          │
 *          └── preserved by BATOS PML4
 *
 * A fully BATOS-owned page-table hierarchy will be
 * implemented later.
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

    uint64_t *current_pml4 =
        physical_to_virtual(
            current_pml4_physical
        );

    uint64_t *batos_pml4 =
        physical_to_virtual(
            kernel_pml4
        );

    /*
     * Import top-level mappings from the current
     * address space.
     *
     * Existing BATOS entries are NOT overwritten.
     */
    for (uint64_t i = 0;
         i < PAGE_TABLE_ENTRIES;
         i++)
    {
        if (batos_pml4[i] & VMM_PRESENT)
            continue;

        if (!(current_pml4[i] & VMM_PRESENT))
            continue;

        batos_pml4[i] =
            current_pml4[i];
    }

    return 0;
}