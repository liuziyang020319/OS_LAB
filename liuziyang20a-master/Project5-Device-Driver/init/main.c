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
#include <os/ioremap.h>
#include <os/smp.h>
#include <os/net.h>
#include <sys/syscall.h>
#include <screen.h>
#include <e1000.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>

extern void ret_from_exception();

#define TASK_NUM_POINT 0xffffffc0502001fe
#define BASE 0xffffffc052000000



int version = 2; // version must between 0 and 9

unsigned long tasknum;
uint64_t padding_ADDR;
// Task info array
task_info_t tasks[TASK_MAXNUM];


static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[SD_WRITE]        = (long (*)())sd_write;
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
    uint64_t *OFFSET_POINT = 0xffffffc0502001f0;
    uint64_t OFFSET = * OFFSET_POINT;
    uint64_t * entryptr = BASE + OFFSET;
    tasknum = *entryptr;
    entryptr += 1;
    padding_ADDR = *entryptr;
    entryptr += 1;
    printl("PAD = %ld",padding_ADDR);
    for(int idx=0;idx<tasknum;++idx){
        for(int i=0;i<32;++i){
            char * chptr = entryptr;
            char ch = *chptr;
            tasks[idx].name[i]=ch;
            uint64_t temp = entryptr;
            temp++;
            entryptr = temp;
        }
        uint64_t TASK_OFFSET = *entryptr;
        tasks[idx].offset=TASK_OFFSET;
        entryptr+=1;
        uint64_t TASK_SIZ = *entryptr;
        tasks[idx].SIZE=TASK_SIZ;
        entryptr+=1;
        uint64_t P_VADDR = *entryptr;
        tasks[idx].p_vaddr = P_VADDR;
        entryptr+=1;
        uint64_t P_FILESZ = *entryptr;
        tasks[idx].p_filesz = P_FILESZ;
        entryptr+=1;
        uint64_t P_MEMSZ = *entryptr;
        tasks[idx].p_memsz = P_MEMSZ;
        entryptr+=1;
        uint64_t P_FLAGS = *entryptr;
        tasks[idx].p_flags = P_FLAGS;
        entryptr+=1; 
    }
}

