#ifndef BATOS_VMM_H
#define BATOS_VMM_H

#include <stdint.h>

#define VMM_PAGE_SIZE 4096ULL

/*
 * VMM-3.2B address-space lifecycle states.
 */
#define VMM_ADDRESS_SPACE_CREATED  0
#define VMM_ADDRESS_SPACE_ACTIVE   1
#define VMM_ADDRESS_SPACE_INACTIVE 2

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
 * The currently active Limine hierarchy is recursively
 * cloned into independently allocated BATOS page tables.
 *
 * Existing BATOS-owned mappings are preserved.
 *
 * Returns:
 *      0  = success
 *     -1  = failure
 */
int vmm_prepare_address_space(void);

/*
 * Get the standalone PML4 produced by the most recent
 * recursive clone operation.
 *
 * This root is retained for VMM structural verification.
 *
 * Returns:
 *     Physical address of cloned PML4.
 *     0 if no clone has been created.
 */
uint64_t vmm_get_last_cloned_pml4(void);

/*
 * Inspect one address-space PML4.
 *
 * This function is read-only.
 *
 * Returns:
 *      0  = success
 *     -1  = failure
 */
int vmm_inspect_address_space(
    uint64_t pml4_physical,
    uint64_t *present_entries
);

/*
 * Verify one complete virtual-address path through
 * two page-table hierarchies.
 *
 * The verification checks:
 *
 *     PML4 → PDPT → PD → PT → PTE
 *
 * and confirms that page-table pages are independently
 * allocated while the final physical mapping and flags
 * are preserved.
 *
 * Returns:
 *      0  = verified
 *     -1  = verification failed
 */
int vmm_verify_recursive_clone(
    uint64_t source_pml4_physical,
    uint64_t cloned_pml4_physical,
    uint64_t virtual_address
);

/*
 * Verify the complete recursively cloned hierarchy.
 *
 * Every present non-huge mapping in the source hierarchy
 * must exist in the clone.
 *
 * Page-table pages must be physically independent.
 * Leaf PTEs must remain identical.
 *
 * Returns:
 *      0  = recursive clone verified
 *     -1  = verification failed
 */
int vmm_verify_clone(
    uint64_t source_pml4_physical,
    uint64_t cloned_pml4_physical
);



/*
 * Verify that a PML4 is registered as the owner
 * of its own address space.
 *
 * Returns:
 *      0  = ownership verified
 *     -1  = verification failed
 */
int vmm_verify_page_table_root(
    uint64_t pml4_physical
);

/*
 * Verify ownership metadata for the complete
 * PML4 -> PDPT -> PD -> PT path of a virtual address.
 *
 * Returns:
 *      0  = ownership verified
 *     -1  = verification failed
 */
int vmm_verify_page_table_ownership(
    uint64_t pml4_physical,
    uint64_t virtual_address
);

/*
 * VMM-3.2B:
 * Get the lifecycle state of an address space.
 */
int vmm_get_address_space_state(
    uint64_t pml4_physical,
    uint8_t *state
);

/*
 * VMM-3.2B:
 * Activate a registered address space.
 */
int vmm_activate_address_space(
    uint64_t pml4_physical
);

/*
 * VMM-3.2B:
 * Verify the lifecycle state of an address space.
 */
int vmm_verify_address_space_state(
    uint64_t pml4_physical,
    uint8_t expected_state
);

#endif
