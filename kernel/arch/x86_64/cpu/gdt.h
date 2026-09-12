#ifndef BATOS_GDT_H
#define BATOS_GDT_H

#include <stdint.h>

/*
 * GDT segment selectors.
 *
 * Index 0: Null descriptor
 * Index 1: Kernel Code
 * Index 2: Kernel Data
 */
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10

/*
 * Initialize the Global Descriptor Table.
 */
void gdt_init(void);

#endif
