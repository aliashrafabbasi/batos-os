#ifndef BATOS_HEAP_H
#define BATOS_HEAP_H

#include <stdint.h>

void heap_init(void);

void *kmalloc(uint64_t size);

void kfree(void *ptr);

/*
 * Heap-1B.2 dynamic page backing.
 *
 * These primitives manage one PMM/VMM-backed heap page.
 * The general kmalloc()/kfree() allocator remains Heap-1A
 * until dynamic allocation is formally integrated.
 */
uint64_t heap_dynamic_page_acquire(void);

int heap_dynamic_page_release(uint64_t virtual_address);

#endif
