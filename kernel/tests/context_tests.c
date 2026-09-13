#include "context_tests.h"

#include <stdint.h>

#include "../arch/x86_64/sched/context.h"

static struct x86_64_context context_a;
static struct x86_64_context context_b;

static uint8_t context_b_stack[4096]
    __attribute__((aligned(16)));

static volatile uint64_t context_b_reached = 0;
static volatile uint64_t context_a_resumed = 0;

static void context_test_b(void);

static void context_test_b_return_trap(void)
{
    /*
     * A fresh context must never accidentally return.
     */
    for (;;)
    {
        __asm__ volatile (
            "cli\n"
            "hlt\n"
        );
    }
}

static void context_test_b(void)
{
    context_b_reached = 1;

    x86_64_context_switch(
        &context_b,
        &context_a
    );

    /*
     * We should never resume here.
     */
    context_test_b_return_trap();
}

void context_tests_run(void)
{
    uint64_t stack_top =
        (uint64_t)(uintptr_t)&context_b_stack[sizeof(context_b_stack)];

    /*
     * SysV AMD64 function-entry ABI:
     *
     * RSP % 16 == 8 at function entry.
     *
     * Reserve one return-address slot even though the test
     * switches away before returning.
     */
    stack_top -= sizeof(uint64_t);

    *(uint64_t *)(uintptr_t)stack_top =
        (uint64_t)(uintptr_t)context_test_b_return_trap;

    context_b.rbx = 0;
    context_b.rbp = 0;
    context_b.r12 = 0;
    context_b.r13 = 0;
    context_b.r14 = 0;
    context_b.r15 = 0;
    context_b.rsp = stack_top;
    context_b.rip = (uint64_t)(uintptr_t)context_test_b;

    x86_64_context_switch(
        &context_a,
        &context_b
    );

    /*
     * Returning here proves that Context A's continuation
     * was restored correctly.
     */
    context_a_resumed = 1;

    if (context_b_reached != 1 || context_a_resumed != 1)
    {
        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt\n"
            );
        }
    }
}
