#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <os/mm.h>
#include <type.h>

unsigned long tasknum;
#define INIT_KERNEL_STACK 0xffffffc052000000

uint64_t load_task_img(char * taskname, uint64_t pgdir)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
    //p1-task3
    // unsigned INC = 0x00010000;
    // unsigned BASE = 0x52000000;
    // /* unsigned mem_address, unsigned num_of_blocks, unsigned block_id*/
    // // unsigned ENTRY_POINT = BASE + (INC * taskid);
    // // unsigned BLOCK_ID = (taskid+1)*15+1;
    // // bios_sdread(ENTRY_POINT,15,BLOCK_ID);
    
    // //task 4
    // //GAP:
    // //4 byte for num task
    // //44 byte for a taskinfo 
    // for(int id=0;id<tasknum;++id){
    //     if(strcmp(tasks[id].name,taskname)==0){
    //         // bios_putstr("FIND IT : ");
    //         // bios_putstr(tasks[id].name);
    //         unsigned long offset = tasks[id].offset;
    //         unsigned long taskid = tasks[id].TASK_ID;
    //         unsigned long APPSIZE = tasks[id].SIZE;
    //         unsigned long ENTRY_POINT = taskid;
    //         unsigned long BLOCK_ID = offset/512;
    //         unsigned long BLOCK_OFFSET = offset - BLOCK_ID*512;
    //         unsigned long BLOCK_SIZE = (offset+APPSIZE)/512-(offset/512)+1;
    //         bios_sdread(ENTRY_POINT,BLOCK_SIZE,BLOCK_ID);
    //         long to;
    //         long from;
    //         for(int i=0;i<APPSIZE;++i){
    //             to   = ENTRY_POINT + i;
    //             from = ENTRY_POINT + i + BLOCK_OFFSET;
    //             char * toptr = to;
    //             char * fromptr = from;
    //             *toptr = *fromptr; 
    //         }
    //         return ENTRY_POINT;
    //     }
    // }
    uint64_t ENTRY_POINT = 0;
    for(int id=0; id<tasknum; ++id){
        if(strcmp(tasks[id].name,taskname) == 0){
            uint64_t offset = tasks[id].offset;
            for(int i = 0; i < tasks[id].p_memsz; i += NORMAL_PAGE_SIZE, offset += NORMAL_PAGE_SIZE){
                if(i < tasks[id].p_filesz){
                    uint64_t va = alloc_page_helper(tasks[id].p_vaddr + i,pgdir,0);
                    uint64_t block_id = offset / 512;
                    uint64_t block_offset = offset - block_id * 512;
                    uint64_t SIZENOW = (tasks[id].p_filesz - i >= NORMAL_PAGE_SIZE) ? NORMAL_PAGE_SIZE : tasks[id].p_filesz - i;
                    uint64_t block_size = (offset + SIZENOW)/512 - (offset/512) + 1;
                    // printl("va: %ld\n",va);
                    uint64_t bias = va - INIT_KERNEL_STACK;
                    bios_sdread(INIT_KERNEL_STACK,block_size,block_id);
                    uint64_t to;
                    uint64_t from;
                    for(int j=0; j < SIZENOW; ++j){
                        to = va + j;
                        from = va + j + block_offset - bias;
                        char * toptr = to;
                        char * fromptr = from;
                        *toptr = *fromptr;
                    }
                    if(SIZENOW != NORMAL_PAGE_SIZE){
                        for(int j=SIZENOW; j < NORMAL_PAGE_SIZE; ++j){
                            to = va + j;
                            char * toptr = to;
                            *toptr = 0;
                        }
                    }
                }
                else{
                    uint64_t va = alloc_page_helper(tasks[id].p_vaddr + i,pgdir,0);
                    uint64_t to;
                    for(int j=0; j < NORMAL_PAGE_SIZE; ++j){
                        to = va + j;
                        char * toptr = to;
                        *toptr = 0;
                    }
                }
            }
            return tasks[id].p_vaddr;
        }
    }

    return -1;
}