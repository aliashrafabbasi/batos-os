BITS 64

section .text

global exception_stub_0
global exception_stub_1
global exception_stub_2
global exception_stub_3
global exception_stub_4
global exception_stub_5
global exception_stub_6
global exception_stub_7
global exception_stub_8
global exception_stub_9
global exception_stub_10
global exception_stub_11
global exception_stub_12
global exception_stub_13
global exception_stub_14
global exception_stub_15
global exception_stub_16
global exception_stub_17
global exception_stub_18
global exception_stub_19
global exception_stub_20
global exception_stub_21
global exception_stub_22
global exception_stub_23
global exception_stub_24
global exception_stub_25
global exception_stub_26
global exception_stub_27
global exception_stub_28
global exception_stub_29
global exception_stub_30
global exception_stub_31

extern exception_handler


; ============================================================
; Common CPU Exception Handler
;
; Stack before our pushes:
;
;   [rsp + 0]   = vector
;   [rsp + 8]   = error code
;   [rsp + 16]  = RIP
;   [rsp + 24]  = CS
;   [rsp + 32]  = RFLAGS
;
; After saving 15 registers:
;
;   [rsp + 0]   = R15
;   [rsp + 8]   = R14
;   [rsp + 16]  = R13
;   [rsp + 24]  = R12
;   [rsp + 32]  = R11
;   [rsp + 40]  = R10
;   [rsp + 48]  = R9
;   [rsp + 56]  = R8
;   [rsp + 64]  = RDI
;   [rsp + 72]  = RSI
;   [rsp + 80]  = RBP
;   [rsp + 88]  = RDX
;   [rsp + 96]  = RCX
;   [rsp + 104] = RBX
;   [rsp + 112] = RAX
;   [rsp + 120] = vector
;   [rsp + 128] = error code
;   [rsp + 136] = RIP
;   [rsp + 144] = CS
;   [rsp + 152] = RFLAGS
;
; RDI receives the pointer to the complete exception frame.
; ============================================================

exception_common:
    cli
    cld

    ; Save all general-purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; First argument = exception_frame*
    mov rdi, rsp

    ; Align stack for System V AMD64 ABI
    and rsp, -16

    ; Call C exception handler
    call exception_handler

.hang:
    cli
    hlt
    jmp .hang


