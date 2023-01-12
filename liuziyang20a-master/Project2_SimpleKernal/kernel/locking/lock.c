#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];
/*
typedef struct mutex_lock
{
    spin_lock_t lock;
    list_head block_queue;
    int key;
} mutex_lock_t;
*/

void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    int i;
    for(i=0;i<LOCK_NUM;i++){
        mlocks[i].lock.status=UNLOCKED;
        mlocks[i].block_queue.prev=&mlocks[i].block_queue;
        mlocks[i].block_queue.next=&mlocks[i].block_queue;
        mlocks[i].key=-1;
    }
}

void spin_lock_init(spin_lock_t *lock)
{
    /* TODO: [p2-task2] initialize spin lock */
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    return 0;
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
}

void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
}

int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */
    int i;
    int id=-1;
    for(i=0;i<LOCK_NUM;i++){
        if(mlocks[i].key==key){
            id=i;
            break;
        }
    }
    if(id!=-1)
        return id;
    for(i=0;i<LOCK_NUM;i++){
        if(mlocks[i].key==-1){
            id=i;
            mlocks[i].key=key;
            break;
        }
    }
    return id;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    // printl("ACQUIRE!!!! %d\n",current_running->pid);
    mutex_lock_t *lock_now = &(mlocks[mlock_idx]);
    // printl("IN!!!! %d %d\n",lock_now->key,lock_now->lock.status);
    while(lock_now->lock.status==LOCKED){
        do_block(&(current_running->list),&lock_now->block_queue);
    }
        
    if(lock_now->lock.status!=LOCKED)
        lock_now->lock.status=LOCKED;
    // printl("LOCKED!!!! %d\n",current_running->pid);
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    mutex_lock_t *lock_now = &(mlocks[mlock_idx]);
    if(lock_now->block_queue.prev!=&(lock_now->block_queue)){
        do_unblock(lock_now->block_queue.prev);
    }
    lock_now->lock.status=UNLOCKED;
    do_scheduler();
}
