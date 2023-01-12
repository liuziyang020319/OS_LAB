#include <atomic.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/lock.h>
#include <os/kernel.h>



void smp_init()
{
    /* TODO: P3-TASK3 multicore*/
}

void wakeup_other_hart()
{
    /* TODO: P3-TASK3 multicore*/
}

uint32_t kernel_lock = 0;

void lock_kernel()
{
    /* TODO: P3-TASK3 multicore*/
    uint32_t key = 1;
    while(key){key = atomic_swap(key, (ptr_t)&kernel_lock);}
}

void unlock_kernel()
{
    /* TODO: P3-TASK3 multicore*/
    kernel_lock = 0;
}