void init_task_load(){
    // for(int idx = 0; idx < tasknum; idx++){
    //     load_task_img(tasks[idx].name);
    // }
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
    pcb_t *pcb, int argc, char *argv[])
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
    // pt_regs->regs[1]  = (reg_t)sys_exit;
    pt_regs->regs[2]  = (reg_t)user_stack;
    pt_regs->regs[4]  = (reg_t)pcb;//thread pointer
    pt_regs->regs[10] = (reg_t)argc;
    pt_regs->regs[11] = (reg_t)argv;

    //SPIE 异常发生之前的中断使能
    //SPP  发生例外时的权限模式
    pt_regs->sepc     = entry_point;
    pt_regs->sstatus  = (SR_SPIE) & (~SR_SPP) | SR_SUM;
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
    

    
    // printl("%ld\n",user_stack);
    
}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    ready_queue.prev = &(ready_queue);
    ready_queue.next = &(ready_queue);
    //
    pcb[0].pid = 1;
    
    pcb[0].pgdir = allocPage(1);
    clear_pgdir(pcb[0].pgdir);

    //copy kernal page table
    share_pgtable(pcb[0].pgdir,pa2kva(PGDIR_PA));
    clean_page(pcb[0].pgdir);

    pcb[0].kernel_sp = allocPage(1) + 1 * PAGE_SIZE;
    pcb[0].user_sp   = USER_STACK_ADDR;
    
    
    pcb[0].kernel_stack_base = pcb[0].kernel_sp;
    pcb[0].user_stack_base   = pcb[0].user_sp;
    
    
    pcb[0].status = TASK_READY;
    pcb[0].cursor_x = 0;
    pcb[0].cursor_y = 0;
    pcb[0].wakeup_time = 0;
    pcb[0].wait_list.prev = &pcb[0].wait_list;
    pcb[0].wait_list.next = &pcb[0].wait_list;
    pcb[0].lock_list.prev = &pcb[0].lock_list;
    pcb[0].lock_list.next = &pcb[0].lock_list;
    pcb[0].cond_list.prev = &pcb[0].cond_list;
    pcb[0].cond_list.next = &pcb[0].cond_list;
    pcb[0].parent_id = 0;
    pcb[0].sibling_id = 0;
    pcb[0].thread_num = 0;
    pcb[0].shm_num = 0;
    for(int j=0;j<32;++j)pcb[0].name[j] = tasks[0].name[j];
    // load_task_img(tasks[0].name);
    unsigned long ENTRY_POINT = load_task_img(tasks[0].name,pcb[0].pgdir);// = tasks[0].TASK_ID;
    
    alloc_page_helper(USER_STACK_ADDR - PAGE_SIZE,pcb[0].pgdir, 0);


    init_pcb_stack(pcb[0].kernel_sp,pcb[0].user_sp,ENTRY_POINT,pcb+0,0,NULL);
    pcb[0].hart_mask = 3;
    list_add(&(pcb[0].list),&ready_queue);

    int i;
    for(i=1; i < NUM_MAX_TASK; i++){
        pcb[i].pid = i+1;
        // pcb[i].kernel_sp = allocKernelPage(1) + 1 * PAGE_SIZE;
        // pcb[i].user_sp   = allocUserPage(1) +   1 * PAGE_SIZE;
        // pcb[i].kernel_stack_base = pcb[i].kernel_sp;
        // pcb[i].user_stack_base   = pcb[i].user_sp;
        pcb[i].status = TASK_EXITED;
        pcb[i].wait_list.prev = &pcb[i].wait_list;
        pcb[i].wait_list.next = &pcb[i].wait_list;
        pcb[i].lock_list.prev = &pcb[i].lock_list;
        pcb[i].lock_list.next = &pcb[i].lock_list;
        pcb[i].cond_list.prev = &pcb[i].cond_list;
        pcb[i].cond_list.next = &pcb[i].cond_list;
        pcb[i].hart_mask = 3;
    }

    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running_0 = &pid0_pcb_0;
    current_running_1 = &pid0_pcb_1;
}

void clean_page(uint64_t pgdir_addr){
    PTE * pgdir = pgdir_addr;
    for(uint64_t va = 0x50000000lu; va < 0x51000000lu; va += 0x200000lu){
        va &= VA_MASK;
        uint64_t vpn2 =
            va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
        pgdir[vpn2] = 0;
    }

}

