ENTRY(_start)

SECTIONS
{
    . = 0;
    .text : {
        *(.text._start)
        *(.text*)
    }
    .data : { *(.data*) }
    .rodata : { *(.rodata*) }
    .bss : { *(.bss*) }

    /DISCARD/ : {
        *(.pdata*)
        *(.xdata*)
        *(.comment)
        *(.note*)
    }
}
