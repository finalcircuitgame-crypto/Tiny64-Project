.extern isr_handler
.section .text
.altmacro

.macro isr_err_stub nr
isr_stub_\nr:
    pushq $\nr
    jmp isr_common_stub
.endm

.macro isr_no_err_stub nr
isr_stub_\nr:
    pushq $0
    pushq $\nr
    jmp isr_common_stub
.endm

isr_common_stub:
    push %rax
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %r8
    push %r9
    push %r10
    push %r11
    
    # Vector is at 72(%rsp) because we pushed 9 64-bit registers
    mov 72(%rsp), %rdi
    
    # Call the C handler
    call isr_handler
    
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rax
    add $16, %rsp # Clean up vector and error code
    iretq

# Helper macros to force expansion of the loop counter
.macro make_isr_no_err nr
    isr_no_err_stub %nr
.endm

.macro make_isr_err nr
    isr_err_stub %nr
.endm

# Define first 32 stubs (Exceptions)
make_isr_no_err 0
make_isr_no_err 1
make_isr_no_err 2
make_isr_no_err 3
make_isr_no_err 4
make_isr_no_err 5
make_isr_no_err 6
make_isr_no_err 7
make_isr_err    8
make_isr_no_err 9
make_isr_err    10
make_isr_err    11
make_isr_err    12
make_isr_err    13
make_isr_err    14
make_isr_no_err 15
make_isr_no_err 16
make_isr_err    17
make_isr_no_err 18
make_isr_no_err 19
make_isr_no_err 20
make_isr_no_err 21
make_isr_no_err 22
make_isr_no_err 23
make_isr_no_err 24
make_isr_no_err 25
make_isr_no_err 26
make_isr_no_err 27
make_isr_no_err 28
make_isr_no_err 29
make_isr_err    30
make_isr_no_err 31

# IRQs and other interrupts (32-255)
.set i, 32
.rept 224
    make_isr_no_err i
    .set i, i + 1
.endr

.section .data
.global isr_stub_table

.macro make_table_entry nr
    .quad isr_stub_\nr
.endm

isr_stub_table:
.set i, 0
.rept 256
    make_table_entry %i
    .set i, i + 1
.endr

.section .note.GNU-stack,"",@progbits