#include <asm.h>
#include <csr.h>

ENTRY(setup_exception)

  /* TODO: [p2-task3] save exception_handler_entry into STVEC */
  la s0, exception_handler_entry
  csrw CSR_STVEC, s0
  /* TODO: [p2-task4] enable interrupts globally */

  li t0, SR_SIE
  csrw CSR_SSTATUS, t0

  jr ra
ENDPROC(setup_exception)
