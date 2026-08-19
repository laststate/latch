ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 2097152
    RAM   (rwx) : ORIGIN = 0x10060000, LENGTH = 65536
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
