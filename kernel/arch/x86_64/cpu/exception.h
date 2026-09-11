#ifndef BATOS_EXCEPTION_H
#define BATOS_EXCEPTION_H

#include "../idt.h"

__attribute__((noreturn))
void exception_handler(struct exception_frame *frame);

#endif
