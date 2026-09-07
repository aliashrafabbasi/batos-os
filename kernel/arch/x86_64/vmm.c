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

static uint64_t *physical_to_virtual(uint64_t physical)
{
    /*
     * Limine provides the Higher Half Direct Map.
     *
     * PMM knows the HHDM offset, so every physical
     * page-table frame can be accessed through it.
     */
    return (uint64_t *)(physical + pmm_get_hhdm_offset());
}

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

static uint64_t allocate_page_table(void)
{
    uint64_t physical =
        pmm_alloc_frame();

    if (physical == 0)
        return 0;

    zero_page(physical);

    return physical;
}

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

void vmm_init(void)
{
    kernel_pml4 =
        vmm_create_address_space();
}

uint64_t vmm_create_address_space(void)
{
    return allocate_page_table();
}

int vmm_map_page(
    uint64_t pml4_physical,
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t flags
)
{
    if (pml4_physical == 0)
        return -1;

    if (virtual_address & (VMM_PAGE_SIZE - 1))
        return -1;

    if (physical_address & (VMM_PAGE_SIZE - 1))
        return -1;

    uint64_t *pml4 =
        physical_to_virtual(pml4_physical);

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

    pt[index] =
        (physical_address & ADDRESS_MASK) |
        VMM_PRESENT |
        (flags & (VMM_WRITABLE | VMM_USER));

    return 0;
}

uint64_t vmm_get_pml4(void)
{
    return kernel_pml4;
}