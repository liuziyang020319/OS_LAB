#include <common.h>
#include <asm.h>
#include <os/bios.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>

#define VERSION_BUF 50
#define TASK_NUM_POINT 0x502001fe
#define BASE 0x52000000

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

// Task info array
task_info_t tasks[TASK_MAXNUM];
unsigned long tasknum;
unsigned char batinfo[1024];

static int bss_check(void)
{
    for (int i = 0; i < VERSION_BUF; ++i)
    {
        if (buf[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}

static void init_bios(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())BIOS_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
}



// void PRINT_INT(int x){
//     int x_num[10];
//     x_num[0]=0;
    
//     while(x){
//         x_num[++x_num[0]]=x%10;
//         x/=10;
//     }

//     while(x_num[0]){
//         bios_putchar('0'+x_num[x_num[0]]);
//         x_num[0]--;
//     }
//     bios_putchar('\r');
// }

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    unsigned long *OFFSET_POINT = 0x502001f0;
    unsigned long OFFSET = * OFFSET_POINT;
    unsigned long * entryptr = BASE + OFFSET;
    tasknum = *entryptr;
    entryptr += 1;
    for(int idx=0;idx<tasknum;++idx){
        for(int i=0;i<32;++i){
            char * chptr = entryptr;
            char ch = *chptr;
            tasks[idx].name[i]=ch;
            int temp = entryptr;
            temp++;
            entryptr = temp;
        }
        int TASK_ID = *entryptr;
        tasks[idx].TASK_ID=TASK_ID;
        entryptr+=1;
        int TASK_OFFSET = *entryptr;
        tasks[idx].offset=TASK_OFFSET;
        entryptr+=1;
        int TASK_SIZ = *entryptr;
        tasks[idx].SIZE=TASK_SIZ;
        entryptr+=1;
    }
}

int main(void)
{
    // Check whether .bss section is set to zero
    int check = bss_check();

    // Init jump table provided by BIOS (ΦωΦ)
    init_bios();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Output 'Hello OS!', bss check result and OS version
    char output_str[] = "bss check: _ version: _\n\r";
    char output_val[2] = {0};
    int i, output_val_pos = 0;

    output_val[0] = check ? 't' : 'f';
    output_val[1] = version + '0';
    for (i = 0; i < sizeof(output_str); ++i)
    {
        buf[i] = output_str[i];
        if (buf[i] == '_')
        {
            buf[i] = output_val[output_val_pos++];
        }
    }

    bios_putstr("Hello OS!\n\r");
    bios_putstr(buf);

    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.
    
    //TASK 2
    /*volatile int now;
    while (1){
        now=port_read_ch();
        while(now==-1){
            now=port_read_ch();
        }
        port_write_ch(now);
    }*/
    //TASK 3
    /*
    while(1){
        volatile int ID = 0;    
        volatile int LEGAL=1;
        volatile int ENTRY_ID;
        volatile int x = 0;
        while(1){
            x=port_read_ch();
            while(x==-1){
                x=port_read_ch();
            }  
            if(x=='\n')break;
            if(x=='\r')break;
            if(x>'9'||x<'0'){
                bios_putstr("WRONG TASK ID FORM\n\r");  
                LEGAL = 0;
                break;
            }   
                
            ID = ID * 10 + x - '0';
        }

        if(LEGAL && ID >= tasknum){
            bios_putstr("WRONG TASK ID -- too big\n\r");
            LEGAL = 0;
        }

        if(LEGAL){             
            bios_putstr("TASK STARTED\n\r");
            ENTRY_ID = load_task_img(ID);
            ((void(*)())ENTRY_ID)();
            bios_putstr("TASK FINISHED\n\r");
        }  
    }
    */
   /*
    while(1){
        char ch[32]={};
        volatile int x = 0; 
        volatile int len = 0;
        volatile int LEGAL = 1;
        volatile int ENTRY_POINT;
        while(1){
            x=port_read_ch();
            while(x==-1){
                x=port_read_ch();
            }  
            port_write_ch(x);
            if(x=='\n')break;
            if(x=='\r')break;
            ch[len++]=x;
            if(len>31){
                LEGAL = 0;
                break;
            }
        }
        ch[len]='\n';
        if(LEGAL){    
            ENTRY_POINT = load_task_img(ch);
            if(ENTRY_POINT == -1){
                bios_putstr("WRONG TASK NAME !\n\r");
            }
            else{
                bios_putstr("TASK STARTED\n\r");
                ((void(*)())ENTRY_POINT)();
                bios_putstr("TASK FINISHED\n\r"); 
            }           
        }
    }
    */
    // volatile int ENTRY_POINT;
    // ENTRY_POINT = load_task_img("2048");
    // bios_putstr("TASK STARTED\n\r");
    // ((void(*)())ENTRY_POINT)();
    // bios_putstr("TASK FINISHED\n\r"); 
    while(1){
        char ch[32]={};
        volatile int x = 0; 
        volatile int len = 0;
        volatile int LEGAL = 1;
        volatile int ENTRY_POINT;
        while(1){
            x=port_read_ch();
            while(x==-1){
                x=port_read_ch();
            }  
            port_write_ch(x);
            if(x=='\n')break;
            if(x=='\r')break;
            ch[len++]=x;
            if(len>31){
                LEGAL = 0;
                break;
            }
        }
        ch[len]='\n';
        if(LEGAL){    
            for(int id=0;id<tasknum;++id){
                if(tasks[id].TASK_ID!=-1)continue;
                if(strcmp(tasks[id].name,ch)==0){
                    bios_putstr("BSS FOUND : ");
                    bios_putstr(tasks[id].name);
                    unsigned long offset   = tasks[id].offset;
                    unsigned long APPSIZE  = tasks[id].SIZE;
                    unsigned long MEMADDR  = 0x52000000;
                    unsigned long BLOCK_ID = offset/512;
                    unsigned long BLOCK_OFFSET = offset - BLOCK_ID*512;
                    unsigned long BLOCK_SIZE = (offset+APPSIZE)/512 - (offset/512)+1;
                    bios_sdread(MEMADDR,BLOCK_SIZE,BLOCK_ID);
                    long from; 
                    for(int i=0;i<APPSIZE;++i){
                        from = MEMADDR + i + BLOCK_OFFSET;
                        char * fromptr = from;
                        *(batinfo + i) = *fromptr;
                    } 
                    *(batinfo + APPSIZE)='\n';
                    char taskname[32];
                    int tasklen=0;
                    for(int i=0;i<32;++i)taskname[i]=0;
                    for(int i=0;i<=APPSIZE;++i){
                        if(batinfo[i]=='\n'){
                            taskname[tasklen]='\n';
                            bios_putstr(taskname);
                            ENTRY_POINT = load_task_img(taskname);
                            if(ENTRY_POINT == -1){
                                bios_putstr("WRONG TASK NAME !\n\r");
                                bios_putstr("NEXT TASK CHECK !\n\r");
                            }
                            else{
                                bios_putstr("TASK STARTED\n\r");
                                ((void(*)())ENTRY_POINT)();
                                bios_putstr("TASK FINISHED\n\r"); 
                            }
                            tasklen=0;
                            for(int j=0;j<32;++j)taskname[j]=0;
                        }
                        else{
                            taskname[tasklen++]=*(batinfo + i);
                        }
                    }
                }
            }
        }
    }
    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1){
        asm volatile("wfi");
    }
    return 0;
}
