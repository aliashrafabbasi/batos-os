#ifndef BATOS_IRQ_H
#define BATOS_IRQ_H

#include <stdint.h>
#include <stddef.h>

#define IRQ_VECTOR_BASE 32
#define IRQ_COUNT 16

enum irq_controller
{
    IRQ_CONTROLLER_PIC = 0,
    IRQ_CONTROLLER_LAPIC = 1
};

struct irq_frame
{
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;

    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    uint64_t vector;

    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
};

/*
 * Hardware interrupt entry assembly consumes this layout directly.
 *
 * The frame begins at the exact RSP used when the GPR save sequence
 * completes. Keep these offsets synchronized with interrupts.asm.
 */
_Static_assert(
    offsetof(struct irq_frame, r15) == 0,
    "irq_frame.r15 offset mismatch"
);

_Static_assert(
    offsetof(struct irq_frame, r14) == 8,
    "irq_frame.r14 offset mismatch"
);

_Static_assert(
    offsetof(struct irq_frame, r13) == 16,
    "irq_frame.r13 offset mismatch"
);

_Static_assert(
    offsetof(struct irq_frame, r12) == 24,
    "irq_frame.r12 offset mismatch"
);

_Static_assert(
    offsetof(struct irq_frame, r11) == 32,
    "irq_frame.r11 offset mismatch"
);

_Static_assert(
    offsetof(struct irq_frame, r10) == 40,
    "irq_frame.r10 offset mismatch"
);

_Static_assert(
    offsetof(struct irq_frame, r9) == 48,
    "irq_frame.r9 offset mismatch"
);

_Static_assert(
    offsetof(struct irq_frame, r8) == 56,
    "irq_frame.r8 offset mismatch"
);

_Static_assert(
    offsetof(struct irq_frame, rdi) == 64,
    "irq_frame.rdi offset mismatch"
);

_Static_assert(
    offsetof(struct irq_frame, rsi) == 72,
    "irq_frame.rsi offset mismatch"
);

_Static_assert(
    offsetof(struct irq_frame, rbp) == 80,
    "irq_frame.rbp offset mismatch"
);

_Static_assert(
    offsetof(struct irq_frame, rdx) == 88,
    "irq_frame.rdx offset mismatch"
);

_Static_assert(
    offsetof(struct irq_frame, rcx) == 96,
    "irq_frame.rcx offset mismatch"
);

_Static_assert(
    offsetof(struct irq_frame, rbx) == 104,
    "irq_frame.rbx offset mismatch"
);

_Static_assert(
    offsetof(struct irq_frame, rax) == 112,
    "irq_frame.rax offset mismatch"
);

_Static_assert(
    offsetof(struct irq_frame, vector) == 120,
    "irq_frame.vector offset mismatch"
);

_Static_assert(
    offsetof(struct irq_frame, rip) == 128,
    "irq_frame.rip offset mismatch"
);

_Static_assert(
    offsetof(struct irq_frame, cs) == 136,
    "irq_frame.cs offset mismatch"
);

_Static_assert(
    offsetof(struct irq_frame, rflags) == 144,
    "irq_frame.rflags offset mismatch"
);

_Static_assert(
    sizeof(struct irq_frame) == 152,
    "irq_frame size mismatch"
);

typedef void (*irq_handler_t)(struct irq_frame *frame);

void irq_init(void);
void irq_dispatch(struct irq_frame *frame);

int irq_register_handler(uint8_t irq, irq_handler_t handler);

int irq_set_controller(
    uint8_t irq,
    enum irq_controller controller
);

enum irq_controller irq_get_controller(uint8_t irq);

uint64_t irq_get_irq0_delivery_count(void);


#endif