pid_t do_exec(char *name, int argc, char *argv[])
{
    /* TODO [P3-TASK1] exec exit kill waitpid ps*/
    current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
    for(int i=1;i<tasknum;i++){
        if(strcmp(tasks[i].name,name) == 0){
            for(int id=0; id < NUM_MAX_TASK; id++){
                if(pcb[id].status == TASK_EXITED){

                    pcb[id].status     = TASK_READY;
                    pcb[id].cursor_x   = 0;
                    pcb[id].cursor_y   = 0;
                    pcb[id].wakeup_time = 0;
                    pcb[id].hart_mask = (*current_running)->hart_mask;

                    pcb[id].wait_list.prev = &pcb[id].wait_list;
                    pcb[id].wait_list.next = &pcb[id].wait_list;
                    pcb[id].lock_list.prev = &pcb[id].lock_list;
                    pcb[id].lock_list.next = &pcb[id].lock_list;
                    pcb[id].cond_list.prev = &pcb[id].cond_list;
                    pcb[id].cond_list.next = &pcb[id].cond_list;

                    for(int j=0;j<32;++j)pcb[id].name[j] = tasks[i].name[j];

                    pcb[id].pgdir = allocPage(1);
                    clear_pgdir(pcb[id].pgdir); 
                    share_pgtable(pcb[id].pgdir,pa2kva(PGDIR_PA));

                    clean_page(pcb[id].pgdir);

                    // ((PTE *)(pcb[id].pgdir))[1] = (PTE )0;

                    pcb[id].kernel_sp  = allocPage(1) + 1 * PAGE_SIZE;
                    pcb[id].user_sp    = USER_STACK_ADDR;
                    pcb[id].kernel_stack_base = pcb[id].kernel_sp;
                    pcb[id].user_stack_base = pcb[id].user_sp;

                    pcb[id].parent_id = 0;
                    pcb[id].sibling_id = 0;
                    pcb[id].thread_num = 0;
                    pcb[id].shm_num = 0;

                    uintptr_t va = alloc_page_helper(pcb[id].user_sp - PAGE_SIZE,pcb[id].pgdir, 0) + PAGE_SIZE;
                    uintptr_t argv_va_base = va - sizeof(uintptr_t)*(argc + 1);
                    uintptr_t argv_pa_base = USER_STACK_ADDR - sizeof(uintptr_t)*(argc + 1);

                    uint64_t user_sp_va = argv_va_base;
                    uint64_t user_sp_pa = argv_pa_base;
                    uintptr_t * argv_ptr = (ptr_t)argv_va_base;
                    for(int i=0; i<argc; i++){
                        uint32_t len = strlen(argv[i]);
                        user_sp_va = user_sp_va - len - 1;
                        user_sp_pa = user_sp_pa - len - 1;
                        (*argv_ptr) = user_sp_pa;
                        strcpy(user_sp_va,argv[i]);
                        argv_ptr ++;
                    }
                    (*argv_ptr) = 0;
                    uint64_t tot = va - user_sp_va;
                    uint64_t siz_aliagnment = ((tot/128)+((tot%128==0)?0:1))*128;
                    user_sp_va = va - siz_aliagnment;
                    pcb[id].user_sp = pcb[id].user_sp - siz_aliagnment;

                    // load_task_img(tasks[i].name);
                    uint64_t ENTRY_POINT = load_task_img(tasks[i].name,pcb[id].pgdir);
                    init_pcb_stack(pcb[id].kernel_sp,pcb[id].user_sp,ENTRY_POINT,pcb+id,argc,argv_pa_base);

                    // printl("%d\n",pcb[id].user_sp);
                    list_add(&(pcb[id].list),&ready_queue);
                    // printl("%d %s\n",pcb+id,pcb[id].name);
                    return pcb[id].pid;
                }
            }
        }
    }
    return 0;
}

static void do_thread_create(pthread_t *thread, void (*start_routine)(void*), void *arg){
    // int id = tasknum;
    // pcb[id].pid = id + 1;
    // pcb[id].kernel_sp = allocKernelPage(1) + 1 * PAGE_SIZE;
    // pcb[id].user_sp   = allocUserPage(1) +   1 * PAGE_SIZE;
    // pcb[id].status = TASK_READY;
    // pcb[id].cursor_x = 0;
    // pcb[id].cursor_y = 0;
    // pcb[id].wakeup_time = 0;
    // init_pcb_stack(pcb[id].kernel_sp,pcb[id].user_sp,entrypoint,pcb+id,0,NULL);
    // list_add(&(pcb[id].list),&ready_queue);
    current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
    int i;
    for(i = 0; i < NUM_MAX_TASK; i++){
        if(pcb[i].status == TASK_EXITED){
            pcb[i].status = TASK_READY;
            pcb[i].cursor_x = 0;
            pcb[i].cursor_y = 0;
            pcb[i].wakeup_time = 0;
            pcb[i].hart_mask = (*current_running)->hart_mask;

            pcb[i].wait_list.prev = &pcb[i].wait_list;
            pcb[i].wait_list.next = &pcb[i].wait_list;
            pcb[i].lock_list.prev = &pcb[i].lock_list;
            pcb[i].lock_list.next = &pcb[i].lock_list;
            pcb[i].cond_list.prev = &pcb[i].cond_list;
            pcb[i].cond_list.next = &pcb[i].cond_list;

            for(int j=0; j<32; ++j)pcb[i].name[j] = (*current_running)->name[j];
            pcb[i].pgdir = (*current_running)->pgdir;
            pcb[i].kernel_sp = allocPage(1) + PAGE_SIZE;
            (*current_running)->thread_num ++;

            pcb[i].user_sp = USER_STACK_ADDR + PAGE_SIZE * ((*current_running)->thread_num);

            pcb[i].kernel_stack_base = pcb[i].kernel_sp;
            pcb[i].user_stack_base = pcb[i].user_sp;

            alloc_page_helper(pcb[i].user_sp - PAGE_SIZE, pcb[i].pgdir, 0);

            pcb[i].thread_num = 0;
            pcb[i].sibling_id = (*current_running)->sibling_id;
            (*current_running)->sibling_id = pcb[i].pid;
            pcb[i].parent_id = (*current_running)->pid;
            pcb[i].shm_num = 0;

            ptr_t entry_point = (ptr_t)start_routine;

            init_pcb_stack(pcb[i].kernel_sp,pcb[i].user_sp,entry_point,pcb+i,(uint64_t)arg,NULL);
            list_add(&(pcb[i].list),&ready_queue);

            *thread = pcb[i].pid;
            return;
        }
    }
    return;
}

