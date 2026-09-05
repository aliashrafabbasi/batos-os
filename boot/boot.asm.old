bits 32

global _start
extern kernel_main

section .text

_start:
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
