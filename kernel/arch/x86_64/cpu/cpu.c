#include "cpu.h"

#include "../gdt.h"
#include "../interrupt/idt.h"

void cpu_init(void)
{
    gdt_init();
    idt_init();
}
