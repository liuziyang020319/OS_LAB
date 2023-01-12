#include <asm.h>
#include <csr.h>
#include <asm/regs.h>

.macro SAVE_CONTEXT
  .local _KERNAL_MODE
  .local _USER_MODE
  /* TODO: [p2-task3] save all general purpose registers here! */
  /* HINT: Pay attention to the function of tp and sp, and save them carefully! */
  // 用户态栈不安全
  // 在用户态，sscratch 保存内核栈的地址；在内核态，sscratch 的值为 0。
  csrrw tp, CSR_SSCRATCH, tp
  bnez  tp, _USER_MODE
_KERNAL_MODE:
  csrr tp, CSR_SSCRATCH
  sd sp, PCB_KERNEL_SP(tp)   
_USER_MODE:
  sd sp, PCB_USER_SP(tp)  
  ld sp, PCB_KERNEL_SP(tp)
  addi sp, sp, SWITCH_TO_SIZE
  
  sd ra, OFFSET_REG_RA(sp)
  sd gp, OFFSET_REG_GP(sp)
  sd t0, OFFSET_REG_T0(sp)
  sd t1, OFFSET_REG_T1(sp)
  sd t2, OFFSET_REG_T2(sp)
  sd s0, OFFSET_REG_S0(sp)
  sd s1, OFFSET_REG_S1(sp)
  sd a0, OFFSET_REG_A0(sp)
  sd a1, OFFSET_REG_A1(sp)
  sd a2, OFFSET_REG_A2(sp)
  sd a3, OFFSET_REG_A3(sp)
  sd a4, OFFSET_REG_A4(sp)
  sd a5, OFFSET_REG_A5(sp)
  sd a6, OFFSET_REG_A6(sp)
  sd a7, OFFSET_REG_A7(sp)
  sd s2, OFFSET_REG_S2(sp)
  sd s3, OFFSET_REG_S3(sp)
  sd s4, OFFSET_REG_S4(sp)
  sd s5, OFFSET_REG_S5(sp)
  sd s6, OFFSET_REG_S6(sp)
  sd s7, OFFSET_REG_S7(sp)
  sd s8, OFFSET_REG_S8(sp)
  sd s9, OFFSET_REG_S9(sp)
  sd s10, OFFSET_REG_S10(sp)
  sd s11, OFFSET_REG_S11(sp)
  sd t3, OFFSET_REG_T3(sp)
  sd t4, OFFSET_REG_T4(sp)
  sd t5, OFFSET_REG_T5(sp)
  sd t6, OFFSET_REG_T6(sp)

  ld s0, PCB_USER_SP(tp)
  sd s0, OFFSET_REG_SP(sp)

  csrr s1, CSR_SSCRATCH
  sd s1, OFFSET_REG_TP(sp)
  /*
   * Disable user-mode memory access as it should only be set in the
   * actual user copy routines.
   *
   * Disable the FPU to detect illegal usage of floating point in kernel
   * space.
   */
  li t0, SR_SUM | SR_FS
  csrrc s2, CSR_SSTATUS, t0

  /* TODO: [p2-task3] save sstatus, sepc, stval and scause on kernel stack */
  sd s2, OFFSET_REG_SSTATUS(sp)
  
  csrr s3, CSR_SEPC
  sd s3, OFFSET_REG_SEPC(sp)

  csrr s4, CSR_STVAL
  sd s4, OFFSET_REG_SBADADDR(sp)

  csrr s5, CSR_SCAUSE
  sd s5, OFFSET_REG_SCAUSE(sp)

  addi sp, sp, -(SWITCH_TO_SIZE)
.endm

.macro RESTORE_CONTEXT
  /* TODO: Restore all general purpose registers and sepc, sstatus */
  /* HINT: Pay attention to sp again! */
  ld sp, PCB_KERNEL_SP(tp)
  addi sp, sp, SWITCH_TO_SIZE

  ld s0, OFFSET_REG_SEPC(sp)
  csrw CSR_SEPC, s0 

  ld s1, OFFSET_REG_SSTATUS(sp)
  csrw CSR_SSTATUS, s1

  ld ra, OFFSET_REG_RA(sp)
  ld gp, OFFSET_REG_GP(sp)
  ld tp, OFFSET_REG_TP(sp)
  ld t0, OFFSET_REG_T0(sp)
  ld t1, OFFSET_REG_T1(sp)
  ld t2, OFFSET_REG_T2(sp)
  ld s0, OFFSET_REG_S0(sp)
  ld s1, OFFSET_REG_S1(sp)
  ld a0, OFFSET_REG_A0(sp)
  ld a1, OFFSET_REG_A1(sp)
  ld a2, OFFSET_REG_A2(sp)
  ld a3, OFFSET_REG_A3(sp)
  ld a4, OFFSET_REG_A4(sp)
  ld a5, OFFSET_REG_A5(sp)
  ld a6, OFFSET_REG_A6(sp)
  ld a7, OFFSET_REG_A7(sp)
  ld s2, OFFSET_REG_S2(sp)
  ld s3, OFFSET_REG_S3(sp)
  ld s4, OFFSET_REG_S4(sp)
  ld s5, OFFSET_REG_S5(sp)
  ld s6, OFFSET_REG_S6(sp)
  ld s7, OFFSET_REG_S7(sp)
  ld s8, OFFSET_REG_S8(sp)
  ld s9, OFFSET_REG_S9(sp)
  ld s10, OFFSET_REG_S10(sp)
  ld s11, OFFSET_REG_S11(sp)
  ld t3, OFFSET_REG_T3(sp)
  ld t4, OFFSET_REG_T4(sp)
  ld t5, OFFSET_REG_T5(sp)
  ld t6, OFFSET_REG_T6(sp)  
  
  ld sp, OFFSET_REG_SP(sp)
