/* user_linker.ld - minimal user linker script placing program at 0x80200000 */
ENTRY(_start)

SECTIONS
{
    /* set location counter to program load address */
    . = 0x80200000;

    .text : {
        KEEP(*(.text))
        KEEP(*(.text.*))
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        *(.data)
        *(.data.*)
    }

    .bss : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /* provide symbol for end if helpful */
    _end = .;
}