; ============================================================
; Exception 0 - Divide Error (#DE)
; No CPU error code
; ============================================================

exception_stub_0:
    push 0
    push 0
    jmp exception_common


; ============================================================
; Exception 1 - Debug (#DB)
; ============================================================

exception_stub_1:
    push 0
    push 1
    jmp exception_common


; ============================================================
; Exception 2 - Non-Maskable Interrupt (NMI)
; ============================================================

exception_stub_2:
    push 0
    push 2
    jmp exception_common


; ============================================================
; Exception 3 - Breakpoint (#BP)
; ============================================================

exception_stub_3:
    push 0
    push 3
    jmp exception_common


; ============================================================
; Exception 4 - Overflow (#OF)
; ============================================================

exception_stub_4:
    push 0
    push 4
    jmp exception_common


; ============================================================
; Exception 5 - Bound Range Exceeded (#BR)
; ============================================================

exception_stub_5:
    push 0
    push 5
    jmp exception_common


; ============================================================
; Exception 6 - Invalid Opcode (#UD)
; ============================================================

exception_stub_6:
    push 0
    push 6
    jmp exception_common


; ============================================================
; Exception 7 - Device Not Available (#NM)
; ============================================================

exception_stub_7:
    push 0
    push 7
    jmp exception_common


; ============================================================
; Exception 8 - Double Fault (#DF)
; CPU pushes error code
; ============================================================

exception_stub_8:
    push 8
    jmp exception_common


; ============================================================
; Exception 9 - Coprocessor Segment Overrun
; Legacy / Reserved
; ============================================================

exception_stub_9:
    push 0
    push 9
    jmp exception_common


; ============================================================
; Exception 10 - Invalid TSS (#TS)
; CPU pushes error code
; ============================================================

exception_stub_10:
    push 10
    jmp exception_common


; ============================================================
; Exception 11 - Segment Not Present (#NP)
; CPU pushes error code
; ============================================================

exception_stub_11:
    push 11
    jmp exception_common


; ============================================================
; Exception 12 - Stack-Segment Fault (#SS)
; CPU pushes error code
; ============================================================

exception_stub_12:
    push 12
    jmp exception_common


; ============================================================
; Exception 13 - General Protection Fault (#GP)
; CPU pushes error code
; ============================================================

exception_stub_13:
    push 13
    jmp exception_common


; ============================================================
; Exception 14 - Page Fault (#PF)
; CPU pushes error code
; ============================================================

exception_stub_14:
    push 14
    jmp exception_common


; ============================================================
; Exception 15 - Reserved
; ============================================================

exception_stub_15:
    push 0
    push 15
    jmp exception_common


; ============================================================
; Exception 16 - x87 Floating-Point Exception (#MF)
; ============================================================

exception_stub_16:
    push 0
    push 16
    jmp exception_common


; ============================================================
; Exception 17 - Alignment Check (#AC)
; CPU pushes error code
; ============================================================

exception_stub_17:
    push 17
    jmp exception_common


; ============================================================
; Exception 18 - Machine Check (#MC)
; ============================================================

exception_stub_18:
    push 0
    push 18
    jmp exception_common


; ============================================================
; Exception 19 - SIMD Floating-Point Exception (#XM/#XF)
; ============================================================

exception_stub_19:
    push 0
    push 19
    jmp exception_common


; ============================================================
; Exception 20 - Virtualization Exception (#VE)
; ============================================================

exception_stub_20:
    push 0
    push 20
    jmp exception_common


; ============================================================
; Exception 21 - Control Protection Exception (#CP)
; CPU pushes error code
; ============================================================

exception_stub_21:
    push 21
    jmp exception_common


; ============================================================
; Exception 22 - Reserved
; ============================================================

exception_stub_22:
    push 0
    push 22
    jmp exception_common


; ============================================================
; Exception 23 - Reserved
; ============================================================

exception_stub_23:
    push 0
    push 23
    jmp exception_common


; ============================================================
; Exception 24 - Reserved
; ============================================================

exception_stub_24:
    push 0
    push 24
    jmp exception_common


; ============================================================
; Exception 25 - Reserved
; ============================================================

exception_stub_25:
    push 0
    push 25
    jmp exception_common


; ============================================================
; Exception 26 - Reserved
; ============================================================

exception_stub_26:
    push 0
    push 26
    jmp exception_common


; ============================================================
; Exception 27 - Reserved
; ============================================================

exception_stub_27:
    push 0
    push 27
    jmp exception_common


; ============================================================
; Exception 28 - Hypervisor Injection Exception (#HV)
; ============================================================

exception_stub_28:
    push 0
    push 28
    jmp exception_common


; ============================================================
; Exception 29 - VMM Communication Exception (#VC)
; ============================================================

exception_stub_29:
    push 0
    push 29
    jmp exception_common


; ============================================================
; Exception 30 - Security Exception (#SX)
; ============================================================

exception_stub_30:
    push 0
    push 30
    jmp exception_common


; ============================================================
; Exception 31 - Reserved
; ============================================================

exception_stub_31:
    push 0
    push 31
    jmp exception_common

; ============================================================
; Hardware IRQ common handler
;
; IRQ CPU frame before our pushes:
;
;   [rsp + 0]  = vector
;   [rsp + 8]  = RIP
;   [rsp + 16] = CS
;   [rsp + 24] = RFLAGS
;
; Unlike CPU exceptions, hardware IRQs do not provide
; an error-code slot.
; ============================================================

global irq_stub_0
global irq_stub_1
global irq_stub_2
global irq_stub_3
global irq_stub_4
global irq_stub_5
global irq_stub_6
global irq_stub_7
global irq_stub_8
global irq_stub_9
global irq_stub_10
global irq_stub_11
global irq_stub_12
global irq_stub_13
global irq_stub_14
global irq_stub_15
global lapic_timer_stub
global lapic_spurious_stub

extern irq_dispatch
extern lapic_timer_interrupt
extern lapic_eoi
extern x86_64_preempt_handle_timer
extern x86_64_preempt_restore_and_iret
extern x86_64_preempt_restore_context_and_resume


irq_common:
    cld

    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    mov rbx, rsp
    and rsp, -16

    call irq_dispatch

    mov rsp, rbx

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

    add rsp, 8

    iretq


; ============================================================
; IRQ 0-15
; ============================================================

irq_stub_0:
    push 32
    jmp irq_common

irq_stub_1:
    push 33
    jmp irq_common

irq_stub_2:
    push 34
    jmp irq_common

irq_stub_3:
    push 35
    jmp irq_common

irq_stub_4:
    push 36
    jmp irq_common

irq_stub_5:
    push 37
    jmp irq_common

irq_stub_6:
    push 38
    jmp irq_common

irq_stub_7:
    push 39
    jmp irq_common

irq_stub_8:
    push 40
    jmp irq_common

irq_stub_9:
    push 41
    jmp irq_common

irq_stub_10:
    push 42
    jmp irq_common

irq_stub_11:
    push 43
    jmp irq_common

irq_stub_12:
    push 44
    jmp irq_common

irq_stub_13:
    push 45
    jmp irq_common

irq_stub_14:
    push 46
    jmp irq_common

irq_stub_15:
    push 47
    jmp irq_common


; ============================================================
; LAPIC timer interrupt
;
; Vector 0xF0 is a Local APIC timer vector.
; It is deliberately kept outside the external IRQ
; dispatcher range (0x20-0x2F).
;
; CPU frame before our pushes:
;
;   [rsp + 0]  = RIP
;   [rsp + 8]  = CS
;   [rsp + 16] = RFLAGS
;
; ============================================================

lapic_timer_stub:
    cld

; CPU provides:
;   RIP -> CS -> RFLAGS
;
; Push the synthetic LAPIC timer vector first.
; After saving the GPRs, memory exactly matches
; struct irq_frame:
;
;   r15..rax -> vector -> RIP -> CS -> RFLAGS

    push 0xF0

    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    mov rbx, rsp
    and rsp, -16

; Reserve storage for the typed architecture resume target.
;
; struct x86_64_resume_target:
;   offset 0 = kind
;   offset 8 = context/frame pointer
;   size     = 16
; Keep the SysV AMD64 call-site stack alignment correct.
;
; After `and rsp, -16`, reserve 24 bytes:
;   rsp + 0  .. 7   = alignment padding
;   rsp + 8  .. 23  = struct x86_64_resume_target
;
; Therefore RSP is 8 mod 16 at each subsequent CALL site,
; as required by the SysV AMD64 ABI.
    sub rsp, 24

; First perform the LAPIC clock/timer accounting.
; This preserves the existing LAPIC -> clock-event
; architecture.
    call lapic_timer_interrupt

; The timer frame is still the live interrupted state.
; The architecture preemption bridge binds it to the
; current task and asks the scheduler for the next owner.
;
; Arguments:
;   RDI = live interrupt frame
;   RSI = typed resume-target storage
    mov rdi, rbx
    lea rsi, [rsp + 8]
    call x86_64_preempt_handle_timer

; Preserve the handler status across LAPIC EOI.
;
; R14 is callee-saved by the SysV AMD64 ABI and is already
; part of the eventual CPU-state restoration path.
    mov r14d, eax

; Snapshot the typed target before LAPIC EOI.
;
; r12 = target kind
; r13 = target context/frame pointer
;
; r12/r13 are callee-saved across lapic_eoi() and are
; restored from the final selected continuation before
; execution resumes.
    mov r12d, [rsp + 8]
    mov r13,  [rsp + 16]

; Complete the Local APIC interrupt while the temporary
; aligned stack remains active. The live interrupt frame
; must never become the CALL stack.
    call lapic_eoi

; The original interrupt-frame stack is restored only after
; the EOI call has returned and will no longer push a return
; address.
    mov rsp, rbx

; A handler error is treated as "continue the interrupted
; task". The live frame remains authoritative in that case.
    test r14d, r14d
    js .timer_fallback

; Dispatch according to the explicit architecture target kind.
    cmp r12d, 1                  ; X86_64_RESUME_CONTEXT
    je .timer_resume_context

    cmp r12d, 2                  ; X86_64_RESUME_INTERRUPT
    je .timer_resume_interrupt

.timer_fallback:
    mov rdi, rbx
    jmp x86_64_preempt_restore_and_iret

.timer_resume_context:
    mov rdi, r13
    jmp x86_64_preempt_restore_context_and_resume

.timer_resume_interrupt:
    mov rdi, r13
    jmp x86_64_preempt_restore_and_iret

; ============================================================
; LAPIC spurious interrupt
; ============================================================
;
; Vector 0xFF belongs to the Local APIC, not the legacy PIC.
;
; Therefore this handler:
;   - does not enter irq_common
;   - does not call irq_dispatch
;   - does not issue a PIC EOI
;   - does not modify general-purpose registers
;
; The CPU has already pushed the interrupt return frame,
; so a direct iretq is sufficient.
;
; ============================================================

lapic_spurious_stub:
    iretq
