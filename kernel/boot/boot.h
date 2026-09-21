#ifndef BATOS_BOOT_H
#define BATOS_BOOT_H

#include <stdint.h>

void boot_init(void);

uint64_t boot_get_kernel_physical_base(void);
uint64_t boot_get_kernel_virtual_base(void);
uint64_t boot_get_kernel_image_size(void);

uint64_t boot_get_kernel_text_start(void);
uint64_t boot_get_kernel_text_end(void);

uint64_t boot_get_kernel_rodata_start(void);
uint64_t boot_get_kernel_rodata_end(void);

uint64_t boot_get_kernel_data_start(void);
uint64_t boot_get_kernel_data_end(void);

uint64_t boot_get_kernel_bss_start(void);
uint64_t boot_get_kernel_bss_end(void);

#endif
