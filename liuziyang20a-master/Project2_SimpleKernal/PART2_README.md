# 项目介绍
实验任务的后半段主要要求是实现用户态和内核态的分离。在上半部分中，实现的系统调用都是直接调用的内核提供的库函数，但实际上一个正确的操作系统应当实现用户进程调用封装好的系统调用。除封装原有的内核态函数和调用之外，在C-core实验中还需要实现线程创建的系统调用。

## 完成的实验任务与重要BUG
### 实验任务3：系统调用
本任务总体难度较大，是整个实验的核心部分。具体的实验设计流程如下：

首先是要在内核初始启动的时候初始化系统调用，这个的代码实现非常简单，只需要按照对应的宏将所需要调用的函数地址封装到对应的表内即可：
```c
static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    int i;
    for(i = 0; i < NUM_SYSCALLS; i++){
        syscall[i] = (long (*)())debug_syscall;
    }
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
```
这里需要注意的一点是系统调用的宏在两个地方被定义，分别是unistd.h和syscall.h，一个对应内核态，一个对应用户态，所以在添加新的系统调用的时候要注意同时修改。

另外一步是完善PCB内核栈的初始化，其实也就是添加RISC-V的32个寄存器初始化逻辑。不赋值为零的四个寄存器分别为：sp, tp, sepc, sstatus。其中sp赋值为内核栈，tp赋值为pcb块地址，SEPC是sret操作的返回地址，对于一个新程序应当设置为ENTRYPOINT，sstatus是状态寄存器，首先将SPP字段置零表示这是个用户态程序，其次将SPIE置1表示允许中断。下一步时设置STVEC寄存器，ecall指令会跳转到这个寄存器指向的地址，所以设置地址为exception_handler_entry。
```
    for(i = 0; i < 32; i++){
        pt_regs->regs[i] = 0;
    }
    pt_regs->regs[2] = (reg_t)user_stack;
    pt_regs->regs[4] = (reg_t)pcb;//thread pointer
```

初始化例外表时只需要将不是SYSCALL的宏设置为handle_other（这直接报例外信息并退出），SYSCALL的宏设置为handle_syscall。handle_syscall的设计非常简单，就是利用设置好的寄存器下标去访问初始化系统调用的数组，然后得到需要调用的函数入口地址，并跳转，唯一要注意的时这一步要将SEPC+4否则程序sret后会陷在这个例外中死循环。

实现汇编的部分主要是进入例外处理和退出例外处理，这并不复杂，但值得注意的是要保证无论是内核态还是用户态程序都要保证存储32个寄存器的步骤都在内核栈中完成。为了保证用户态程序使用正确的栈存储，我维护了sscratch寄存器，其在内核态为0，用户态为tp，由此可以判断程序当前在哪个状态。

invoke_syscall所需要实现的部分是将用户态输入的那些参数正确的放在指定的寄存器，以确保handle_syscall能够调用这些参数。本部分实验我所遇到的主要困难就在这一部分，由于没有学习汇编语言，我并不知道如何将c程序和汇编语言中的寄存器进行交互，这导致程序始终在第一次系统调用后就报错。

在查阅了大量资料后，我发现正确的实现逻辑设这样的：
```
    long ret=0;
    asm volatile("mv a7, a0\n\t"
                 "mv a0, a1\n\t"
                 "mv a1, a2\n\t"
                 "mv a2, a3\n\t"
                 "mv a3, a4\n\t"
                 "ecall\n\t"
                 "mv %0, a0\n\t"
                 :"=r"(ret)
                );
    return ret;
```
其中，ret是系统调用的函数入口地址。实现逻辑是将函数调用种类的宏放在a7，剩下的参数次序放在a0-a4，ecall命令地址自动跳转到handle_syscall_entry，interrupt_helper的分析后，跳转到syscall_handle，得到入口地址，存储到a0种，赋值给ret，最终返回。

对于休眠部分，核心解决策略是将需要休眠的程序放到sleep_queue里阻塞起来，然后在每一次do_scheduler之前将休眠时间到位的程序重新放回ready_queue中。判断需要阻塞到什么时候的办法是利用得到的时钟频率和阻塞时长来计算：
```
current_running->wakeup_time = get_ticks() + sleep_time * time_base;
```
判断可以唤醒的逻辑是看get_ticks是否大于wakeup_time，是就唤醒。

总的来讲以上就是本任务的全体流程，总的来讲只遇到了invoke_syscall指令错误设计的问题，但是在调试的时候由于并不知道gdb不打断点不会跟踪到用户态程序，所以花费了很多时间。

### 实验任务4：时钟中断、抢占式调度
本部分是实现任务的抢占式调度，这样子任务调度的控制权就从用户程序变为了CPU本身。
抢占式调度的实现总体来讲并不复杂，打开全体中断使能只需要将sstatus的sie位写高即可。对于中断入口表初始化，由于只实现了负责调度的中断，可以直接全部设置成时间调度的处理中断就行了。处理抢占式调度的中断时，检查睡眠队列，刷新屏幕，重新设置timer，最后do_scheduler即可。对于main部分，首先要设置一个timer，这样第一个中断可以让程序从kernal切换到第一个用户态程序，其余部分就是按照讲义要求注释掉do_scheduler换成enable_preempt即可。

在执行本部分时，我遇到了一个考虑不周的bug，在发生在第一个抢占式调度时。由于我维护tp寄存器来寻找所需要存储的数据，但是我并没有将kernal程序的tp值放进去，在之前的程序中由于kernal向其它程序切换是使用do_scheduler，所以不会报错，但在抢占式调度的时候这个问题就暴露了。解决方案是我在head.S程序中初始化tp的值：
```
  la tp, pid0_pcb
  li sp, KERNEL_STACK # sp = KERNEL_STACK 
  j main  # jump to main
```

### 实验任务5:实现线程的创建thread_create
由于本实验中实现的进程和线程区别不大，实际上可以大量复用进程调度的程序从而实现线程创建。对于线程的控制块可以直接复用进程的控制块，而且也放在ready_queue里进行调度。

在实际实现中核心的代码如下：
```
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
```
可以发现线程创建函数全部复用init_pcb逻辑，唯一的区别就是entry_point是由用户态函数传递过来的。用户态函数调用逻辑如下：
```
    sys_thread_create(&multi_test);
```

总的来讲任务5整体流程非常简单，实际实现仅花费不到30分钟，并且也没有遇到错误。