BITS 64

section .text

global x86_64_task_bootstrap_trampoline
extern task_bootstrap_entry

; Initial task-context entry trampoline.
;
; RSP points at the bootstrap return slot.
; [RSP + 8] contains the explicit struct task *.
;
; The task pointer is loaded into RDI, which is the first
; SysV AMD64 argument register, and execution enters the C
; task bootstrap without introducing another call frame.

x86_64_task_bootstrap_trampoline:
    mov rdi, [rsp + 8]
    jmp task_bootstrap_entry
