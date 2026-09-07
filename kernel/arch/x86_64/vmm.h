#ifndef BATOS_VMM_H
#define BATOS_VMM_H

#include <stdint.h>

#define VMM_PAGE_SIZE 4096ULL

#define VMM_PRESENT  (1ULL << 0)
#define VMM_WRITABLE (1ULL << 1)
#define VMM_USER     (1ULL << 2)
#define VMM_HUGE     (1ULL << 7)

/*
 * Initialize BATOS Virtual Memory Manager.
 */
void vmm_init(void);

/*
 * Create a new empty 4-level x86-64 address space.
 *
 * Returns:
 *     Physical address of the new PML4.
 *     0 on failure.
 */
uint64_t vmm_create_address_space(void);

/*
 * Map one 4 KiB virtual page to one physical frame.
 *
 * Returns:
 *      0  = success
 *     -1  = failure
 */
int vmm_map_page(
    uint64_t pml4_physical,
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t flags
);

/*
 * Walk the BATOS page tables and translate a virtual
 * address into its corresponding physical address.
 *
 * This performs a software page-table walk.
 * It does NOT modify CR3.
 *
 * Returns:
 *      0  = translation successful
 *     -1  = translation failed
 */
int vmm_translate(
    uint64_t pml4_physical,
    uint64_t virtual_address,
    uint64_t *physical_address
);

/*
 * Get the physical address of BATOS's kernel PML4.
 */
uint64_t vmm_get_pml4(void);

/*
 * Read the CPU's current CR3 register.
 */
uint64_t vmm_read_cr3(void);

/*
 * Write a physical PML4 address into CR3.
 */
void vmm_write_cr3(uint64_t pml4_physical);

/*
 * Prepare BATOS's address space for safe activation.
 *
 * The current active PML4 is shallow-copied into
 * BATOS's PML4. Existing BATOS-owned entries are
 * preserved.
 *
 * Returns:
 *      0  = success
 *     -1  = failure
 */
int vmm_prepare_address_space(void);

/*
 * Inspect BATOS's active address-space structure.
 *
 * Returns:
 *      0  = success
 *     -1  = failure
 */
int vmm_inspect_address_space(
    uint64_t pml4_physical,
    uint64_t *present_entries
);


#endif