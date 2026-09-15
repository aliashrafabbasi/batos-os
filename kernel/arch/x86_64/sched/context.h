#ifndef BATOS_X86_64_CONTEXT_H
#define BATOS_X86_64_CONTEXT_H

#include <stdint.h>
#include <stddef.h>

struct x86_64_context
{
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;

    uint64_t rsp;
    uint64_t rip;
};

/*
 * x86_64_context is consumed directly by switch.asm.
 * Keep the C layout mechanically synchronized with the
 * assembly offsets.
 */
_Static_assert(offsetof(struct x86_64_context, rbx) == 0, "context.rbx offset mismatch");
_Static_assert(offsetof(struct x86_64_context, rbp) == 8, "context.rbp offset mismatch");
_Static_assert(offsetof(struct x86_64_context, r12) == 16, "context.r12 offset mismatch");
_Static_assert(offsetof(struct x86_64_context, r13) == 24, "context.r13 offset mismatch");
_Static_assert(offsetof(struct x86_64_context, r14) == 32, "context.r14 offset mismatch");
_Static_assert(offsetof(struct x86_64_context, r15) == 40, "context.r15 offset mismatch");
_Static_assert(offsetof(struct x86_64_context, rsp) == 48, "context.rsp offset mismatch");
_Static_assert(offsetof(struct x86_64_context, rip) == 56, "context.rip offset mismatch");
_Static_assert(sizeof(struct x86_64_context) == 64, "context size mismatch");

void x86_64_context_switch(
    struct x86_64_context *current,
    const struct x86_64_context *next
);

void x86_64_context_save(
    struct x86_64_context *current
);

__attribute__((noreturn))
void x86_64_context_restore(
    const struct x86_64_context *next
);

#endif