static void debug_syscall(void){
    printk("> [Error] Syscall number not in used. \n\r");
    while(1);
}

static int getchar(){
    int x=port_read_ch();
    return x;
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
    for(i = 0; i < NUM_SYSCALLS; i++){
        syscall[i] = (long (*)())debug_syscall;
    }
    syscall[SYSCALL_EXEC]           = (long (*)())do_exec;
    syscall[SYSCALL_EXIT]           = (long (*)())do_exit;
    syscall[SYSCALL_SLEEP]          = (long (*)())do_sleep;
    syscall[SYSCALL_KILL]           = (long (*)())do_kill;
    syscall[SYSCALL_WAITPID]        = (long (*)())do_waitpid;
    syscall[SYSCALL_PS]             = (long (*)())do_process_show;
    syscall[SYSCALL_GETPID]         = (long (*)())do_getpid;
    syscall[SYSCALL_YIELD]          = (long (*)())do_scheduler;
      
    syscall[SYSCALL_THREAD_CREATE]  = (long (*)())do_thread_create;
    syscall[SYSCALL_TASK_SET]       = (long (*)())do_task_set;
    syscall[SYSCALL_TASK_SET_P]     = (long (*)())do_task_set_p;

    syscall[SYSCALL_GETVA2PA]       = (long (*)())get_va2pa;
    syscall[SYSCALL_SNAPSHOT]       = (long (*)())snapshot;

    syscall[SYSCALL_WRITE]          = (long (*)())screen_write;
    syscall[SYSCALL_CURSOR]         = (long (*)())screen_move_cursor;
    syscall[SYSCALL_REFLUSH]        = (long (*)())screen_reflush;
    syscall[SYSCALL_CLEAR]          = (long (*)())screen_clear;
  
    syscall[SYSCALL_GET_TIMEBASE]   = (long (*)())get_time_base;
    syscall[SYSCALL_GET_TICK]       = (long (*)())get_ticks;
    syscall[SYSCALL_GET_CHAR]       = (long (*)())getchar;
  
    syscall[SYSCALL_LOCK_INIT]      = (long (*)())do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ]       = (long (*)())do_mutex_lock_acquire_id;
    syscall[SYSCALL_LOCK_RELEASE]   = (long (*)())do_mutex_lock_release_id;
  
    syscall[SYSCALL_BARR_INIT]      = (long (*)())do_barrier_init;
    syscall[SYSCALL_BARR_WAIT]      = (long (*)())do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY]   = (long (*)())do_barrier_destroy;

    syscall[SYSCALL_COND_INIT]      = (long (*)())do_condition_init; 
    syscall[SYSCALL_COND_WAIT]      = (long (*)())do_condition_wait;
    syscall[SYSCALL_COND_SIGNAL]    = (long (*)())do_condition_signal;
    syscall[SYSCALL_COND_BROADCAST] = (long (*)())do_condition_broadcast;
    syscall[SYSCALL_COND_DESTROY]   = (long (*)())do_condition_destroy;

    syscall[SYSCALL_MBOX_OPEN]      = (long (*)())do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE]     = (long (*)())do_mbox_close;
    syscall[SYSCALL_MBOX_SEND]      = (long (*)())do_mbox_send;
    syscall[SYSCALL_MBOX_RECV]      = (long (*)())do_mbox_recv;

    syscall[SYSCALL_SHM_GET]        = (long (*)())shm_page_get;
    syscall[SYSCALL_SHM_DT]         = (long (*)())shm_page_dt;

    syscall[SYSCALL_NET_SEND]       = (long (*)())do_net_send;
    syscall[SYSCALL_NET_RECV]       = (long (*)())do_net_recv;

    sleep_queue.next = &sleep_queue;
    sleep_queue.prev = &sleep_queue; 

    send_queue.next = &send_queue;
    send_queue.prev = &send_queue;

    recv_queue.next = &recv_queue;
    recv_queue.prev = &recv_queue;

}



