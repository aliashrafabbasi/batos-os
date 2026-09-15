BITS 64

section .text

global x86_64_preempt_restore_and_iret

; Architecture-owned preemptive return boundary.
;
; Input:
;   RDI = address of a complete struct irq_frame.
;
; The frame contains:
;   r15..rax
;   vector
;   rip
;   cs
;   rflags
;
; The vector field is metadata and is not consumed by iretq.
;
; This entry point is the architectural interrupt-return
; boundary for LAPIC timer preemption. The timer path selects
; a valid resumable frame before transferring control here.

x86_64_preempt_restore_and_iret:
    mov rsp, rdi

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 8                  ; discard vector
    iretq
