; =============================================================
; VUKANG OS - kernel/arch/usermode.asm
; enter_usermode(entry, user_stack): drop from ring 0 to ring 3.
; usermode_return(code): called from the exit syscall to resume the
; kernel exactly where enter_usermode was called (like longjmp).
; =============================================================
global enter_usermode
global usermode_return
global saved_kernel_esp

section .bss
saved_kernel_esp: resd 1

section .text
enter_usermode:
    push ebp
    push ebx
    push esi
    push edi
    mov [saved_kernel_esp], esp
    mov ecx, [esp+20]        ; entry
    mov edx, [esp+24]        ; user stack
    mov ax, 0x23             ; user data selector (RPL 3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push dword 0x23          ; ss
    push edx                 ; esp
    pushfd
    pop eax
    or eax, 0x200            ; interrupts on in user mode
    push eax                 ; eflags
    push dword 0x1B          ; cs (user code, RPL 3)
    push ecx                 ; eip
    iret

usermode_return:
    mov eax, [esp+4]         ; exit code
    mov esp, [saved_kernel_esp]
    mov cx, 0x10
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
