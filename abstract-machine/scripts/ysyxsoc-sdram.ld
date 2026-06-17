ENTRY(_start)

MEMORY {
  FLASH (rx) : ORIGIN = 0x30000000, LENGTH = 0x400000
  ISRAM (rwx) : ORIGIN = 0x0f000000, LENGTH = 0x2000
  SRAM (rwx) : ORIGIN = 0xa0000000, LENGTH = 0x4000000
}

SECTIONS {
  /* ---- Bootloader: runs directly from Flash (advances AT> FLASH counter) ---- */
  .boot : {
    *(entry)
    . = ALIGN(4);
  } > FLASH AT> FLASH

  /* ---- SSBL: VMA in ISRAM (fast on-chip SRAM), LMA auto after .boot in Flash ---- */
  .ssbl : {
    _ssbl_start = .;
    *(.ssbl*)
    . = ALIGN(4);
    _ssbl_end = .;
  } > ISRAM AT> FLASH
  _ssbl_lma = LOADADDR(.ssbl);

  /* ---- .text + .rodata: VMA in SRAM, LMA auto after .ssbl in Flash ---- */
  . = ORIGIN(SRAM);
  _stext = .;
  .text : {
    *(.text*)
    *(.rodata*)
    *(.srodata*)
    . = ALIGN(4);
    _etext = .;
  } > SRAM AT> FLASH
  _stext_lma = LOADADDR(.text);

  /* ---- .data: VMA in SRAM, LMA auto after .text in Flash ---- */
  .data : {
    _sdata = .;
    *(.data*)
    *(.sdata*)
    . = ALIGN(4);
    _edata = .;
  } > SRAM AT> FLASH
  _sidata = LOADADDR(.data);

  /* ---- .bss: VMA in SRAM ---- */
  .bss : ALIGN(4) {
    _bss_start = .;
    _sbss = .;
    *(.bss*)
    *(.sbss*)
    *(.scommon)
    . = ALIGN(4);
    _ebss = .;
  } > SRAM AT> FLASH

  _stack_top = ORIGIN(SRAM) + LENGTH(SRAM);
  _stack_pointer = _stack_top;
  end = .;
  _end = .;
  _heap_start = ALIGN(0x1000);
}
