#include <os/list.h>
#include <os/sched.h>
#include <type.h>

uint64_t time_elapsed = 0;
uint64_t time_base = 0;

uint64_t get_ticks()
{
    __asm__ __volatile__(
        "rdtime %0"
        : "=r"(time_elapsed));
    return time_elapsed;
}

uint64_t get_timer()
{
    return get_ticks() / time_base;
}

uint64_t get_time_base()
{
    return time_base;
}

void latency(uint64_t time)
{
    uint64_t begin_time = get_timer();

    while (get_timer() - begin_time < time);
    return;
}

void check_sleeping(void)
{
    // TODO: [p2-task3] Pick out tasks that should wake up from the sleep queue
    list_node_t *now = &sleep_queue;
    list_node_t *nxt = now->next;
    if(now->next == &now) return;
    uint64_t nowtick;
    for(now = now->next; now!= &sleep_queue; now = nxt){
        nowtick = get_ticks();
        nxt = now->next;
        if(LIST_to_PCB(now)->wakeup_time <= nowtick){
            list_del(now);
            list_add(now,&ready_queue);
            pcb_t *pcb_now = LIST_to_PCB(now);
            pcb_now->status = TASK_READY;
        }
    }
}