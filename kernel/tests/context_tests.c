#include "context_tests.h"

#include <stdint.h>

#include "../arch/x86_64/sched/context.h"

static struct x86_64_context context_a;
static struct x86_64_context context_b;

static uint8_t context_b_stack[4096]
    __attribute__((aligned(16)));

static volatile uint64_t context_b_reached = 0;
static volatile uint64_t context_a_resumed = 0;

static volatile uint64_t context_rbx_after = 0;
static volatile uint64_t context_rbp_after = 0;
static volatile uint64_t context_r12_after = 0;
static volatile uint64_t context_r13_after = 0;
static volatile uint64_t context_r14_after = 0;
static volatile uint64_t context_r15_after = 0;

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

    /*
     * Seed the SysV AMD64 callee-saved registers immediately before
     * entering the context switch. Context A must preserve these
     * values across A -> B -> A.
     */
    __asm__ volatile (
        "mov $0x4241544f53524258, %%rbx\n"
        "mov $0x4241544f53525042, %%rbp\n"
        "mov $0x4241544f53523132, %%r12\n"
        "mov $0x4241544f53523133, %%r13\n"
        "mov $0x4241544f53523134, %%r14\n"
        "mov $0x4241544f53523135, %%r15\n"
        :
        :
        : "rbx", "rbp", "r12", "r13", "r14", "r15"
    );

    x86_64_context_switch(
        &context_a,
        &context_b
    );

    /*
     * Returning here proves that Context A's continuation
     * was restored correctly. Capture the callee-saved registers
     * before executing any further C logic.
     */
    __asm__ volatile (
        "mov %%rbx, %0\n"
        "mov %%rbp, %1\n"
        "mov %%r12, %2\n"
        "mov %%r13, %3\n"
        "mov %%r14, %4\n"
        "mov %%r15, %5\n"
        : "=m"(context_rbx_after),
          "=m"(context_rbp_after),
          "=m"(context_r12_after),
          "=m"(context_r13_after),
          "=m"(context_r14_after),
          "=m"(context_r15_after)
        :
        : "memory"
    );

    context_a_resumed = 1;

    if (context_b_reached != 1 ||
        context_a_resumed != 1 ||
        context_rbx_after != 0x4241544f53524258ULL ||
        context_rbp_after != 0x4241544f53525042ULL ||
        context_r12_after != 0x4241544f53523132ULL ||
        context_r13_after != 0x4241544f53523133ULL ||
        context_r14_after != 0x4241544f53523134ULL ||
        context_r15_after != 0x4241544f53523135ULL)
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
