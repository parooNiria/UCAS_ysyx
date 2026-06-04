ENTRY(_start)

MEMORY {
  FLASH (rx) : ORIGIN = 0x30000000, LENGTH = 0x400000
  SRAM (rwx) : ORIGIN = 0x80000000, LENGTH = 0x200000
}

SECTIONS {
  . = ORIGIN(FLASH);
  .text : {
    *(entry)
    *(.text*)
  } > FLASH
  etext = .;
  _etext = .;

  .rodata : ALIGN(4) {
    *(.rodata*)
    *(.srodata*)
  } > FLASH

  .data : ALIGN(4) {
    _sdata = .;
    *(.data*)
    *(.sdata*)
    . = ALIGN(4);
    _edata = .;
  } > SRAM AT> FLASH
  _sidata = LOADADDR(.data);

  .bss : ALIGN(4) {
    _bss_start = .;
    _sbss = .;
    *(.bss*)
    *(.sbss*)
    *(.scommon)
    . = ALIGN(4);
    _ebss = .;
  } > SRAM

  _stack_top = ORIGIN(SRAM) + LENGTH(SRAM);
  _stack_pointer = _stack_top;
  end = .;
  _end = .;
  _heap_start = ALIGN(0x1000);
}