int main(void)
{
    if(get_current_cpu_id() == 0){
        // for (uint64_t pa = 0x50000000lu; pa < 0x51000000lu;
        //  pa += 0x200000lu) {
        //     clean_page(pa,PGDIR_PA);
        // }
        set_satp(SATP_MODE_SV39, 0, PGDIR_PA >> NORMAL_PAGE_SHIFT);
        local_flush_tlb_all();

        

        lock_kernel();
        // Init jump table provided by kernel and bios(ΦωΦ)
        init_jmptab();

        // Init task information (〃'▽'〃)
        init_task_info();
        // init_task_load();
        init_swap_page();
        init_schm_page();
        
        // Init Process Control Blocks |•'-'•) ✧
        init_pcb();
        printk("> [INIT] PCB initialization succeeded.\n");

        current_running = &current_running_0;

        // Read CPU frequency (｡•ᴗ-)_
        time_base = bios_read_fdt(TIMEBASE);
        
        e1000 = (volatile uint8_t *)bios_read_fdt(EHTERNET_ADDR);
        uint64_t plic_addr = bios_read_fdt(PLIC_ADDR);
        uint32_t nr_irqs = (uint32_t)bios_read_fdt(NR_IRQS);
        printk("> [INIT] e1000: 0x%lx, plic_addr: 0x%lx, nr_irqs: 0x%lx.\n", e1000, plic_addr, nr_irqs);

        // IOremap
        plic_addr = (uintptr_t)ioremap((uint64_t)plic_addr, 0x4000 * NORMAL_PAGE_SIZE);
        e1000 = (uint8_t *)ioremap((uint64_t)e1000, 8 * NORMAL_PAGE_SIZE);
        printk("> [INIT] IOremap initialization succeeded.\n");

        // Init lock mechanism o(´^｀)o
        init_locks();
        printk("> [INIT] Lock mechanism initialization succeeded.\n");

        // Init condition mechanism 
        init_conditions();
        printk("> [INIT] condition mechanism initialization succeeded.\n");

        // Init barrier mechanism
        init_barriers();
        printk("> [INIT] barrier mechanism initialization succeeded.\n");

        // Init Mailbox mechanism
        init_mbox();
        printk("> [INIT] mailbox mechanism initialization succeeded.\n");

        // Init interrupt (^_^)
        init_exception();
        printk("> [INIT] Interrupt processing initialization succeeded.\n");

        // TODO: [p5-task4] Init plic
        // plic_init(plic_addr, nr_irqs);
        // printk("> [INIT] PLIC initialized successfully. addr = 0x%lx, nr_irqs=0x%x\n", plic_addr, nr_irqs);

        // Init network device
        e1000_init();
        printk("> [INIT] E1000 device initialized successfully.\n");

        // Init system call table (0_0)
        init_syscall();
        printk("> [INIT] System call initialized successfully.\n");

        // Init screen (QAQ)
        init_screen();
        printk("> [INIT] SCREEN initialization succeeded.\n");        

        // send_ipi((unsigned long *)0);
        // clear_sip();
    }
    else{
        current_running = &current_running_1;
        init_exception();
        lock_kernel();
        bios_set_timer(get_ticks() + TIMER_INTERVAL);
        unlock_kernel();
        
        enable_interrupt();
        while(1){
            enable_preempt();
            asm volatile("wfi");
        }
    }

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    bios_set_timer(get_ticks() + TIMER_INTERVAL);
    unlock_kernel();
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
