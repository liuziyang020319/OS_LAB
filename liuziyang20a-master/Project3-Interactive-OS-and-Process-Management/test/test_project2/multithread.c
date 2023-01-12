#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
long a[1000];
long multi_sum=0;
long multi_finished=0;
static void multi_test(){
    int print_location = 6;
    for(int i=1;i<1000;i+=2){
        multi_sum += a[i];
        sys_move_cursor(0, print_location);
        printf("> [TASK] This is a multithread2 ! (%d/1000)\n",i);
    }
    while(1){
        sys_move_cursor(0, print_location);
        printf("> [TASK] multithread2 finished, sum = %d!            \n",multi_sum);
        multi_finished = 1;
    }
} 
int main(){
    for(int i=1;i<=1000;i++){
        a[i] = i;
    }
    sys_thread_create(&multi_test);
    int sum = 0;
    int print_location = 7;
    for(int i=2;i<=1000;i+=2){
        sum+=a[i];
        sys_move_cursor(0, print_location);
        printf("> [TASK] This is a multithread1 ! (%d/1000)\n",i);
    }
    while(multi_finished!=1);
    int ans = sum + multi_sum;
    while(1){
        sys_move_cursor(0, print_location);
        printf("> [TASK] multithread1 finished, ans = %d!           \n",ans);        
    }
    return 0;
}