#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* current running task PCB */
pcb_t * volatile current_running;

/* global process id */
pid_t process_id = 1;

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    
    // __asm__ __volatile__("csrr x0, mscratch\n");//check M ver
    // __asm__ __volatile__("csrr x0, sscratch\n");//check S ver
    check_sleeping();
    // TODO: [p2-task1] Modify the current_running pointer.
    pcb_t * prev_run = current_running;
    if(ready_queue.prev == &ready_queue){
        return ;
    }
    list_node_t * prev_list = ready_queue.prev;
    list_node_t * now = prev_list;
    // screen_move_cursor(0,20);
    for(;now!=(&ready_queue);now=now->prev){
        printl("%d ",((pcb_t *)((char *)(now) - 16))->pid);
    }
    printl("\n");
    list_del(prev_list);
    current_running = ((pcb_t *)((char *)(prev_list) - 16));
    current_running->status = TASK_RUNNING;
    if(prev_run != 0 && prev_run->status == TASK_RUNNING){
        prev_run->status = TASK_READY;
        list_add(&(prev_run->list),&ready_queue);
    }
    // TODO: [p2-task1] switch_to current_running
    switch_to(prev_run,current_running);
    screen_move_cursor(current_running->cursor_x,current_running->cursor_y);
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
    current_running->status = TASK_BLOCKED;
    current_running->wakeup_time = get_ticks() + sleep_time * time_base;
    list_del(&(current_running->list));
    list_add(&(current_running->list), &sleep_queue);
    do_scheduler(); 
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    list_del(pcb_node);
    list_add(pcb_node,queue);
    pcb_t *pcb_now = ((pcb_t *)((char *)(pcb_node) - 16));
    pcb_now->status = TASK_BLOCKED;
    // printl("BLOCKED ID = %d\n",pcb_now->pid);
    do_scheduler();
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    list_del(pcb_node);
    list_add(pcb_node,&ready_queue);
    pcb_t *pcb_now = ((pcb_t *)((char *)(pcb_node) - 16));
    pcb_now->status = TASK_READY;
}
