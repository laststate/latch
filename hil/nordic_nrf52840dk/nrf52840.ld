ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 1024K
  RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 256K
}

SECTIONS
{
  .vectors :
  {
    KEEP(*(.vectors))
  } > FLASH

  .text :
  {
    *(.text*)
    *(.rodata*)
  } > FLASH

  .data : AT (ADDR(.text) + SIZEOF(.text))
  {
    *(.data*)
  } > RAM

  .bss (NOLOAD) :
  {
    __bss_start__ = .;
    *(.bss*)
    *(COMMON)
    __bss_end__ = .;
  } > RAM

  . = ALIGN(8);
  __stack_top__ = ORIGIN(RAM) + LENGTH(RAM);
}
