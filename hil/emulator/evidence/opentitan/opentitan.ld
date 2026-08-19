ENTRY(_start)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00008000, LENGTH = 32768
    RAM   (rwx) : ORIGIN = 0x10000000, LENGTH = 131072
}

SECTIONS
{
    .start : { KEEP(*(.start)) } > FLASH
    .text : { *(.text*) *(.rodata*) } > FLASH
    .data : { *(.data*) } > RAM AT > FLASH
    .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
    . = ALIGN(16);
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
