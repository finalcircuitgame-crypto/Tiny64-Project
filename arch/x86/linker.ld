ENTRY(_start)

SECTIONS
{
    /* Kernel starts at 1MB in physical memory and is identity mapped */
    . = 0x100000;

    .text : ALIGN(4096)
    {
        *(.text)
        *(.text.*)
    }

    .rodata : ALIGN(4096)
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data : ALIGN(4096)
    {
        *(.data)
        *(.data.*)
    }

    .bss : ALIGN(4096)
    {
        *(.bss)
        *(COMMON)
    }

    /DISCARD/ :
    {
        *(.note.GNU-stack)
        *(.comment)
    }
}
