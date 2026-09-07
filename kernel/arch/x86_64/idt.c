#include "idt.h"

struct idt_entry
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256] __attribute__((aligned(16)));
static struct idt_ptr idt_descriptor;

static uint16_t get_cs(void)
{
    uint16_t cs;

    __asm__ volatile (
        "mov %%cs, %0"
        : "=r"(cs)
    );

    return cs;
}

static void idt_set_gate(uint8_t vector, uint64_t handler)
{
    idt[vector].offset_low =
        (uint16_t)(handler & 0xFFFF);

    idt[vector].selector = get_cs();
    idt[vector].ist = 0;
    idt[vector].type_attr = 0x8E;

    idt[vector].offset_mid =
        (uint16_t)((handler >> 16) & 0xFFFF);

    idt[vector].offset_high =
        (uint32_t)((handler >> 32) & 0xFFFFFFFF);

    idt[vector].zero = 0;
}

void idt_init(void)
{
    __asm__ volatile ("cli" ::: "memory");

    for (int i = 0; i < 256; i++)
    {
        idt[i] = (struct idt_entry){0};
    }

    /*
     * Vector 0 = #DE Divide Error.
     * Only this exception is installed for now.
     */
    idt_set_gate(0, (uint64_t)exception_stub);

    idt_descriptor.limit = sizeof(idt) - 1;
    idt_descriptor.base = (uint64_t)&idt[0];

    __asm__ volatile (
        "lidt (%0)"
        :
        : "r"(&idt_descriptor)
        : "memory"
    );

    /*
     * Interrupts remain disabled.
     *
     * PIC/APIC and keyboard interrupts will be
     * enabled only after their handlers are ready.
     */
}
