.global gdt_flush

gdt_flush:
    # Load GDT
    lgdt (%rdi)

    # Reload data segments
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    # Reload code segment
    pushq $0x08
    leaq .reload_cs(%rip), %rax
    pushq %rax
    lretq

.reload_cs:
    ret

.section .note.GNU-stack,"",@progbits

.section .note.GNU-stack,"",@progbits
