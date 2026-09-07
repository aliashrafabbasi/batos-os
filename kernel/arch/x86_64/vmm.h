#ifndef BATOS_VMM_H
#define BATOS_VMM_H

#include <stdint.h>

#define VMM_PAGE_SIZE 4096ULL

#define VMM_PRESENT  (1ULL << 0)
#define VMM_WRITABLE (1ULL << 1)
#define VMM_USER     (1ULL << 2)
#define VMM_HUGE     (1ULL << 7)

void vmm_init(void);

uint64_t vmm_create_address_space(void);

int vmm_map_page(
    uint64_t pml4_physical,
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t flags
);

uint64_t vmm_get_pml4(void);

#endif