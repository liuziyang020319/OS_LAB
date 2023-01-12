#ifndef __INCLUDE_TASK_H__
#define __INCLUDE_TASK_H__

#include <type.h>

#define TASK_MEM_BASE    0x52000000
#define TASK_MAXNUM      16
#define TASK_SIZE        0x10000

/* TODO: [p1-task4] implement your own task_info_t! */
typedef struct {
    char name[32];//ANSI
    long TASK_ID;//MAYBE NOT NECESSARY
    long offset;
    long SIZE;
} task_info_t;

extern task_info_t tasks[TASK_MAXNUM];
extern unsigned long tasknum;
extern unsigned char batinfo[1024];
#endif