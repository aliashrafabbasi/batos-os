BITS 64

section .text

global exception_stub

exception_stub:
    cli

    mov dx, 0x3F8
    mov al, 'D'
    out dx, al

.hang:
    hlt
    jmp .hang
