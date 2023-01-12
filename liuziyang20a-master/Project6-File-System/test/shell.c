/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define SHELL_BEGIN 20
#define SHELL_WIDTH 80
static char buffer[SHELL_WIDTH];
static void init_display(){
    sys_screen_clear();
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");
    // printf("> root@UCAS_OS: ");
}
static void ps(){
    sys_ps();
}
int argc;
char name[32];
char current_dir[100];
char * argv[32];
char arg[32][32];

static void exec(char * name,int argc,char * argv[]){
    // printf("exec name =%s\n",name);
    // printf("exec argc = %d\n",argc);
    
    // for(int i=0;i<argc;i++){
    //     printf("%s\n",argv[i]);
    // }
    int yield = 0;
    if(name[0] == '\0')return;
    if(argv[argc-1][0] == '&'){
        argc--;
        yield = 1;
    }
    // printf("yield = %d \n",yield);
    int pid = sys_exec(name,argc,argv);
    if(pid != 0)
        printf("Info: execute %s successfully, pid = %d\n",name,pid);
    else
        printf("Info: execute %s ERROR\n",name);
    if(yield){
        return;
    }
    else{
        if(pid !=0 ){
            sys_waitpid(pid);
        }
    }

}
static void kill(pid_t pid){
    printf("kill %d\n",pid);
    sys_kill(pid);
}
static void shell_clear(){
    sys_screen_clear();
    init_display();
}
static void taskset_p(int mask, int pid){
    sys_task_set(pid,mask);
}
static void taskset(int mask,char * name,int argc,char * argv[]){
    // printf("mask %d ",mask);
    // printf("exec name =%s\n",name);
    // printf("exec argc = %d\n",argc);
    // int pid = sys_exec(name,argc,argv);
    int pid = sys_task_set_p(mask,name,argc,argv);
    // taskset_p(mask,pid);
    if(pid != 0)
        printf("Info: execute %s successfully, pid = %d\n",name,pid);
    else
        printf("Info: execute %s ERROR\n",name);
}
static void display_help(){
    sys_move_cursor(0,-1);
    printf("[HELP]                                         \n");
    printf("[ps]                             : process show\n");
    printf("[clear]                          : clear screen\n");
    printf("[exec] [task name] [argv] : execute task\n");
    printf("[kill] [pid]                     : kill process\n");
}

static void do_cd(char * path){
    char name[64];
    strcpy(name,path);
    int flag = sys_cd(path);
    if(flag == 1){
        if(strcmp(name,".") == 0)return;
        if(strcmp(name,"..") == 0){
            int len = strlen(current_dir);
            int idx;
            for(idx = len-1;;idx--){
                if(current_dir[idx] == '/'){
                    current_dir[idx] = 0;
                    return;
                }
            }
        }
        strcat(current_dir,"/");
        strcat(current_dir,name);
    }
}

static void do_ln(char *path){
    uint32_t idx = 0;
    int len = strlen(path);
    while(idx < len && path[idx] != ' ')idx++;
    if(idx < len){
        path[idx]=0;
        idx ++;
    }
    while(idx < len && path[idx] == ' ')idx++;
    if(idx == len)return;
    printf("src = %s dst = %s\n",path,path+idx);
    sys_ln(path,path+idx);
}

void do_ls(char *path){
    int len = strlen(path);
    if(strncmp(path,"ls -l",5) == 0){
        if(len == 5){
            sys_ls(0,1);
        }
        else{
            sys_ls(path + 6,1);            
        }
    }
    else{
        if(len == 2){
            sys_ls(0,0);
        }
        else{
            sys_ls(path+3,0);    
        }        
    }
}

