ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00400000, LENGTH = 4194304
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 131072
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
