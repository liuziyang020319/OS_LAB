/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK0		0x50500000
#define KERNEL_STACK1   0x504ff000


.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero //control and status register set ,把 CSR 中 x0 指定的 bit 置 1  0x104
  csrw CSR_SIP, zero //0x144

  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  
  csrr t0, CSR_MHARTID
  bnez t0, secondary

  la s0, __bss_start //start of bss
  la s1, __BSS_END__ //end
  bge s0, s1, clear_bss_done # if s0 >= s1 then target

clear_bss:
  sw zero, 0(s0)
  addi s0, s0, 4; # s0 = s0 + 4
  blt s0, s1, clear_bss # if s0 < s1 then loop
clear_bss_done:  

  la tp, pid0_pcb_0
  li sp, KERNEL_STACK0
  j done
  /* TODO: [p1-task2] setup C environment */

secondary:
  la tp, pid0_pcb_1
  li sp, KERNEL_STACK1

done: 

  csrw CSR_SSCRATCH, x0

  call main  # jump to main

END(_start)
