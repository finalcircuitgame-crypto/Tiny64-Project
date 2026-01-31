ENTRY(efi_main)

SECTIONS
{
    . = 0;

    .text : {
        *(.text*)
    }

    .rdata : {
        *(.rdata*)
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(.bss*)
        *(COMMON)
    }

    .reloc : {
        *(.reloc*)
        LONG(0);
    }
}
