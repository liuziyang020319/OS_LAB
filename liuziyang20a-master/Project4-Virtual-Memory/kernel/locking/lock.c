#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <os/string.h>
#include <os/smp.h>
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
        mlocks[i].lock_id.prev=&mlocks[i].lock_id;
        mlocks[i].lock_id.next=&mlocks[i].lock_id;
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

void do_mutex_lock_init_ptr(mutex_lock_t * lock_now){
    lock_now->lock_id.prev = &(lock_now->lock_id);
    lock_now->lock_id.next = &(lock_now->lock_id);
    lock_now->lock.status = UNLOCKED;
    lock_now->block_queue.prev = &(lock_now->block_queue);
    lock_now->block_queue.next = &(lock_now->block_queue);
    lock_now->key = -1;
}

void do_mutex_lock_acquire(mutex_lock_t * lock_now)
{
    /* TODO: [p2-task2] acquire mutex lock */
    // printl("ACQUIRE!!!! %d\n",current_running->pid);
    // printl("IN!!!! %d %d\n",lock_now->key,lock_now->lock.status);
    current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;

    while(lock_now->lock.status==LOCKED){
        do_block(&((*current_running)->list),&lock_now->block_queue);
    }
        
    if(lock_now->lock.status!=LOCKED)
        lock_now->lock.status=LOCKED;
    list_add(&(lock_now->lock_id),&((*current_running)->lock_list));
}

void do_mutex_lock_release(mutex_lock_t * lock_now)
{
    /* TODO: [p2-task2] release mutex lock */
    list_del(&(lock_now->lock_id));
    if(lock_now->block_queue.prev!=&(lock_now->block_queue) ){
        do_unblock(lock_now->block_queue.prev);
    }
    lock_now->lock.status=UNLOCKED;
    // do_scheduler();
}

void do_mutex_lock_acquire_id(int mlock_idx)
{
    mutex_lock_t *lock_now = &(mlocks[mlock_idx]);
    do_mutex_lock_acquire(lock_now);
}

void do_mutex_lock_release_id(int mlock_idx)
{
    mutex_lock_t *lock_now = &(mlocks[mlock_idx]);
    do_mutex_lock_release(lock_now);
}

barrier_t barriers[BARRIER_NUM];

void init_barriers(void){
    int i;
    for(i=0; i < BARRIER_NUM; i++){
        barriers[i].queue_siz = 0;
        barriers[i].key = -1;
        barriers[i].target = 0;
        barriers[i].block_queue.prev = &(barriers[i].block_queue);
        barriers[i].block_queue.next = &(barriers[i].block_queue);
        do_mutex_lock_init_ptr(&(barriers[i].lock));
    }
}

int do_barrier_init(int key,int goal){
    int i;
    int id=-1;
    for(i=0; i < BARRIER_NUM; i++){
        if(barriers[i].key==key){
            id=i;
            break;
        }
    }
    if(id!=-1)return -1;
    for(i=0; i < BARRIER_NUM; i++){
        if(barriers[i].key == -1){
            id = i;
            barriers[id].key = key;
            barriers[id].target = goal;
            break;
        }
    }
    return id;
}

void do_barrier_wait(int bar_idx){
    current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
    barrier_t * barrier_now = &(barriers[bar_idx]);
    do_mutex_lock_acquire(&(barrier_now->lock));
    barrier_now->queue_siz++;
    if(barrier_now->queue_siz >= barrier_now->target){
        while(barrier_now->block_queue.prev!=&(barrier_now->block_queue)){
            do_unblock((barrier_now->block_queue.prev));
        }
        barrier_now->queue_siz = 0;
        do_mutex_lock_release(&(barrier_now->lock));
        return;
    }
    else{
        no_yield_block(&((*current_running)->list),&(barrier_now->block_queue));
        do_mutex_lock_release(&(barrier_now->lock));
        do_scheduler();
    }
}

void do_barrier_destroy(int bar_idx){
    barrier_t * barrier_now = &(barriers[bar_idx]);
    do_mutex_lock_acquire(&(barrier_now->lock));
    while(barrier_now->block_queue.prev!=&(barrier_now->block_queue)){
        do_unblock((barrier_now->block_queue.prev));
    }
    barrier_now->queue_siz=0;
    barrier_now->key=-1;
    barrier_now->target=0;
    do_mutex_lock_release(&(barrier_now->lock));
}

condition_t conds[CONDITION_NUM];
void init_conditions(void){
    for(int i=0; i < CONDITION_NUM; i++){
        conds[i].key = -1;
        do_mutex_lock_init_ptr(&(conds[i].cond_lock));
        conds[i].block_queue.prev = &(conds[i].block_queue);
        conds[i].block_queue.next = &(conds[i].block_queue);
        conds[i].mutex_lock = NULL;
    }
}

