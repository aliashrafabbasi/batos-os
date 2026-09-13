#ifndef BATOS_X86_64_CONTEXT_H
#define BATOS_X86_64_CONTEXT_H

#include <stdint.h>

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

void x86_64_context_switch(
    struct x86_64_context *current,
    const struct x86_64_context *next
);

#endif
