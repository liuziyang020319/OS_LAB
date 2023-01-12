#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/time.h>
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>

extern void ret_from_exception();

#define TASK_NUM_POINT 0x502001fe
#define BASE 0x52000000



int version = 2; // version must between 0 and 9

// Task info array
task_info_t tasks[TASK_MAXNUM];
unsigned long tasknum;

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[QEMU_LOGGING]    = (long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (long (*)())set_timer;
    jmptab[READ_FDT]        = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (long (*)())screen_move_cursor;
    jmptab[PRINT]           = (long (*)())printk;
    jmptab[YIELD]           = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT]      = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ]       = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE]   = (long (*)())do_mutex_lock_release;
}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    unsigned long *OFFSET_POINT = 0x502001f0;
    unsigned long OFFSET = * OFFSET_POINT;
    unsigned long * entryptr = BASE + OFFSET;
    tasknum = *entryptr;
    entryptr += 1;
    for(int idx=0;idx<tasknum;++idx){
        for(int i=0;i<32;++i){
            char * chptr = entryptr;
            char ch = *chptr;
            tasks[idx].name[i]=ch;
            int temp = entryptr;
            temp++;
            entryptr = temp;
        }
        int TASK_ID = *entryptr;
        tasks[idx].TASK_ID=TASK_ID;
        entryptr+=1;
        int TASK_OFFSET = *entryptr;
        tasks[idx].offset=TASK_OFFSET;
        entryptr+=1;
        int TASK_SIZ = *entryptr;
        tasks[idx].SIZE=TASK_SIZ;
        entryptr+=1;
    }
}

/*
        "zero "," ra  "," sp  "," gp  "," tp  ",
        " t0  "," t1  "," t2  ","s0/fp"," s1  ",
        " a0  "," a1  "," a2  "," a3  "," a4  ",
        " a5  "," a6  "," a7  "," s2  "," s3  ",
        " s4  "," s5  "," s6  "," s7  "," s8  ",
        " s9  "," s10 "," s11 "," t3  "," t4  ",
        " t5  "," t6  "
        p30
*/
/*
    MIP
    00 USER
    01 SUPERVISOR
    11 MACHINE
*/

static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
    int i;
     /* TODO: [p2-task3] initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    for(i = 0; i < 32; i++){
        pt_regs->regs[i] = 0;
    }
    pt_regs->regs[2] = (reg_t)user_stack;
    pt_regs->regs[4] = (reg_t)pcb;//thread pointer
    
    //SPIE 异常发生之前的中断使能
    //SPP  发生例外时的权限模式
    pt_regs->sepc     = entry_point;
    pt_regs->sstatus  = (SR_SPIE) & (~SR_SPP);
    pt_regs->sbadaddr = 0;
    pt_regs->scause   = 0;
    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    for(i=0; i < 14; i++){
        pt_switchto->regs[i]=0;
    }
    pt_switchto->regs[0]=(reg_t)&ret_from_exception;
    pt_switchto->regs[1]=(reg_t)user_stack;
    pcb->kernel_sp = (reg_t)pt_switchto;
}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    int i;
    for(i=0;i < tasknum && i < NUM_MAX_TASK; i++){
        pcb[i].pid = i+1;
        pcb[i].kernel_sp = allocKernelPage(1) + 1 * PAGE_SIZE;
        pcb[i].user_sp   = allocUserPage(1) +   1 * PAGE_SIZE;
        pcb[i].status = TASK_READY;
        pcb[i].cursor_x = 0;
        pcb[i].cursor_y = 0;
        pcb[i].wakeup_time = 0;
        load_task_img(tasks[i].name);
        unsigned long ENTRY_POINT = tasks[i].TASK_ID;
        init_pcb_stack(pcb[i].kernel_sp,pcb[i].user_sp,ENTRY_POINT,pcb+i);
        list_add(&(pcb[i].list),&ready_queue);
    }

    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running = &pid0_pcb;
}

static void do_thread_create(void * entrypoint){
    int id = tasknum;
    pcb[id].pid = id + 1;
    pcb[id].kernel_sp = allocKernelPage(1) + 1 * PAGE_SIZE;
    pcb[id].user_sp   = allocUserPage(1) +   1 * PAGE_SIZE;
    pcb[id].status = TASK_READY;
    pcb[id].cursor_x = 0;
    pcb[id].cursor_y = 0;
    pcb[id].wakeup_time = 0;
    init_pcb_stack(pcb[id].kernel_sp,pcb[id].user_sp,entrypoint,pcb+id);
    list_add(&(pcb[id].list),&ready_queue);
}

static void debug_syscall(void){
    printk("> [Error] Syscall number not in used. \n\r");
    while(1);
}
/*
#define SYSCALL_SLEEP 2
#define SYSCALL_YIELD 7
#define SYSCALL_THREAD_CREATE 10
#define SYSCALL_WRITE 20
#define SYSCALL_CURSOR 22
#define SYSCALL_REFLUSH 23
#define SYSCALL_GET_TIMEBASE 30
#define SYSCALL_GET_TICK 31
#define SYSCALL_LOCK_INIT 40
#define SYSCALL_LOCK_ACQ 41
#define SYSCALL_LOCK_RELEASE 42
*/


static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    int i;
    // for(i = 0; i < NUM_SYSCALLS; i++){
    //     syscall[i] = (long (*)())debug_syscall;
    // }
    syscall[SYSCALL_SLEEP]        = (long (*)())do_sleep;
    syscall[SYSCALL_YIELD]        = (long (*)())do_scheduler;
    
    syscall[SYSCALL_THREAD_CREATE]= (long (*)())do_thread_create;

    syscall[SYSCALL_WRITE]        = (long (*)())screen_write;
    syscall[SYSCALL_CURSOR]       = (long (*)())screen_move_cursor;
    syscall[SYSCALL_REFLUSH]      = (long (*)())screen_reflush;
    syscall[SYSCALL_GET_TIMEBASE] = (long (*)())get_time_base;
    syscall[SYSCALL_GET_TICK]     = (long (*)())get_ticks;

    syscall[SYSCALL_LOCK_INIT]    = (long (*)())do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ]     = (long (*)())do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE] = (long (*)())do_mutex_lock_release;
    sleep_queue.next = &sleep_queue;
    sleep_queue.prev = &sleep_queue; 
}

int main(void)
{

    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Init Process Control Blocks |•'-'•) ✧
    init_pcb();
    printk("> [INIT] PCB initialization succeeded.\n");

    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);

    // Init lock mechanism o(´^｀)o
    init_locks();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    bios_set_timer(get_ticks() + TIMER_INTERVAL);
    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1){
        // If you do non-preemptive scheduling, it's used to surrender control
        // do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        enable_preempt();
        asm volatile("wfi");
    }
    return 0;
}
