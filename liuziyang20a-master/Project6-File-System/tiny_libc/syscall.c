#include <syscall.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

static const long IGNORE = 0L;

static long invoke_syscall(long sysno, long arg0, long arg1, long arg2,
                           long arg3, long arg4)
{
    /* TODO: [p2-task3] implement invoke_syscall via inline assembly */
    long ret=0;
    /*asm volatile("mv a7, %[sysno]\n\t"
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
                );*/
    asm volatile(
        "mv a7, %1\n"
        "mv a0, %2\n"
        "mv a1, %3\n"
        "mv a2, %4\n"
        "mv a3, %5\n"
        "mv a4, %6\n"
        "ecall\n"
        "mv %0, a0\n"
        :"=r"(ret)
        :"r"(sysno),"r"(arg0),"r"(arg1),"r"(arg2),"r"(arg3),"r"(arg4)
        :"a0","a1","a2","a3","a4","a7"
    );
    return ret;
}

void sys_yield(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_yield */
    invoke_syscall(SYSCALL_YIELD,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_move_cursor(int x, int y)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_move_cursor */
    invoke_syscall(SYSCALL_CURSOR,x,y,IGNORE,IGNORE,IGNORE);
}

void sys_write(char *buff)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_write */
    invoke_syscall(SYSCALL_WRITE,(uintptr_t)buff,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_reflush(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_reflush */
    invoke_syscall(SYSCALL_REFLUSH,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_screen_clear(void)
{
    invoke_syscall(SYSCALL_CLEAR,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
}

int sys_mutex_init(int key)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_init */
    return invoke_syscall(SYSCALL_LOCK_INIT,key,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_mutex_acquire(int mutex_idx)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_acquire */
    invoke_syscall(SYSCALL_LOCK_ACQ,mutex_idx,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_mutex_release(int mutex_idx)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_release */
    invoke_syscall(SYSCALL_LOCK_RELEASE,mutex_idx,IGNORE,IGNORE,IGNORE,IGNORE);
}

long sys_get_timebase(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_timebase */
    return invoke_syscall(SYSCALL_GET_TIMEBASE,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
}

long sys_get_tick(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_tick */
    return invoke_syscall(SYSCALL_GET_TICK,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_sleep(uint32_t time)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_sleep */
    invoke_syscall(SYSCALL_SLEEP,time,IGNORE,IGNORE,IGNORE,IGNORE);
}

void pthread_create(pthread_t *thread, void (*start_routine)(void*), void *arg){
    invoke_syscall(SYSCALL_THREAD_CREATE,(uintptr_t)thread,(uintptr_t)start_routine,(uintptr_t)arg,IGNORE,IGNORE);
}

int pthread_join(pthread_t thread){
    while(1);
    return 0;   
}

void sys_task_set(pid_t pid, int mask){
    invoke_syscall(SYSCALL_TASK_SET,pid,mask,IGNORE,IGNORE,IGNORE);
}

int sys_task_set_p(int mask,char *name, int argc, char **argv){
    return invoke_syscall(SYSCALL_TASK_SET_P,mask,(uintptr_t)name,argc,(uintptr_t)argv,IGNORE);
}

uintptr_t sys_getva2pa(uintptr_t va){
    return invoke_syscall(SYSCALL_GETVA2PA,va,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_snapshot(uintptr_t va1,uintptr_t va2){
    invoke_syscall(SYSCALL_SNAPSHOT,va1,va2,IGNORE,IGNORE,IGNORE);
}

// S-core
// pid_t  sys_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2)
// {
//     /* TODO: [p3-task1] call invoke_syscall to implement sys_exec for S_CORE */
// }    
// A/C-core
pid_t sys_exec(char *name, int argc, char **argv)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exec */
    return invoke_syscall(SYSCALL_EXEC,(uintptr_t)name,argc,(uintptr_t)argv,IGNORE,IGNORE);
}

void sys_exit(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exit */
    invoke_syscall(SYSCALL_EXIT,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
}

int  sys_kill(pid_t pid)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_kill */
    return invoke_syscall(SYSCALL_KILL,pid,IGNORE,IGNORE,IGNORE,IGNORE);
}

int  sys_waitpid(pid_t pid)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_waitpid */
    return invoke_syscall(SYSCALL_WAITPID,pid,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_ps(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_ps */
    invoke_syscall(SYSCALL_PS,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
}

pid_t sys_getpid(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_getpid */
    return invoke_syscall(SYSCALL_GETPID,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
}

int  sys_getchar(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_getchar */
    return invoke_syscall(SYSCALL_GET_CHAR,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
}

int  sys_barrier_init(int key, int goal)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrier_init */
    return invoke_syscall(SYSCALL_BARR_INIT,key,goal,IGNORE,IGNORE,IGNORE);
}

void sys_barrier_wait(int bar_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_wait */
    invoke_syscall(SYSCALL_BARR_WAIT,bar_idx,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_barrier_destroy(int bar_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_destory */
    invoke_syscall(SYSCALL_BARR_DESTROY,bar_idx,IGNORE,IGNORE,IGNORE,IGNORE);
}

int sys_condition_init(int key)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_init */
    return invoke_syscall(SYSCALL_COND_INIT,key,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_condition_wait(int cond_idx, int mutex_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_wait */
    invoke_syscall(SYSCALL_COND_WAIT,cond_idx,mutex_idx,IGNORE,IGNORE,IGNORE);
}

void sys_condition_signal(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_signal */
    invoke_syscall(SYSCALL_COND_SIGNAL,cond_idx,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_condition_broadcast(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_broadcast */
    invoke_syscall(SYSCALL_COND_BROADCAST,cond_idx,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_condition_destroy(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_destroy */
    invoke_syscall(SYSCALL_COND_DESTROY,cond_idx,IGNORE,IGNORE,IGNORE,IGNORE);
}

int sys_mbox_open(char * name)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_open */
    return invoke_syscall(SYSCALL_MBOX_OPEN,(uintptr_t)name,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_mbox_close(int mbox_id)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_close */
    invoke_syscall(SYSCALL_MBOX_CLOSE,mbox_id,IGNORE,IGNORE,IGNORE,IGNORE);
}

int sys_mbox_send(int mbox_idx, void *msg, int msg_length)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_send */
    return invoke_syscall(SYSCALL_MBOX_SEND,mbox_idx,(uintptr_t)msg,msg_length,IGNORE,IGNORE);
}

int sys_mbox_recv(int mbox_idx, void *msg, int msg_length)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_recv */
    return invoke_syscall(SYSCALL_MBOX_RECV,mbox_idx,(uintptr_t)msg,msg_length,IGNORE,IGNORE);
}

void* sys_shmpageget(int key)
{
    /* TODO: [p4-task5] call invoke_syscall to implement sys_shmpageget */
    return invoke_syscall(SYSCALL_SHM_GET,key,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_shmpagedt(void *addr)
{
    /* TODO: [p4-task5] call invoke_syscall to implement sys_shmpagedt */
    invoke_syscall(SYSCALL_SHM_DT,(uintptr_t)addr,IGNORE,IGNORE,IGNORE,IGNORE);   
}

int sys_net_send(void *txpacket, int length)
{
    /* TODO: [p5-task1] call invoke_syscall to implement sys_net_send */
    return invoke_syscall(SYSCALL_NET_SEND,(uintptr_t)txpacket,length,IGNORE,IGNORE,IGNORE);
}

int sys_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    /* TODO: [p5-task2] call invoke_syscall to implement sys_net_recv */
    return invoke_syscall(SYSCALL_NET_RECV,(uintptr_t)rxbuffer,pkt_num,(uintptr_t)pkt_lens,IGNORE,IGNORE);
}

int sys_mkfs(void)
{
    // TODO [P6-task1]: Implement sys_mkfs
    return invoke_syscall(SYSCALL_FS_MKFS,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);  // sys_mkfs succeeds
}

int sys_statfs(void)
{
    // TODO [P6-task1]: Implement sys_statfs
    return invoke_syscall(SYSCALL_FS_STATFS,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);  // sys_statfs succeeds
}

int sys_cd(char *path)
{
    // TODO [P6-task1]: Implement sys_cd
    return invoke_syscall(SYSCALL_FS_CD,(uintptr_t)path,IGNORE,IGNORE,IGNORE,IGNORE);  // sys_cd succeeds
}

int sys_mkdir(char *path)
{
    // TODO [P6-task1]: Implement sys_mkdir
    return invoke_syscall(SYSCALL_FS_MKDIR,(uintptr_t)path,IGNORE,IGNORE,IGNORE,IGNORE);  // sys_mkdir succeeds
}

int sys_rmdir(char *path)
{
    // TODO [P6-task1]: Implement sys_rmdir
    return invoke_syscall(SYSCALL_FS_RMDIR,(uintptr_t)path,IGNORE,IGNORE,IGNORE,IGNORE);  // sys_rmdir succeeds
}

int sys_ls(char *path, int option)
{
    // TODO [P6-task1]: Implement sys_ls
    // Note: argument 'option' serves for 'ls -l' in A-core
    return invoke_syscall(SYSCALL_FS_LS,(uintptr_t)path,option,IGNORE,IGNORE,IGNORE);  // sys_ls succeeds
}

int sys_touch(char *path)
{
    // TODO [P6-task2]: Implement sys_touch
    return invoke_syscall(SYSCALL_FS_TOUCH,(uintptr_t)path,IGNORE,IGNORE,IGNORE,IGNORE);  // sys_touch succeeds
}

int sys_cat(char *path)
{
    // TODO [P6-task2]: Implement sys_cat
    return invoke_syscall(SYSCALL_FS_CAT,(uintptr_t)path,IGNORE,IGNORE,IGNORE,IGNORE);  // sys_cat succeeds
}

int sys_fopen(char *path, int mode)
{
    // TODO [P6-task2]: Implement sys_fopen
    return invoke_syscall(SYSCALL_FS_FOPEN,(uintptr_t)path,mode,IGNORE,IGNORE,IGNORE);  // return the id of file descriptor
}

int sys_fread(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement sys_fread
    return invoke_syscall(SYSCALL_FS_FREAD,fd,(uintptr_t)buff,length,IGNORE,IGNORE);  // return the length of trully read data
}

int sys_fwrite(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement sys_fwrite
    return invoke_syscall(SYSCALL_FS_FWRITE,fd,(uintptr_t)buff,length,IGNORE,IGNORE);  // return the length of trully written data
}

int sys_fclose(int fd)
{
    // TODO [P6-task2]: Implement sys_fclose
    return invoke_syscall(SYSCALL_FS_FCLOSE,fd,IGNORE,IGNORE,IGNORE,IGNORE);  // sys_fclose succeeds
}

int sys_ln(char *src_path, char *dst_path)
{
    // TODO [P6-task2]: Implement sys_ln
    return invoke_syscall(SYSCALL_FS_LN,(uintptr_t)src_path,(uintptr_t)dst_path,IGNORE,IGNORE,IGNORE);  // sys_ln succeeds 
}

int sys_rm(char *path)
{
    // TODO [P6-task2]: Implement sys_rm
    return invoke_syscall(SYSCALL_FS_RM,(uintptr_t)path,IGNORE,IGNORE,IGNORE,IGNORE);  // sys_rm succeeds 
}

int sys_lseek(int fd, int offset, int whence)
{
    // TODO [P6-task2]: Implement sys_lseek
    return invoke_syscall(SYSCALL_FS_LSEEK,fd,offset,whence,IGNORE,IGNORE);  // the resulting offset location from the beginning of the file
}
