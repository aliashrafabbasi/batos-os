#ifndef BATOS_TSS_H
#define BATOS_TSS_H

#include <stdint.h>

#define TSS_IST1_STACK_SIZE 4096

/*
 * GDT selector reserved for the 64-bit TSS.
 */
#define GDT_TSS 0x18

void tss_init(void);

uint64_t tss_get_base(void);
uint32_t tss_get_limit(void);

uint16_t tss_get_selector(void);
uint64_t tss_get_ist1(void);

#endif