int do_condition_init(int key){
    int i;
    int id=-1;
    for(i=0; i < CONDITION_NUM; i++){
        if(conds[i].key == key){
            id = i;
            break;    
        }
    }    
    if(id!=-1)return -1;
    for(i=0; i < CONDITION_NUM; i++){
        if(conds[i].key == -1){
            id = i;
            conds[i].key = key;
            break;
        }
    }
    return id;
}

void do_condition_wait(int cond_idx, int mutex_idx){
    current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
    mutex_lock_t * mutex_now = &(mlocks[mutex_idx]);
    condition_t * cond_now = &(conds[cond_idx]);

    // printl("pid = %d, wait\n",current_running->pid);
    
    do_mutex_lock_acquire(&(cond_now->cond_lock));
    cond_now->mutex_lock = mutex_now;
    do_mutex_lock_release(mutex_now);
    no_yield_block(&((*current_running)->list),&(cond_now->block_queue));
    do_mutex_lock_release(&(cond_now->cond_lock));

    do_scheduler();
    do_mutex_lock_acquire(mutex_now);
}

void do_condition_wait_ptr(int cond_idx, mutex_lock_t * mutex_now){
    current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
    condition_t * cond_now = &(conds[cond_idx]);
    
    do_mutex_lock_acquire(&(cond_now->cond_lock));
    cond_now->mutex_lock = mutex_now;
    do_mutex_lock_release(mutex_now);
    no_yield_block(&((*current_running)->list),&(cond_now->block_queue));
    do_mutex_lock_release(&(cond_now->cond_lock));

    do_scheduler();
    do_mutex_lock_acquire(mutex_now); 
}

void do_condition_signal(int cond_idx){
    condition_t * cond_now = &(conds[cond_idx]);

    do_mutex_lock_acquire(&(cond_now->cond_lock));
    if(cond_now->block_queue.prev != &(cond_now->block_queue))
        do_unblock((cond_now->block_queue.prev));
    do_mutex_lock_release(&(cond_now->cond_lock));
}

void do_condition_broadcast(int cond_idx){
    // printl("IN BROADCAST!\n");
    condition_t * cond_now = &(conds[cond_idx]);
    do_mutex_lock_acquire(&(cond_now->cond_lock));
    while(cond_now->block_queue.prev != &(cond_now->block_queue))
        do_unblock((cond_now->block_queue.prev));    
    do_mutex_lock_release(&(cond_now->cond_lock));
}

void do_condition_destroy(int cond_idx){
    condition_t * cond_now = &(conds[cond_idx]);
    do_mutex_lock_acquire(&(cond_now->cond_lock));    
    cond_now->key = -1;
    while(cond_now->block_queue.prev != &(cond_now->block_queue))
        do_unblock((cond_now->block_queue.prev));    
    cond_now->mutex_lock = NULL;     
    do_mutex_lock_release(&(cond_now->cond_lock));
}

mailbox_t mailboxs[MBOX_NUM];

void init_mbox(){
    for(int i=0; i < MBOX_NUM; i++){
        mailboxs[i].siz  = 0;
        mailboxs[i].head = 0;
        mailboxs[i].tail = 0;
        mailboxs[i].using = 0;
        mailboxs[i].name[0] = '\0';
        do_mutex_lock_init_ptr(&(mailboxs[i].lock));
        mailboxs[i].cond_idx = 0;
    }
}

int do_mbox_open(char *name){
    int i;
    int id = -1;
    for(i=0; i < MBOX_NUM; i++){
        if(strcmp(mailboxs[i].name,name) == 0){
            id = i;
            mailboxs[i].using++;
            break;
        }
    }
    if(id != -1)return id;
    for(i=0; i < MBOX_NUM; i++){
        if(mailboxs[i].name[0] == '\0'){
            strcpy(mailboxs[i].name,name);
            mailboxs[i].cond_idx = do_condition_init(i + 1000);
            id = i;
            break;
        }
    }
    return id;
}

void do_mbox_close(int mbox_idx){
    mailbox_t * mailbox_now = &(mailboxs[mbox_idx]);
    do_mutex_lock_acquire(&(mailbox_now->lock));
    mailbox_now->using -- ;
    if(mailbox_now->using == 0){
        mailbox_now->siz = 0;
        mailbox_now->head = 0;
        mailbox_now->tail = 0;
        mailbox_now->cond_idx = 0;
        mailbox_now->name[0] = '\0'; 
    }
    do_mutex_lock_release(&(mailbox_now->lock));
}

char *mboxncpy(char *dest, const char *src, int n)
{
    char *tmp = dest;

    while (n-- > 0) {
        *dest++ = *src++;
    }

    return tmp;
}

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
        // printl("recv buffer=%s,msg=%s,len=%d",msg,((*mailbox_now).buffer + mailbox_now->head),msg_length);
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