.endm

ENTRY(enable_preempt)
  not t0, x0
  csrs CSR_SIE, t0
  jr ra
ENDPROC(enable_preempt)

ENTRY(disable_preempt)
  csrw CSR_SIE, zero
  jr ra
ENDPROC(disable_preempt)

ENTRY(enable_interrupt)
  li t0, SR_SIE
  csrs CSR_SSTATUS, t0
  jr ra
ENDPROC(enable_interrupt)

ENTRY(disable_interrupt)
  li t0, SR_SIE
  csrc CSR_SSTATUS, t0
  jr ra
ENDPROC(disable_interrupt)

// NOTE: the address of previous pcb in a0
// NOTE: the address of next pcb in a1
ENTRY(switch_to)
  # addi sp, sp, -(SWITCH_TO_SIZE)

  /* TODO: [p2-task1] save all callee save registers on kernel stack,
   * see the definition of `struct switchto_context` in sched.h*/
  mv tp, a0
  ld tp,  PCB_KERNEL_SP(tp)
  sd ra,  SWITCH_TO_RA(tp)
  sd sp,  SWITCH_TO_SP(tp)
  sd s0,  SWITCH_TO_S0(tp)
  sd s1,  SWITCH_TO_S1(tp)
  sd s2,  SWITCH_TO_S2(tp)
  sd s3,  SWITCH_TO_S3(tp)
  sd s4,  SWITCH_TO_S4(tp)
  sd s5,  SWITCH_TO_S5(tp)
  sd s6,  SWITCH_TO_S6(tp)
  sd s7,  SWITCH_TO_S7(tp)
  sd s8,  SWITCH_TO_S8(tp)
  sd s9,  SWITCH_TO_S9(tp)
  sd s10, SWITCH_TO_S10(tp)
  sd s11, SWITCH_TO_S11(tp)

  /* TODO: [p2-task1] restore all callee save registers from kernel stack,
   * see the definition of `struct switchto_context` in sched.h*/
  
  mv tp, a1
  ld tp,  PCB_KERNEL_SP(tp)
  ld ra,  SWITCH_TO_RA(tp)
  ld sp,  SWITCH_TO_SP(tp)
  ld s0,  SWITCH_TO_S0(tp)
  ld s1,  SWITCH_TO_S1(tp)
  ld s2,  SWITCH_TO_S2(tp)
  ld s3,  SWITCH_TO_S3(tp)
  ld s4,  SWITCH_TO_S4(tp)
  ld s5,  SWITCH_TO_S5(tp)
  ld s6,  SWITCH_TO_S6(tp)
  ld s7,  SWITCH_TO_S7(tp)
  ld s8,  SWITCH_TO_S8(tp)
  ld s9,  SWITCH_TO_S9(tp)
  ld s10, SWITCH_TO_S10(tp)
  ld s11, SWITCH_TO_S11(tp)
  # addi sp, sp, SWITCH_TO_SIZE

  mv tp, a1
  csrw CSR_SSCRATCH, tp

  li t0, SR_SUM
  csrs CSR_SSTATUS, t0

  jr ra
ENDPROC(switch_to)

ENTRY(ret_from_exception)
  /* TODO: [p2-task3] restore context via provided macro and return to sepc */
  call unlock_kernel


  RESTORE_CONTEXT

  csrw CSR_SSCRATCH, tp

  li t0, SR_SUM
  csrs CSR_SSTATUS, t0

  sret

  /* HINT: remember to check your sp, does it point to the right address? */
ENDPROC(ret_from_exception)

//regs_context_t *regs, uint64_t stval, uint64_t scause

ENTRY(exception_handler_entry)

  /* TODO: [p2-task3] save context via the provided macro */
  # csrr x0, sscratch
  
  SAVE_CONTEXT

  call lock_kernel

  csrw CSR_SSCRATCH, x0

  li t0, SR_SUM
  csrs CSR_SSTATUS, t0  

  /* TODO: [p2-task3] load ret_from_exception into $ra so that we can return to
   * ret_from_exception when interrupt_help complete.
   */
  
  la ra, ret_from_exception

  /* TODO: [p2-task3] call interrupt_helper
   * NOTE: don't forget to pass parameters for it.
   */

  ld a0, PCB_KERNEL_SP(tp)
  addi a0, a0, SWITCH_TO_SIZE
  csrr a1, CSR_STVAL
  csrr a2, CSR_SCAUSE
  call x0, interrupt_helper

ENDPROC(exception_handler_entry)

ENTRY(clear_sip)
  csrw CSR_SIP, zero
  jr ra
ENDPROC(clear_sip)