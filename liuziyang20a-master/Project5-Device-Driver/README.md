# 项目介绍
本次实验的主要要求是设计一个交互界面，也就是所谓的SHELL。除此之外，还要求实现一些简单的交互，如查看正在运行的进程状态，杀死、启动进程。在基本实验的基础上，我也实现了双核的启动与运行。
## 完成的实验任务与重要BUG
### 实验任务1：终端和终端命令的实现
本部分难度很小，本质就是写一个支持命令行的终端。这本质就是C语言相关课程中所谓要求实现命令行识别的一个任务。
但是值得注意的是，本实验中的API使用，在实现PS命令时由于是在内核态打印资料，所以需要选择printk命令，但是我最开始错的使用了printv命令，这导致screenclear系统调用没有办法清空这一块屏幕。调整后的PS代码实现如下：
```c
void do_process_show(void)
{
    /* TODO [P3-TASK1] exec exit kill waitpid ps*/
    current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
    screen_move_cursor(0,pcb[0].cursor_y);
    printk("\n[Process Table]\n");
    int i=0,j=0;
    for(;i<NUM_MAX_TASK;i++){
        if(pcb[i].status == TASK_EXITED) continue;
        printk("[%d] PID : %d NAME : %s MASK : %d STATUS : ",j,pcb[i].pid,pcb[i].name,pcb[i].hart_mask);
        if(pcb[i].status == TASK_RUNNING) {
            printk("RUNNING ");
            if(current_running_0->pid == pcb[i].pid){
                printk("running on Core 0\n");
            }
            if(current_running_1->pid == pcb[i].pid){
                printk("running on Core 1\n");
            }
        }
        if(pcb[i].status == TASK_BLOCKED)
            printk("BLOCKED\n");
        if(pcb[i].status == TASK_READY)
            printk("READY\n"); 
        j++;
    }
    screen_reflush();
}
```
另外，为了充分的实现我所需要的SHELL的特性，我还将mov_cursor设计为一个系统调用，使得DEL操作可以在当行进行删除。
### 实验任务1续：exec、kill、exit、waitpid的实现
exec实验是本实验中遇到的最为困难，调试任务最为艰巨的实验。为了实现正常的exec中argc和argv的命令，需要调整user_stack的栈顶位置。在最开始时，我错误的只将switch_to操作中的CALLEE寄存器中SP的值修改为新的USER_SP，但是实际上在RESTORE_CONTEXT中也存在了一个USR_SP，而且由于所有程序的入口地址都已经被修改为了ret_from_exception，所以这个USR_SP会成为进程的USR_SP。这个错误非常难以察觉，耗费了7个小时才被发现。

核心代码：
```c
    pt_switchto->regs[0]=(reg_t)&ret_from_exception;
    pcb->kernel_sp = (reg_t)pt_switchto;

    uint64_t user_sp_now = argv_base;
    uint64_t * argv_ptr = (ptr_t)argv_base;
    for(i=0; i<argc; i++){
        uint32_t len = strlen(argv[i]);
        user_sp_now = user_sp_now - len - 1;
        (*argv_ptr) = user_sp_now;
        strcpy(user_sp_now,argv[i]);
        argv_ptr ++;
    }
    (*argv_ptr) = 0;
    int tot =  user_stack - user_sp_now;
    int siz_alignment = ((tot/128)+((tot%128==0)?0:1))*128;
    user_sp_now = user_stack - siz_alignment;
    pt_switchto->regs[1]=(reg_t)user_sp_now;
    pcb->user_sp=(reg_t)user_sp_now;
    pt_regs->regs[2]  = (reg_t)user_sp_now;
```
kill 和 exit都比较类似，并且不难实现，唯一值得注意的是锁资源的释放。为此，我设计了一个队列，分别存wait和lock而被block的进程，然后在kill和exit的时候将其杀掉。需要注意的是，在多核运行时kill一个正在运行的进程是完全可能的一件事，所以应当在此时将进程标记为EXITED，然后在yield的时候将其释放资源。

