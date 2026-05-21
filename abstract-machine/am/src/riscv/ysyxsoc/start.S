.section entry, "ax"
.globl _start
.type _start, @function

_start:
  mv s0, zero
  la sp, _stack_pointer

  la t0, _sidata
  la t1, _sdata
  la t2, _edata
1:
  beq t1, t2, 2f
  lw a0, 0(t0)
  sw a0, 0(t1)
  addi t0, t0, 4
  addi t1, t1, 4
  j 1b

2:
  la t1, _sbss
  la t2, _ebss
3:
  beq t1, t2, 4f
  sw zero, 0(t1)
  addi t1, t1, 4
  j 3b

4:
  call _trm_init

.size _start, . - _start