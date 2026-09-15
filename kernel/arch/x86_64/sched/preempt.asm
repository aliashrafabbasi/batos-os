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


; void x86_64_preempt_restore_context_and_resume(
;     const struct x86_64_context *next
; );
;
; SysV AMD64:
;   RDI = next context
;
; Restore a cooperative task continuation selected by
; scheduler-driven timer preemption.
;
; Unlike x86_64_context_restore(), this boundary is entered
; from an interrupt context with IF cleared by hardware.
; Therefore interrupts are explicitly re-enabled immediately
; before entering the restored continuation.
;
; The STI is followed directly by JMP. Per x86 interrupt
; semantics, the pending interrupt recognition boundary occurs
; after the instruction immediately following STI.

global x86_64_preempt_restore_context_and_resume

x86_64_preempt_restore_context_and_resume:
    mov rbx, [rdi + 0]
    mov rbp, [rdi + 8]
    mov r12, [rdi + 16]
    mov r13, [rdi + 24]
    mov r14, [rdi + 32]
    mov r15, [rdi + 40]

    mov rsp, [rdi + 48]
    mov rax, [rdi + 56]

    sti
    jmp rax
