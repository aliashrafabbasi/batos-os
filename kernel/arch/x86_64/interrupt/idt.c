#include "kernel/arch/x86_64/interrupt/idt.h"
#include "kernel/arch/x86_64/cpu/tss.h"

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

static struct idt_entry idt[256]
    __attribute__((aligned(16)));

static struct idt_ptr idt_descriptor;

/*
 * Read current code segment selector.
 */
static uint16_t get_cs(void)
{
    uint16_t cs;

    __asm__ volatile (
        "mov %%cs, %0"
        : "=r"(cs)
    );

    return cs;
}

/*
 * Install one IDT gate.
 *
 * Double Fault (#DF), vector 8,
 * uses IST1 from the TSS.
 */
static void idt_set_gate(
    uint8_t vector,
    uint64_t handler
)
{
    idt[vector].offset_low =
        (uint16_t)(handler & 0xFFFF);

    idt[vector].selector =
        get_cs();

    /*
     * IST index:
     *
     * 0 = normal stack
     * 1 = TSS.IST1
     */
    idt[vector].ist =
        (vector == 8) ? 1 : 0;

    idt[vector].type_attr = 0x8E;

    idt[vector].offset_mid =
        (uint16_t)((handler >> 16) & 0xFFFF);

    idt[vector].offset_high =
        (uint32_t)((handler >> 32) & 0xFFFFFFFF);

    idt[vector].zero = 0;
}

/*
 * Initialize IDT.
 */
void idt_init(void)
{
    __asm__ volatile (
        "cli"
        ::: "memory"
    );

    /*
     * Clear all 256 entries.
     */
    for (int i = 0; i < 256; i++)
    {
        idt[i] = (struct idt_entry){0};
    }

    /*
     * CPU exception handlers.
     */
    idt_set_gate(0,  (uint64_t)exception_stub_0);
    idt_set_gate(1,  (uint64_t)exception_stub_1);
    idt_set_gate(2,  (uint64_t)exception_stub_2);
    idt_set_gate(3,  (uint64_t)exception_stub_3);
    idt_set_gate(4,  (uint64_t)exception_stub_4);
    idt_set_gate(5,  (uint64_t)exception_stub_5);
    idt_set_gate(6,  (uint64_t)exception_stub_6);
    idt_set_gate(7,  (uint64_t)exception_stub_7);
    idt_set_gate(8,  (uint64_t)exception_stub_8);
    idt_set_gate(9,  (uint64_t)exception_stub_9);
    idt_set_gate(10, (uint64_t)exception_stub_10);
    idt_set_gate(11, (uint64_t)exception_stub_11);
    idt_set_gate(12, (uint64_t)exception_stub_12);
    idt_set_gate(13, (uint64_t)exception_stub_13);
    idt_set_gate(14, (uint64_t)exception_stub_14);
    idt_set_gate(15, (uint64_t)exception_stub_15);
    idt_set_gate(16, (uint64_t)exception_stub_16);
    idt_set_gate(17, (uint64_t)exception_stub_17);
    idt_set_gate(18, (uint64_t)exception_stub_18);
    idt_set_gate(19, (uint64_t)exception_stub_19);
    idt_set_gate(20, (uint64_t)exception_stub_20);
    idt_set_gate(21, (uint64_t)exception_stub_21);
    idt_set_gate(22, (uint64_t)exception_stub_22);
    idt_set_gate(23, (uint64_t)exception_stub_23);
    idt_set_gate(24, (uint64_t)exception_stub_24);
    idt_set_gate(25, (uint64_t)exception_stub_25);
    idt_set_gate(26, (uint64_t)exception_stub_26);
    idt_set_gate(27, (uint64_t)exception_stub_27);
    idt_set_gate(28, (uint64_t)exception_stub_28);
    idt_set_gate(29, (uint64_t)exception_stub_29);
    idt_set_gate(30, (uint64_t)exception_stub_30);
    idt_set_gate(31, (uint64_t)exception_stub_31);

    /*
     * Hardware IRQ handlers.
     *
     * PIC IRQ0-15 are mapped to vectors 32-47.
     */
    idt_set_gate(32, (uint64_t)irq_stub_0);
    idt_set_gate(33, (uint64_t)irq_stub_1);
    idt_set_gate(34, (uint64_t)irq_stub_2);
    idt_set_gate(35, (uint64_t)irq_stub_3);
    idt_set_gate(36, (uint64_t)irq_stub_4);
    idt_set_gate(37, (uint64_t)irq_stub_5);
    idt_set_gate(38, (uint64_t)irq_stub_6);
    idt_set_gate(39, (uint64_t)irq_stub_7);
    idt_set_gate(40, (uint64_t)irq_stub_8);
    idt_set_gate(41, (uint64_t)irq_stub_9);
    idt_set_gate(42, (uint64_t)irq_stub_10);
    idt_set_gate(43, (uint64_t)irq_stub_11);
    idt_set_gate(44, (uint64_t)irq_stub_12);
    idt_set_gate(45, (uint64_t)irq_stub_13);
    idt_set_gate(46, (uint64_t)irq_stub_14);
    idt_set_gate(47, (uint64_t)irq_stub_15);

    /*
     * LAPIC timer interrupts use dedicated vector 0xF0.
     * This vector is intentionally outside the external
     * IRQ range (0x20-0x2F).
     */
    idt_set_gate(
        0xF0,
        (uint64_t)lapic_timer_stub
    );

    /*
     * LAPIC spurious interrupts use vector 0xFF.
     * Install the dedicated handler before LAPIC SVR
     * software-enable can expose that vector.
     */
    idt_set_gate(0xFF, (uint64_t)lapic_spurious_stub);

    /*
     * Build IDTR.
     */
    idt_descriptor.limit =
        sizeof(idt) - 1;

    idt_descriptor.base =
        (uint64_t)&idt[0];

    /*
     * Load IDTR.
     */
    __asm__ volatile (
        "lidt (%0)"
        :
        : "r"(&idt_descriptor)
        : "memory"
    );
}

/*
 * Return IST field of an IDT gate.
 */
uint8_t idt_get_ist(uint8_t vector)
{
    return idt[vector].ist;
}

/*
 * Return selector field of an IDT gate.
 */
uint16_t idt_get_selector(uint8_t vector)
{
    return idt[vector].selector;
}