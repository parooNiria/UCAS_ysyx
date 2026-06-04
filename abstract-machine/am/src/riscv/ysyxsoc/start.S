.section entry, "ax"
.globl _start
.type _start, @function

_start:
  mv s0, zero
  la sp, _stack_pointer

  /* ---- FSBL: Copy .ssbl from Flash (LMA) to ISRAM (VMA) ----
   * Only copies the small SSBL (~200 bytes), runs in a few iterations. */
  la t0, _ssbl_lma
  la t1, _ssbl_start
  la t2, _ssbl_end
0:
  beq t1, t2, 1f
  lw a0, 0(t0)
  sw a0, 0(t1)
  addi t0, t0, 4
  addi t1, t1, 4
  j 0b

1:
  /* Jump to SSBL in ISRAM (fast on-chip SRAM at 0x0f000000) */
  la t0, _ssbl_entry
  jr t0
.size _start, . - _start


.section .ssbl, "ax"
.globl _ssbl_entry
.type _ssbl_entry, @function

_ssbl_entry:
  /* ---- SSBL: Running from fast ISRAM ----
   * Copies .text + .data from Flash to PSRAM, then zeroes .bss.
   * ISRAM is much faster than Flash, so the copy loop runs at full speed. */

  /* --- Copy .text from Flash (LMA) to PSRAM (VMA) ---
   *   .weak _stext_lma: undefined → 0 → skip (XIP link scripts)
   *   defined (psram-run.ld) → flash address → do the copy. */
  .weak _stext_lma
  la t0, _stext_lma
  beqz t0, 3f
  la t1, _stext
  la t2, _etext
2:
  beq t1, t2, 3f
  lw a0, 0(t0)
  sw a0, 0(t1)
  addi t0, t0, 4
  addi t1, t1, 4
  j 2b

  /* --- Copy .data from Flash to PSRAM --- */
3:
  la t0, _sidata
  la t1, _sdata
  la t2, _edata
4:
  beq t1, t2, 5f
  lw a0, 0(t0)
  sw a0, 0(t1)
  addi t0, t0, 4
  addi t1, t1, 4
  j 4b

  /* --- Zero .bss --- */
5:
  la t1, _sbss
  la t2, _ebss
6:
  beq t1, t2, 7f
  sw zero, 0(t1)
  addi t1, t1, 4
  j 6b

7:
  call _trm_init

.size _ssbl_entry, . - _ssbl_entry