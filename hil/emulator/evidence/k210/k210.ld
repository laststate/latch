ENTRY(_start)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x80000000, LENGTH = 1048576
    RAM   (rwx) : ORIGIN = 0x80000000, LENGTH = 2097152
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
