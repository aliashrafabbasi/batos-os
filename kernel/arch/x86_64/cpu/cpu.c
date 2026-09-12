#include "cpu.h"

#include "../gdt.h"
#include "../idt.h"

void cpu_init(void)
{
    gdt_init();
    idt_init();
}
