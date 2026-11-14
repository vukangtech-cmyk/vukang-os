; =============================================================
; VUKANG OS - kernel/arch/isr.asm
; CPU exception (0-31), hardware IRQ (32-47) and syscall (0x80)
; entry stubs. All share isr_common which calls the C dispatcher.
; The dispatcher RETURNS the stack pointer to resume, which is how
; the scheduler switches tasks: it just hands back another task's ESP.
; =============================================================
extern interrupt_dispatch

%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp isr_common
%endmacro
%macro ISR_ERR 1
global isr%1
isr%1:
    push dword %1
    jmp isr_common
%endmacro

%assign i 0
%rep 32
  %if i = 8 || (i >= 10 && i <= 14) || i = 17 || i = 21 || i = 29 || i = 30
    ISR_ERR i
  %else
    ISR_NOERR i
  %endif
  %assign i i+1
%endrep
%assign i 32
%rep 16
    ISR_NOERR i
  %assign i i+1
%endrep
ISR_NOERR 128

isr_common:
    pusha
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    cld
    push esp                 ; regs_t *
    call interrupt_dispatch
    mov esp, eax             ; possibly a different task's stack
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8               ; int_no + err_code
    iret

; table of stub addresses for idt.c
section .data
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 48
    dd isr %+ i
  %assign i i+1
%endrep