### 实验任务2 实现同步原语
同步原语部分难度都不是很大，特别是如果实现了cond，就可以用cond实现mailbox。对于barrier，只需要设计一个计数器，统计一下达到屏障的进程数量，当进程数量达标，就唤醒所有阻塞的进程即可。对于管程，则依旧是利用锁维护一个block队列，在signal的时候释放一个，在broadcast的时候全部释放。mailbox可以直接由管程实现，核心思想是维护一个端头和一个端尾，每一次从端尾写数据，端头读数据。如果不够大或者数据不够多，那么就用管程控制即可。
```c
int do_mbox_send(int mbox_idx, void * msg, int msg_length){
    mailbox_t * mailbox_now = &(mailboxs[mbox_idx]);
    do_mutex_lock_acquire(&(mailbox_now->lock));
    int block = 0;
    while(MAX_MBOX_LENGTH - mailbox_now->siz < msg_length){
        block++;
        do_condition_wait_ptr(mailbox_now->cond_idx,&(mailbox_now->lock));
    }
        
    mailbox_now->siz += msg_length;
    if(mailbox_now->tail + msg_length - 1< MAX_MBOX_LENGTH){
        mboxncpy(((*mailbox_now).buffer + (mailbox_now->tail)),msg,msg_length);
        // printl("send buffer=%s,msg=%s,len=%d",msg,((*mailbox_now).buffer + mailbox_now->tail),msg_length);
        if(mailbox_now->tail + msg_length == MAX_MBOX_LENGTH)
            mailbox_now->tail = 0;
        else
            mailbox_now->tail = mailbox_now->tail + msg_length;
    }
    else{
        int prelen = MAX_MBOX_LENGTH - (mailbox_now->tail);
        mboxncpy(((*mailbox_now).buffer + (mailbox_now->tail)),msg,prelen);
        mboxncpy(mailbox_now->buffer,msg+prelen,msg_length-prelen);
        mailbox_now->tail = msg_length - prelen;
    }
    do_condition_broadcast(mailbox_now->cond_idx);
    do_mutex_lock_release(&(mailbox_now->lock));
    return block;
}

int do_mbox_recv(int mbox_idx, void * msg, int msg_length){
    mailbox_t * mailbox_now = &(mailboxs[mbox_idx]);
    do_mutex_lock_acquire(&(mailbox_now->lock));
    int block = 0;
    while(mailbox_now->siz < msg_length){
        block++;
        do_condition_wait_ptr(mailbox_now->cond_idx,&(mailbox_now->lock));
    }
    mailbox_now->siz -= msg_length;
    if(mailbox_now->head + msg_length -1< MAX_MBOX_LENGTH){
        mboxncpy(msg,((*mailbox_now).buffer + mailbox_now->head),msg_length);
        if(mailbox_now->head + msg_length == MAX_MBOX_LENGTH)
            mailbox_now->head = 0;
        else
            mailbox_now->head = mailbox_now->head + msg_length;
    }
    else{
        int prelen = MAX_MBOX_LENGTH - (mailbox_now->head);
        mboxncpy(msg,((*mailbox_now).buffer + mailbox_now->head),prelen);
        mboxncpy(msg+prelen,(*mailbox_now).buffer,msg_length-prelen);
        mailbox_now->head = msg_length - prelen;
    }
    do_condition_broadcast(mailbox_now->cond_idx);
    do_mutex_lock_release(&(mailbox_now->lock));
    return block;    
}
```
### 实验任务3/4 多核操作系统
多核操作系统看似复杂，但实际上在使用大内核锁逻辑后已经非常简单了。

首先是需要从核的bootload逻辑，这个逻辑核心思想就是开中断，然后设置kernal为中断入口地址，然后等待核间中断即可。
```riscv

	// MASK ALL INTERRUPTS
	csrw CSR_SIE, zero
	csrw CSR_SIP, zero

	// let stvec pointer to kernel_main
	la t0, kernel
	csrw CSR_STVEC, t0

	li t0, SIE_SSIE
	csrs CSR_SIE, t0

	li t0, SR_SIE
	csrw CSR_SSTATUS, t0

	lui a5, 	 %hi(msg2)
	addi a1, a5, %lo(msg2)

	li a0, BIOS_PUTSTR
	call bios_func_entry
wait:
	wfi
	j wait
```
除此之外，由于bss等都已经被主核给清空了，所以只需要设置一下从核的tp寄存器和sp寄存器即可。
主核进入kernal之后，主核完成大部分任务，在所有初始化已经完毕之后，向从核发出核间中断，使得从核也进入kernal。
```c
        send_ipi((unsigned long *)0);
        clear_sip();
```
对从核来讲，只需要重新设置自己例外入口地址(之前设置到kernal入口地址上了)，然后等待中断即可。
```c
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
```
需要注意的一点是调度阶段很有可能出现没有需要运行的程序，对此我解决的办法是将主核/从核的kernal给放进去，由于其会在最后的while(1)空转，因此也就保证了在没有需要调度程序的时候正确运行。

对于绑核操作，只需要在PCB块中添加一个mask，在调度的时候根据需要调的核ID选择需要调度的进程即可。

### O2的正确编译
由于O2编译优化会使得一些函数inline，最终删除这些可以inline的函数，所以这使得invoke_syscall函数的内联汇编出现了问题。为了使得O2优化中这自作主张的错误优化失效，对此的解决办法是将其设计一个中间逻辑，而不是单纯的用a0、a1...来代指函数接入的参数。修改逻辑如下：
```c    long ret=0;
    asm volatile("mv a7, %[sysno]\n\t"
                 "mv a0, %[arg0]\n\t"
                 "mv a1, %[arg1]\n\t"
                 "mv a2, %[arg2]\n\t"
                 "mv a3, %[arg3]\n\t"
                 "mv a4, %[arg4]\n\t"
                 "ecall\n\t"
                 "mv %[ret], a0\n\t"
                 :[ret] "=r" (ret)
                 :[sysno]"r"(sysno),
                 [arg0]"r"(arg0),
                 [arg1]"r"(arg1),
                 [arg2]"r"(arg2),
                 [arg3]"r"(arg3),
                 [arg4]"r"(arg4)
                );
    return ret;
```
## 运行代码的流程：
### 脚本运行
```
make all
make run-smp
```
### TASK1
```
    exec waitpid &
```
### TASK2
```
    exec condition &
    exec barrier &
    exec mbox_server &
    exec mbox_client &
```
### TASK3/4
```
    exec multicore &
    taskset 0x01 affinity
```