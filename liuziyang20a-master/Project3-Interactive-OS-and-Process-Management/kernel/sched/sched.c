#include <os/string.h>
#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <os/smp.h>

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack_0 = INIT_KERNEL_STACK + PAGE_SIZE;
const ptr_t pid0_stack_1 = INIT_KERNEL_STACK + PAGE_SIZE * 2;
pcb_t pid0_pcb_0 = {
    .pid = NUM_MAX_TASK,
    .status = TASK_RUNNING,
    .kernel_sp = (ptr_t)pid0_stack_0,
    .user_sp = (ptr_t)pid0_stack_0,
    .hart_mask = 1
};

pcb_t pid0_pcb_1 = {
    .pid = NUM_MAX_TASK,
    .status = TASK_RUNNING,
    .kernel_sp = (ptr_t)pid0_stack_1,
    .user_sp = (ptr_t)pid0_stack_1,
    .hart_mask = 2
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* current running task PCB */
pcb_t ** current_running;
pcb_t * current_running_0;
pcb_t * current_running_1;

list_node_t *find_next_proc(int hart_id){
    list_node_t * now;
    list_node_t * ret;
    ret = NULL;
    if(ready_queue.prev == &ready_queue){
        return NULL;
    }
    int flag = 0;
    for(now = ready_queue.prev; now != &(ready_queue); now=now->prev){
        if(((LIST_to_PCB(now)->hart_mask) == 3 || (LIST_to_PCB(now)->hart_mask) == hart_id + 1)){
            ret = now;
            flag = 1;
            break;
        }
    }
    return ret;
    // else{
    //     if(hart_id != 1 && pid0_pcb_0.status == TASK_READY)
    //         return &(pid0_pcb_0.list);
    //     if(hart_id != 2 && pid0_pcb_1.status == TASK_READY)
    //         return &(pid0_pcb_1.list);
    // }
    // for(now = ready_queue.prev; now != &(ready_queue); now=now->prev){
    //     if(((LIST_to_PCB(now)->hart_mask) == 3 || (LIST_to_PCB(now)->hart_mask) == hart_id + 1)){
    //         ret = now;
    //         flag = 1;
    //         break;
    //     }
    // }
    // return ret;
    
}

/* global process id */
pid_t process_id = 1;

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    
    // __asm__ __volatile__("csrr x0, mscratch\n");//check M ver
    // __asm__ __volatile__("csrr x0, sscratch\n");//check S ver
    int hart_id = get_current_cpu_id();
    // if(hart_id ==1 && current_running_1->pid!=NUM_MAX_TASK){
    //     printk("%d!\n",current_running_1->pid);
    // }
    current_running = hart_id ? &current_running_1 : &current_running_0;
    check_sleeping();
    // TODO: [p2-task1] Modify the current_running pointer.
    
    pcb_t * prev_run = (*current_running);
    list_node_t * prev_list = find_next_proc(hart_id);

    // if((*current_running)->pid == 1 && hart_id ==1){
    //     printk("SHELL IN1! %d\n",prev_list);
    // }    

    if(prev_list == NULL){
        if(prev_run->pid < NUM_MAX_TASK && prev_run->status == TASK_EXITED){
            list_del(&((*current_running)->list));
            while(prev_run->wait_list.prev != &(prev_run->wait_list)){
                do_unblock(prev_run->wait_list.prev);
            }
            while(prev_run->lock_list.prev != &(prev_run->lock_list)){
                do_mutex_lock_release((mutex_lock_t *)prev_run->lock_list.prev);
            }
            *current_running = hart_id ? &(pid0_pcb_1) : &(pid0_pcb_0);
        }
        else{
            if((*current_running)->status == TASK_RUNNING && ((*current_running)->hart_mask == hart_id+1 || (*current_running)->hart_mask == 3)){
                return;
            }
            else{
                *current_running = hart_id ? &(pid0_pcb_1) : &(pid0_pcb_0);
            }
        }        
    }
    else{
        list_del(prev_list);
        (*current_running) = LIST_to_PCB(prev_list);
    }
    
    (*current_running)->status = TASK_RUNNING;
    if(prev_run->status == TASK_RUNNING && prev_run->pid < NUM_MAX_TASK){
        prev_run->status = TASK_READY;
        list_add(&(prev_run->list),&ready_queue);
    }else{
        if(prev_run->pid < NUM_MAX_TASK && prev_run->status == TASK_EXITED){
            while(prev_run->wait_list.prev != &(prev_run->wait_list)){
                do_unblock(prev_run->wait_list.prev);
            }
            while(prev_run->lock_list.prev != &(prev_run->lock_list)){
                do_mutex_lock_release((mutex_lock_t *)prev_run->lock_list.prev);
            }
        }
    }
    // if((*current_running)->pid == 1){
    //     printk("SHELL IN! %d %d\n",(*current_running)->hart_mask,hart_id);
    // }
    // TODO: [p2-task1] switch_to current_running
    switch_to(prev_run,(*current_running));
    // screen_reflush();
    // printl("IN!");
    // printl("%d %d %d %d\n",((ptr_t *)1380982744)[0],((ptr_t *)1380982744)[1],((ptr_t *)1380982744)[2],((ptr_t *)1380982744)[3]);
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
    current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
    (*current_running)->status = TASK_BLOCKED;
    (*current_running)->wakeup_time = get_ticks() + sleep_time * time_base;
    list_del(&((*current_running)->list));
    list_add(&((*current_running)->list), &sleep_queue);
    do_scheduler(); 
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    list_del(pcb_node);
    list_add(pcb_node,queue);
    pcb_t *pcb_now = LIST_to_PCB(pcb_node);
    pcb_now->status = TASK_BLOCKED;
    do_scheduler();
}

