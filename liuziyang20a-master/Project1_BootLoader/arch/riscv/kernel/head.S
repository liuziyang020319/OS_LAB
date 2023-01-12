/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50500000

.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero //control and status register set ,把 CSR 中 x0 指定的 bit 置 1  0x104
  csrw CSR_SIP, zero //0x144

  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  
  la s0, __bss_start //start of bss
  la s1, __BSS_END__ //end
  bge s0, s1, end # if s0 >= s1 then target

loop:
  sw zero, 0(s0)
  addi s0, s0, 4; # s0 = s0 + 4
  blt s0, s1, loop # if s0 < s1 then loop
  

  /* TODO: [p1-task2] setup C environment */

end: 
  li sp, KERNEL_STACK # sp = KERNEL_STACK 
  j main  # jump to main

END(_start)
