.section .text
.global _start
.extern kernel_main

_start:
    cli
    
    # Set up a basic stack using RIP-relative address to stay in physical memory
    lea stack_top(%rip), %rsp
    
    # Bridge ABI difference: UEFI (MS ABI) passes 1st arg in RCX,
    # but Kernel (SysV ABI) expects it in RDI.
    mov %rcx, %rdi
    
    # Call the C entry point using RIP-relative call
    call kernel_main

    # Should not return
halt:
    hlt
    jmp halt

.section .bss
.align 16
stack_bottom:
    .skip 16384 # 16KB stack
stack_top:

.section .note.GNU-stack,"",@progbits