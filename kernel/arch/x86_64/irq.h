#ifndef BATOS_IRQ_H
#define BATOS_IRQ_H

#include <stdint.h>

#define IRQ_VECTOR_BASE 32
#define IRQ_COUNT 16

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

typedef void (*irq_handler_t)(struct irq_frame *frame);

void irq_init(void);
void irq_dispatch(struct irq_frame *frame);

int irq_register_handler(uint8_t irq, irq_handler_t handler);

uint64_t irq_get_ticks(void);

extern void irq_stub_0(void);
extern void irq_stub_1(void);
extern void irq_stub_2(void);
extern void irq_stub_3(void);
extern void irq_stub_4(void);
extern void irq_stub_5(void);
extern void irq_stub_6(void);
extern void irq_stub_7(void);
extern void irq_stub_8(void);
extern void irq_stub_9(void);
extern void irq_stub_10(void);
extern void irq_stub_11(void);
extern void irq_stub_12(void);
extern void irq_stub_13(void);
extern void irq_stub_14(void);
extern void irq_stub_15(void);

#endif
