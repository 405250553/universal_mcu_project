/* STM32F407 minimal linker script */

MEMORY
{
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 1024K
  RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 128K
}

/* 堆疊頂端 */
_estack = ORIGIN(RAM) + LENGTH(RAM);

/* 初始化符號，startup.s 會用到 */
_sdata = ORIGIN(RAM);       /* data section start */
_edata = _sdata + SIZEOF(.data);
_sidata = LOADADDR(.data);  /* data section load address (FLASH) */

_sbss = _sdata + SIZEOF(.data);  /* bss start */
_ebss = _sbss + SIZEOF(.bss);    /* bss end */

SECTIONS
{
  .isr_vector :
  {
    KEEP(*(.isr_vector))
  } > FLASH

  .text :
  {
    *(.text*)
    *(.rodata*)
  } > FLASH

  .data : AT(ADDR(.text) + SIZEOF(.text))
  {
    _sdata = .;
    *(.data*)
    _edata = .;
  } > RAM

  .bss :
  {
    _sbss = .;
    *(.bss*)
    *(COMMON)
    _ebss = .;
  } > RAM
}
