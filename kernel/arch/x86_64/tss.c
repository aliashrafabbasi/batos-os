#include "tss.h"

struct tss
{
    uint32_t reserved0;

    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;

    uint64_t reserved1;

    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;

    uint64_t reserved2;

    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

static struct tss kernel_tss
    __attribute__((aligned(16)));

static uint8_t ist1_stack[TSS_IST1_STACK_SIZE]
    __attribute__((aligned(16)));

void tss_init(void)
{
    kernel_tss = (struct tss){0};

    /*
     * IST stacks grow downward.
     * Therefore IST1 points to the top of the stack.
     */
    kernel_tss.ist1 =
        (uint64_t)&ist1_stack[TSS_IST1_STACK_SIZE];

    /*
     * Disable the I/O permission bitmap for now.
     */
    kernel_tss.iomap_base = sizeof(struct tss);
}

uint64_t tss_get_base(void)
{
    return (uint64_t)&kernel_tss;
}

uint32_t tss_get_limit(void)
{
    return sizeof(struct tss) - 1;
}

uint16_t tss_get_selector(void)
{
    uint16_t selector;

    __asm__ volatile (
        "str %0"
        : "=r"(selector)
    );

    return selector;
}

uint64_t tss_get_ist1(void)
{
    return kernel_tss.ist1;
}