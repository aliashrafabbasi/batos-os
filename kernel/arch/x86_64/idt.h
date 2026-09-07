#ifndef BATOS_IDT_H
#define BATOS_IDT_H

#include <stdint.h>

void idt_init(void);

extern void exception_stub(void);

#endif
