#include <sys/syscall.h>

long (*syscall[NUM_SYSCALLS])();
/*
a7 寄存器存放系统调用号，区分是哪个 Syscall
a0-a5 寄存器依次用来表示 Syscall 编程接口中定义的参数
*/
void handle_syscall(regs_context_t *regs, uint64_t interrupt, uint64_t cause)
{
    /* TODO: [p2-task3] handle syscall exception */
    /**
     * HINT: call syscall function like syscall[fn](arg0, arg1, arg2),
     * and pay attention to the return value and sepc
     */
    regs->sepc = regs->sepc + 4; 
    regs->regs[10] = syscall[regs->regs[17]](regs->regs[10],regs->regs[11],regs->regs[12]);
}
