; =============================================================
;  VUKANG OS - boot/boot.asm
;  Multiboot entry point. GRUB loads the kernel at 1 MiB and
;  jumps here in 32-bit protected mode.
; =============================================================
MB_MAGIC    equ 0x1BADB002
MB_FLAGS    equ (1 << 0) | (1 << 1)      ; align modules + memory map
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

section .bss
align 16
stack_bottom:
    resb 32768                           ; 32 KiB boot/kernel stack
stack_top:

section .text
global _start
global kernel_stack_top
extern kmain

_start:
    cli
    mov esp, stack_top
    xor ebp, ebp
    push ebx                             ; multiboot info pointer
    push eax                             ; multiboot magic
    call kmain
.hang:
    cli
    hlt
    jmp .hang

kernel_stack_top equ stack_top