void no_yield_block(list_node_t *pcb_node, list_head *queue)
{
    list_del(pcb_node);
    list_add(pcb_node,queue);
    pcb_t *pcb_now = LIST_to_PCB(pcb_node);
    pcb_now->status = TASK_BLOCKED;    
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    list_del(pcb_node);
    list_add(pcb_node,&ready_queue);
    pcb_t *pcb_now = LIST_to_PCB(pcb_node);
    pcb_now->status = TASK_READY;
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



void do_exit(void)
{
    /* TODO [P3-TASK1] exec exit kill waitpid ps*/
    current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;

    (*current_running)->status = TASK_EXITED;
    list_del(&((*current_running)->list));
    while(((*current_running)->wait_list).prev != &((*current_running)->wait_list))
        do_unblock((((*current_running)->wait_list).prev));
    while(((*current_running)->lock_list).prev != &((*current_running)->lock_list))
        do_mutex_lock_release((mutex_lock_t *)((*current_running)->lock_list).prev);
    do_scheduler();
}

int do_kill(pid_t pid)
{
    /* TODO [P3-TASK1] exec exit kill waitpid ps*/
    int id = pid - 1;
    if(id < 0 || id >= NUM_MAX_TASK || pcb[id].status == TASK_EXITED){
        printk("\n> [Kill] Pid number not in use. \n\r");
        return 0;
    }
    if(id == 0){
        printk("\n> [Kill] Can not kill the shell. \n\r");
        return 0;
    }
    if(pcb[id].status != TASK_RUNNING){
        list_del(&(pcb[id].list));
        while(pcb[id].wait_list.prev != &(pcb[id].wait_list))
            do_unblock(pcb[id].wait_list.prev);
        while(pcb[id].lock_list.prev != &(pcb[id].lock_list))
            do_mutex_lock_release((mutex_lock_t *)pcb[id].lock_list.prev);
    }
    pcb[id].status = TASK_EXITED;
    return 1;
}

int do_waitpid(pid_t pid)
{
    /* TODO [P3-TASK1] exec exit kill waitpid ps*/
    current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
    int id = pid - 1;
    if(id < 0 || id >= NUM_MAX_TASK ){
        printk("> [Waitpid] Pid number not in use. \n\r");
        return 0;
    }
    if(id == 0){
        printk("> [Waitpid] Can not wait the shell to kill. \n\r");
        return 0;
    }
    if(pcb[id].status == TASK_EXITED){
        return 0;
    }
    do_block((&((*current_running)->list)),(&(pcb[id].wait_list)));
    return 1;
}

void do_process_show(void)
{
    /* TODO [P3-TASK1] exec exit kill waitpid ps*/
    current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
    screen_move_cursor(0,pcb[0].cursor_y);
    printk("\n[Process Table]\n");
    printk("Core 0 %s %d\n",current_running_0->name,current_running_0->status == TASK_RUNNING);
    printk("Core 1 %s %d\n",current_running_1->name,current_running_1->status == TASK_RUNNING);
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
    // screen_move_cursor(0,pcb[0].cursor_y+j+1);
}
pid_t do_getpid(void){
    /* TODO [P3-TASK1] exec exit kill waitpid ps*/
    current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
    return (*current_running)->pid;
}

void do_task_set(pid_t pid, int mask){
    int id = pid - 1;
    if(id < 0 || id >= NUM_MAX_TASK ){
        printk("> [Taskset] Pid number not in use. \n\r");
        return 0;
    }
    if(mask > 3 || mask < 1){
        printk("> [Taskset] Core number not in use. \n\r");
        return 0;
    }
    if(pcb[id].status == TASK_EXITED)return;
    pcb[id].hart_mask = mask;
}
int do_task_set_p(int mask,char *name, int argc, char *argv[]){
    int pid = do_exec(name,argc,argv);
    do_task_set(pid,mask);
    return pid;
}