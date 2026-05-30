ENTRY(_start)

MEMORY {
  MROM (rx)  : ORIGIN = 0x20000000, LENGTH = 0x1000
  SRAM (rwx) : ORIGIN = 0x0f000000, LENGTH = 0x2000
}

SECTIONS {
  . = ORIGIN(MROM);
  .text : {
    *(entry)
    *(.text*)
  } > MROM
  etext = .;
  _etext = .;

  .rodata : ALIGN(4) {
    *(.rodata*)
    *(.srodata*)
  } > MROM

  .data : ALIGN(4) {
    _sdata = .;
    *(.data*)
    *(.sdata*)
    . = ALIGN(4);
    _edata = .;
  } > SRAM AT> MROM
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