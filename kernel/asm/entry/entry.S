.section .text
.global _start

_start:
    cli

    # Enable SSE
    mov %cr0, %rax
    and $0xFFFB, %ax    # Clear CR0.EM (bit 2)
    or $0x2, %ax        # Set CR0.MP (bit 1)
    mov %rax, %cr0
    
    mov %cr4, %rax
    or $0x600, %ax      # Set CR4.OSFXSR (bit 9) and CR4.OSXMMEXCPT (bit 10)
    mov %rax, %cr4

    # Fix ABI: UEFI (MS x64) -> Kernel (SysV)
    # MS Arg1: RCX -> SysV Arg1: RDI (Framebuffer*)
    # MS Arg2: RDX -> SysV Arg2: RSI (MemoryMap*)
    # MS Arg3: R8  -> SysV Arg3: RDX (MapSize)
    # MS Arg4: R9  -> SysV Arg4: RCX (DescSize)
    # MS Arg5: [RSP+40] -> SysV Arg5: R8 (boot_flags)
    
    mov 40(%rsp), %rax  # Capture 5th arg (boot_flags) before stack change
    mov %rcx, %rdi
    mov %rdx, %rsi
    mov %r8, %rdx
    mov %r9, %rcx
    mov %rax, %r8
    
    # Set up stack pointer
    mov $stack_top, %rsp
    
    # Call kernel with framebuffer pointer in RDI (first argument)
    call kmain
    hlt

.section .note.GNU-stack,"",@progbits
.section .bss
.align 16
stack_bottom:
.space 0x10000
.align 16
stack_top:

.section .note.GNU-stack,"",@progbits
