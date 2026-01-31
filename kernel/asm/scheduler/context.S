.section .text
.global switch_context

// switch_context(uint64_t* old_rsp, uint64_t new_rsp)
switch_context:
    // Save current registers
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    // Save old RSP
    movq %rsp, (%rdi)

    // Load new RSP
    movq %rsi, %rsp

    // Restore registers
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    ret
.section .note.GNU-stack,"",@progbits
