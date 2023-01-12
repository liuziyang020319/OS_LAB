#include <asm.h>

.section ".entry_function","ax"
ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */
    la t0, __bss_start
    la t1, __BSS_END__
    bge t0, t1, done
bss_clr_loop:
    sw zero, 0(t0)
    addi t0, t0, 4
    blt t0, t1, bss_clr_loop # if t0 < t1 than bss_clr_loop

done:
    addi sp, sp, -8         # 分配空间
    sd ra, 0(sp)          # 储存返回地址

    /* TODO: [p1-task3] enter main function */

    j main

    /* TODO: [p1-task3] finish task and return to the kernel */
    /* NOTE: You need to replace this with new mechanism in p3-task2! */
    ld ra, 0(sp)                    # 从栈顶加载返回值，
    addi sp, sp, 8                  # 弹出栈顶的返回值
    ret

// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
