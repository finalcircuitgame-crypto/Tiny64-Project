.extern mouse_handler
.extern keyboard_handler
.extern tick_handler

.extern exception_handler

.macro ISR_NOERR n
    .global isr\n
isr\n:
    pushq $0
    pushq $\n
    mov %rsp, %rdi
    call exception_handler

1:
    cli
    hlt
    jmp 1b
.endm

.macro ISR_ERR n
    .global isr\n
isr\n:
    pushq $\n

    mov %rsp, %rdi
    call exception_handler

1:
    cli
    hlt
    jmp 1b
.endm

.global isr32
.global isr33
.global isr44

// CPU exceptions (0-31)
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

isr32: # IRQ 0 - Timer
    push %rax
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15
    push %r8
    push %r9
    push %r10
    push %r11
    call tick_handler
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rax
    iretq

isr33: # IRQ 1 - Keyboard
    push %rax
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15
    push %r8
    push %r9
    push %r10
    push %r11
    call keyboard_handler
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rax
    iretq

isr44: # IRQ 12 - Mouse
    push %rax
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15
    push %r8
    push %r9
    push %r10
    push %r11
    call mouse_handler
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rax
    iretq

.section .note.GNU-stack,"",@progbits

.section .note.GNU-stack,"",@progbits
