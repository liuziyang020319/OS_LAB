#include <stdio.h>
#include <stdint.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#define BASE1 0x10800000
#define PAGESIZE 0x1000

typedef struct Node{
    int a,b,c;
}Node;
typedef Node * Nodeptr;
Nodeptr original;
Nodeptr shot[11];
int main(int argc, char* argv[]){
    char *prog_name = argv[0];
    int print_location = 1;
    if (argc > 1) {
        print_location = atol(argv[1]);
    }
    sys_move_cursor(0, print_location);
    original = (Nodeptr)BASE1;
    
    for(int i=0; i<=10; i++){
        shot[i] = (Nodeptr)(BASE1 + (i+1) * PAGESIZE);
    }
    (*original).a = 1;
    (*original).b = 2;
    (*original).c = 3;
    for(int i=0;i<=10;i++){
        if(i%3==0 && i){
            (*original).a = 1 * (i+2);
            (*original).b = 2 * (i+2);
            (*original).c = 3 * (i+2);                
        }
        sys_snapshot(original,shot[i]);
    }
    for(int i=0; i<=10; i++){
        printf("shot%d : a=%d b=%d c=%d va=%lx pa=%lx\n",i,(*shot[i]).a,(*shot[i]).b,(*shot[i]).c,shot[i],sys_getva2pa(shot[i]));
    }
    printf("original : a=%d b=%d c=%d va=%lx pa=%lx\n",(*original).a,(*original).b,(*original).c,original,sys_getva2pa(original));
    return 0;
}