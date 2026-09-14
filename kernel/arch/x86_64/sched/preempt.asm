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
; This entry point is intentionally not connected to the timer
; interrupt path yet. PREEMPTION-1B first establishes and tests
; the frame/return ABI before integrating scheduling decisions.

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
