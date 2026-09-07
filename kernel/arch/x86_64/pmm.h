#ifndef BATOS_PMM_H
#define BATOS_PMM_H

#include <stdint.h>

void pmm_init(void);

uint64_t pmm_alloc_frame(void);
void pmm_free_frame(uint64_t physical_address);

uint64_t pmm_get_total_frames(void);
uint64_t pmm_get_free_frames(void);
uint64_t pmm_get_used_frames(void);
uint64_t pmm_get_bitmap_physical(void);
uint64_t pmm_get_bitmap_size(void);
uint64_t pmm_get_hhdm_offset(void);

#endif
