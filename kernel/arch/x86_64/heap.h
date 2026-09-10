#ifndef BATOS_HEAP_H
#define BATOS_HEAP_H

#include <stdint.h>

void heap_init(void);

void *kmalloc(uint64_t size);

void kfree(void *ptr);

#endif
