# 项目介绍
本次实验的第一部分是在原有实验一设计的操作系统引导的基础上进行修改，并使得操作系统具备一些最基本的进程调度和锁功能。
## 完成的实验任务与重要BUG
### 实验任务1：任务启动和非抢占式调度
本部分实验要求为每一个进程设计一个进程控制块(PCB)，其用来记录进程当前状态和控制进程运行的关键信息。事实上，在实际实现中我发现实验任务1/2中，原本提供的简单进程控制块就足够完成相关的控制内容，其构成如下：
```c
typedef struct pcb
{
    /* register context */
    // NOTE: this order must be preserved, which is defined in regs.h!!
    reg_t kernel_sp;
    reg_t user_sp;

    /* previous, next pointer */
    list_node_t list;

    /* process id */
    pid_t pid;

    /* BLOCK | READY | RUNNING */
    task_status_t status;

    /* cursor position */
    int cursor_x;
    int cursor_y;

    /* time(seconds) to wake up sleeping PCB */
    uint64_t wakeup_time;

} pcb_t;
```
主要包含内核和用户态栈地址，一个用于进程切换的list队列指针，但前进程的pid编号(其余内容暂时没用到)。PCB初始化总的来讲非常的简单，其就是利用$mm.c$将内存进行分配，从而获得栈地址，其余的寄存器等值初始化为0即可。对于list，我维护的是一个循环队列，将所有待执行的程序串起来。

我设计的任务切换具体实现方法就是利用switch_to函数，其将A进程的寄存器的值store，并将B进程的寄存器值load，并进入B进程的入口地址即可。如果B进程是一个新进程，那么直接将B进程的入口ra设计成ENRTY_POINT即可。

总的来讲，实验任务1并不复杂，只要弄明白do_scheduler的流程，就基本可以实现本任务。
运行效果如图：
```
> [TASK] This task is to test scheduler. (8)                                    
> [TASK] This task is to test scheduler. (7)                                    
                                                                                
                                                                                
                                                                                
                                                                                
                                                                                
                                                                                
                                                                                
                                                                                
                                               _QEMU: Terminated                
stu@stu:~/ucas-os-22fall/Project2_SimpleKernal$ \                               
                                        |\ ____\_\__                            
                                      -=\c`""""""" "`)                          
                                         `~~~~~/ /~~`                           
                                           -==/ /                               
                                             '-'  
```
### 实验任务2: 互斥锁的实现
实验任务2主要的要求是实现一个互斥锁机制，其可以实现在多个进程同时访问同一个锁的时候后访问的进程被挂起阻塞，只有一个进程被释放后下一个进程才会被唤醒。

互斥锁设计方式大体流程是这样的，每个用户程序需要获得一个句柄，由句柄来确定上在哪一个锁上（如果没有已有的锁使用该句柄，那么需要新生成一个锁）。互斥锁的大体设计是这样的：
```c
typedef struct mutex_lock
{
    spin_lock_t lock;
    list_head block_queue;
    int key;
} mutex_lock_t;
```
其中key自然是句柄的键值，lock是代表锁的状态(其只有lock和unlock两种情况)。block_queue则是互斥锁设计的核心策略，其本质和进程的ready_queue是同一个队列，即一个循环队列，在事实上的运行中，需要实现FIFO的思想，这样子可以保证不会出现某一个在锁上的进程感到饥饿。

上锁的执行流程是这样的：如果当前执行的程序所需要的锁是上锁的，需要执行do_block命令，从而实现阻塞。do_block的流程就是将当前的current_running存ready_queue转到block_queue，这样的话再进行do_scheduler()操作就不会将这个程序选中了，这也就阻塞了这个进程。

而解锁就很简单了，只需要从block_queue取出一个进程，然后放入到ready_queue中就可以了，为了保证进程运行的公平性，所以取出进程必须要严格执行先入先出的策略。

在实际实现中，遇到了两个错误：
第一个错误是错误的实现了上锁，实际上并没有修改lock的值：
```c
void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    mutex_lock_t lock_now = mlocks[mlock_idx];
    while(lock_now.lock.status==LOCKED)
        do_block(&(current_running->list),&lock_now.block_queue);
    lock_now.lock.status=LOCKED;
}
```
我错误的创建了一个新变量叫做 lock_now,使得其对应的值为所需的互斥锁，很显然这样的操作并不能正确修改互斥锁，所以在运行过程中出现了崩溃。但是本错误难以察觉，这是因为我最开始直接再这一块打印lock_now的数据，很显然看上去是正确的，所以浪费了很多的时间在订正本错误上。

第二个错误是我没能够理解句柄的含义，最开始我实现的时候是这样分配互斥锁的：
```c
int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */
    int i;
    int id=-1;
    for(i=0;i<LOCK_NUM;i++){
        if(mlocks[i].key==-1){
            id=i;
            mlocks[i].key=key;
            break;
        }
    }
    return id;
}
```
这样lock1.c和lock2.c就各自分配到了一把互斥锁，实际运行起来就没有互斥现象，在运行了一段时间后也会发生报错。
正确运行的效果如图：
```
> [TASK] This task is to test scheduler. (2)                                    
> [TASK] This task is to test scheduler. (1)                                    
> [TASK] Has acquired lock and running.(0)                                      
> [TASK] Applying for a lock.                                                   
                                                                                
                                                                                
                                                                                
                                                                                
                                                                                
                                                                                
                              _                                                 
                            -=\`\                                               
                        |\ ____\_\__                                            
                      -=\c`""""""" "`)                                          
                         `~~~~~/ /~~`                                           
                           -==/
```
## 运行代码的流程：
直接使用脚本运行会把所有的程序load制作进image中，但在前期的实验中显然操作系统并不能支持后续程序的运行，所以脚本运行并不能在PART1部分中使用。
### 脚本运行
```
make all
make run
```
### 实验1输入：
```
make clean
make dirs
make elf
make asm
cp createimage build/
cd ./build && ./createimage --extended bootblock main print1 fly print2&& cd ..
```
### 实验2输入：
```
make clean
make dirs
make elf
make asm
cp createimage build/
cd ./build && ./createimage --extended bootblock main print1 fly print2 lock1 lock2 && cd ..
```