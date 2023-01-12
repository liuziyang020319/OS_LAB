#include <os/task.h>
#include <os/string.h>
#include <os/bios.h>
#include <type.h>

unsigned long tasknum;
uint64_t load_task_img(char * taskname)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
    //p1-task3
    unsigned INC = 0x00010000;
    unsigned BASE = 0x52000000;
    /* unsigned mem_address, unsigned num_of_blocks, unsigned block_id*/
    // unsigned ENTRY_POINT = BASE + (INC * taskid);
    // unsigned BLOCK_ID = (taskid+1)*15+1;
    // bios_sdread(ENTRY_POINT,15,BLOCK_ID);
    
    //task 4
    //GAP:
    //4 byte for num task
    //44 byte for a taskinfo 
    for(int id=0;id<tasknum;++id){
        if(strcmp(tasks[id].name,taskname)==0){
            bios_putstr("FIND IT : ");
            bios_putstr(tasks[id].name);
            unsigned long offset = tasks[id].offset;
            unsigned long taskid = tasks[id].TASK_ID;
            unsigned long APPSIZE = tasks[id].SIZE;
            unsigned long ENTRY_POINT = BASE + (INC * taskid);
            unsigned long BLOCK_ID = offset/512;
            unsigned long BLOCK_OFFSET = offset - BLOCK_ID*512;
            unsigned long BLOCK_SIZE = (offset+APPSIZE)/512-(offset/512)+1;
            bios_sdread(ENTRY_POINT,BLOCK_SIZE,BLOCK_ID);
            long to;
            long from;
            for(int i=0;i<APPSIZE;++i){
                to   = ENTRY_POINT + i;
                from = ENTRY_POINT + i + BLOCK_OFFSET;
                char * toptr = to;
                char * fromptr = from;
                *toptr = *fromptr; 
            }
            return ENTRY_POINT;
        }
    }
    return -1;
}