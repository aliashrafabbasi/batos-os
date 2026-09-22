BITS 64

section .text

global x86_64_context_switch
global x86_64_context_switch_and_enable_interrupts
global x86_64_context_save
global x86_64_context_restore
global x86_64_context_switch_to_interrupt

; void x86_64_context_switch(
;     struct x86_64_context *current,
;     const struct x86_64_context *next
; );
;
; SysV AMD64:
;   RDI = current
;   RSI = next
;
; Context layout:
;   +0   RBX
;   +8   RBP
;   +16  R12
;   +24  R13
;   +32  R14
;   +40  R15
;   +48  RSP
;   +56  RIP

x86_64_context_switch:
    ; Save callee-saved registers.
    mov [rdi + 0],  rbx
    mov [rdi + 8],  rbp
    mov [rdi + 16], r12
    mov [rdi + 24], r13
    mov [rdi + 32], r14
    mov [rdi + 40], r15

    ; Save the current stack pointer.
    mov [rdi + 48], rsp

    ; Save a continuation address.
    lea rax, [rel .resume]
    mov [rdi + 56], rax

    ; Restore next context.
    mov rbx, [rsi + 0]
    mov rbp, [rsi + 8]
    mov r12, [rsi + 16]
    mov r13, [rsi + 24]
    mov r14, [rsi + 32]
    mov r15, [rsi + 40]

    mov rsp, [rsi + 48]
    mov rax, [rsi + 56]

    ; Enter next context without changing interrupt state.
    jmp rax

.resume:
    ret

; void x86_64_context_switch_and_enable_interrupts(
;     struct x86_64_context *current,
;     const struct x86_64_context *next
; );
;
; Scheduler-owned handoff used when the caller has deliberately
; kept maskable interrupts disabled while committing scheduler
; ownership. The destination context is entered only after
; scheduler_current and CPU execution ownership are consistent.
;
; SysV AMD64:
;   RDI = current
;   RSI = next

x86_64_context_switch_and_enable_interrupts:
    ; Save callee-saved registers.
    mov [rdi + 0],  rbx
    mov [rdi + 8],  rbp
    mov [rdi + 16], r12
    mov [rdi + 24], r13
    mov [rdi + 32], r14
    mov [rdi + 40], r15

    ; Save the current stack pointer.
    mov [rdi + 48], rsp

    ; Save a continuation address.
    lea rax, [rel .scheduler_resume]
    mov [rdi + 56], rax

    ; Restore next context.
    mov rbx, [rsi + 0]
    mov rbp, [rsi + 8]
    mov r12, [rsi + 16]
    mov r13, [rsi + 24]
    mov r14, [rsi + 32]
    mov r15, [rsi + 40]

    mov rsp, [rsi + 48]
    mov rax, [rsi + 56]

    ; The scheduler ownership commit is complete.
    ; Establish the normal task execution interrupt state
    ; immediately before entering the destination continuation.
    sti
    jmp rax

.scheduler_resume:
    ret

; void x86_64_context_save(
;     struct x86_64_context *current
; );
;
; SysV AMD64:
;   RDI = current
;
; Save the current cooperative continuation and return normally.
; When this context is restored later, execution resumes at
; .save_resume and the original caller continues after the
; x86_64_context_save() call.

x86_64_context_save:
    ; Save callee-saved registers.
    mov [rdi + 0],  rbx
    mov [rdi + 8],  rbp
    mov [rdi + 16], r12
    mov [rdi + 24], r13
    mov [rdi + 32], r14
    mov [rdi + 40], r15

    ; Save the current stack pointer.
    mov [rdi + 48], rsp

    ; Save a continuation address.
    lea rax, [rel .save_resume]
    mov [rdi + 56], rax

    ret

.save_resume:
    ret


; void x86_64_context_restore(
;     const struct x86_64_context *next
; );
;
; SysV AMD64:
;   RDI = next
;
; Restore a previously saved cooperative continuation.
; This function intentionally does not return to its caller.

x86_64_context_restore:
    ; Restore callee-saved registers.
    mov rbx, [rdi + 0]
    mov rbp, [rdi + 8]
    mov r12, [rdi + 16]
    mov r13, [rdi + 24]
    mov r14, [rdi + 32]
    mov r15, [rdi + 40]

    ; Restore stack and continuation.
    mov rsp, [rdi + 48]
    mov rax, [rdi + 56]

    ; Enter the restored context.
    jmp rax


; void x86_64_context_switch_to_interrupt(
;     struct x86_64_context *current,
;     const struct irq_frame *target
; );
;
; SysV AMD64:
;   RDI = current cooperative context
;   RSI = target interrupt-return frame
;
; Save the current cooperative continuation exactly like
; x86_64_context_switch(), then restore the authoritative
; interrupt-return frame and terminate the handoff with iretq.
;
; The target frame contains:
;   r15..rax
;   vector
;   rip
;   cs
;   rflags
;
; The vector is metadata and is discarded before iretq.
;
; This primitive deliberately does not modify the target RFLAGS.
; Interrupt state therefore comes from the target continuation's
; authoritative interrupt frame.

x86_64_context_switch_to_interrupt:
    ; Save current cooperative continuation.
    mov [rdi + 0],  rbx
    mov [rdi + 8],  rbp
    mov [rdi + 16], r12
    mov [rdi + 24], r13
    mov [rdi + 32], r14
    mov [rdi + 40], r15
    mov [rdi + 48], rsp

    lea rax, [rel .interrupt_resume]
    mov [rdi + 56], rax

    ; Restore the authoritative interrupt-return frame.
    mov rsp, rsi

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

.interrupt_resume:
    ret