void Calc(int len){
    if(len == 0)return;
    
    if(strcmp(buffer, "ps") == 0){
        ps();
        return;
    }
    else if(strcmp(buffer, "clear") == 0){
            shell_clear();
            return;
         }
         else{
            if(strncmp(buffer,"mkfs",4) == 0){
                sys_mkfs();
                return;
            }
            if(strncmp(buffer,"statfs",6) == 0){
                sys_statfs();
            }
            if(strncmp(buffer,"cd",2) == 0){
                do_cd(buffer+3);
            }
            if(strncmp(buffer,"mkdir",5) == 0){
                sys_mkdir(buffer+6);
            }
            if(strncmp(buffer,"rmdir",5) == 0){
                sys_rmdir(buffer+6);
            }
            if(strncmp(buffer,"ls",2) == 0){
                do_ls(buffer);
            }
            if(strncmp(buffer,"touch",5) == 0){
                sys_touch(buffer+6);
            }
            if(strncmp(buffer,"cat",3) == 0){
                sys_cat(buffer+4);
            }
            if(strncmp(buffer,"rm ",3) == 0){
                sys_rm(buffer+3);
            }
            if(strncmp(buffer,"ln",2) == 0){
                do_ln(buffer+3);
            }
            // if(len<=4)return;
            if(strncmp(buffer,"taskset",7) == 0){
                // printf("IN!! taskset\n");
                int id = 7;
                while(id<len && buffer[id] == ' ')id++;
                if(id >= len)return;
                if(buffer[id] == '-'){
                    id+=2;
                    while(id<len && buffer[id] == ' ')id++;
                    if(id >= len)return;
                    int mask = 0;
                    int pid  = 0;
                    id = id + 2;
                    while(id<len && buffer[id]>='0'&&buffer[id]<='9'){
                        mask=mask*10+buffer[id]-'0';
                        id++;
                    }            
                    while(id<len && buffer[id] == ' ')id++;
                    if(id >= len)return;     
                    while(id<len && buffer[id]>='0'&&buffer[id]<='9'){
                        pid=pid*10+buffer[id]-'0';
                        id++;
                    }
                    taskset_p(mask,pid);
                }
                else{
                    int mask = 0;
                    id = id + 2;
                    while(id<len && buffer[id]>='0'&&buffer[id]<='9'){
                        mask=mask*10+buffer[id]-'0';
                        id++;
                    }
                    // printf("mask %d",mask);
                    while(id<len && buffer[id] == ' ')id++;
                    if(id >= len)return;
                    int pre = id;
                    while((buffer[id]>='a'&&buffer[id]<='z') || (buffer[id]>='A'&&buffer[id]<='Z') || (id!=5 && buffer[id] > 32 && buffer[id]<127)){
                        name[id - pre]=buffer[id];
                        id++;
                    }           
                    name[id - pre] = '\0';
                    argv[0]=name;       
                    taskset(mask,name,1,argv);  
                    return;
                }               
                return;
            }
            char now[5];
            for(int i=0;i<4;i++)now[i]=buffer[i];
            now[4]='\0';
            if(strcmp(now, "kill") == 0){
                int id=5;
                int x=0;
                while(id<len && buffer[id] == ' ')id++;
                if(buffer[id]<'0'||buffer[id]>'9')return;
                while(id<len && buffer[id]>='0'&&buffer[id]<='9'){
                    x=x*10+buffer[id]-'0';
                    id++;
                }
                kill(x);
            }
            else if(strcmp(now, "exec") == 0){
                    int id=5;
                    while((buffer[id]>='a'&&buffer[id]<='z') || (buffer[id]>='A'&&buffer[id]<='Z') || (id!=5 && buffer[id] > 32 && buffer[id]<127)){
                        if((id-5)==32)  
                            return;
                        name[id-5]=buffer[id];
                        id++;
                    }
                    name[id-5]='\0'; 
                    while(id<len && buffer[id] == ' ')id++;
                    if(id==len){
                        argv[0]=name;
                        exec(name,1,argv);
                    }
                    else{
                        int pos=1;
                        argv[0]=name;
                        while(1){
                            int loc=0;
                            while(id<len&&buffer[id]>32&&buffer[id]<127){
                                arg[pos][loc++]=buffer[id];
                                id++;
                            }                         
                            arg[pos][loc]='\0';
                            argv[pos]=arg[pos];
//                            printf("%d %d %d :: %s %s\n",pos,id,argv,argv[pos],name);
                            while(id<len && buffer[id]==' ')id++;
                            if(id==len){
                                exec(name,pos+1,argv);
                                return;
                            }
                            else{
                                pos++;
                            }
                        }
                    }
                }
        }
}
int getchar(){
    int x = sys_getchar();
    while(x==-1)
        x=sys_getchar();
    return x;
}
int main(void)
{
    current_dir[0]=0;
    init_display();
    while (1)
    {
        // TODO [P3-task1]: call syscall to read UART port
        printf("> root@UCAS_OS %s: ",current_dir);
        sys_reflush();
        int i=0;
        char c;
        while(i < SHELL_WIDTH){
            c = getchar();
            if(c == 8 || c == 127){//backspace/del
                if(i > 0){
                    i--;
                    buffer[i]='\0';
                    sys_move_cursor(0,-1);
                    printf("> root@UCAS_OS %s: %s                               ",current_dir,buffer);
                    sys_reflush();                    
                }   
            }
            else{
                if(c == '\r'){
                    printf("\n");
                    // printf("> root@UCAS_OS: ");
                    sys_reflush();
                    break;
                }
                buffer[i++] = c;
                buffer[i]='\0';
                sys_move_cursor(0,-1);
                printf("> root@UCAS_OS %s: %s                               ",current_dir,buffer);
                sys_reflush();
            }
        }
        buffer[i]='\0';
        // TODO [P3-task1]: parse input
        // note: backspace maybe 8('\b') or 127(delete)

        // TODO [P3-task1]: ps, exec, kill, clear   

        // TODO [P6-task1]: mkfs, statfs, cd, mkdir, rmdir, ls

        // TODO [P6-task2]: touch, cat, ln, ls -l, rm

        Calc(i);

        // printf("SHELL OUT!\n");
        // sys_reflush();

    }

    return 0;
}
