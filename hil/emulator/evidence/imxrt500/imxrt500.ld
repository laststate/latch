ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 5242880
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 1048576
}

SECTIONS
{
    .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
    .text : { *(.text*) *(.rodata*) } > FLASH
    .data : { *(.data*) } > RAM AT > FLASH
    .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
    . = ALIGN(8);
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
