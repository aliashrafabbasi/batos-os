#ifndef IDT_H
#define IDT_H

#include <stdint.h>

struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  ist;
    uint8_t  flags;
    uint16_t base_middle;
    uint32_t base_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void idt_init(void);
void keyboard_handler(void);

#endif