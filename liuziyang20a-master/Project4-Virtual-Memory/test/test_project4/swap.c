#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#define NUM_ADDRESS 128

#define BUFFER_LENGTH (4*(MAX_MBOX_LENGTH))         

#define BASE1 0x10800000
#define BASE2 0x80200000

int main(int argc, char* argv[]){
    char *prog_name = argv[0];
    int print_location = 1;
    if (argc > 1) {
        print_location = atol(argv[1]);
    }
    sys_move_cursor(0, print_location);

    uint64_t* mem_table[NUM_ADDRESS];
    int i;
    for(i = 0; i < NUM_ADDRESS; i++){
        mem_table[i] = BASE1 + i * 0x1000;
        *mem_table[i] = (uint64_t)mem_table[i];
    }
    int success = 1;

    for(i = 0; i < NUM_ADDRESS; i++){
        if(*mem_table[i] == (uint64_t)mem_table[i]){
            if(i % 8 == 0)
                printf("[%d]address:%u match success!\n", i, mem_table[i]);
        }
        else{
            printf("[%d]address:%u match failed!\n", i, mem_table[i]);
            break;
        }
        if(i % 32 == 0 && i)sys_move_cursor(0, print_location);
    }

    // if(success)
    //     printf("BASE1 %d passed\n",BASE1);
    // else{
    //     return 0;
    // }
    sys_move_cursor(0, print_location);
    for(i = 0; i < NUM_ADDRESS; i++){
        mem_table[i] = BASE2 + i * 0x1000;
        *mem_table[i] = (uint64_t)mem_table[i];
    }
    for(i = 0; i < NUM_ADDRESS; i++){
        if(*mem_table[i] == (uint64_t)mem_table[i]){
            if(i % 8 == 0)
                printf("[%d]address:%u match success!\n", i, mem_table[i]);
        }
        else{
            printf("[%d]address:%u match failed!\n", i, mem_table[i]);
            break;
        }
        if(i % 32 == 0 && i)sys_move_cursor(0, print_location);
    }
    sys_move_cursor(0, print_location);
    for(i = 0; i < NUM_ADDRESS; i++){
        mem_table[i] = BASE1 + i * 0x1000;
    }
    for(i = 0; i < NUM_ADDRESS; i++){
        if(*mem_table[i] == (uint64_t)mem_table[i]){
            if(i % 8 == 0)
                printf("[%d]address:%u match success!\n", i, mem_table[i]);
        }
        else{
            printf("[%d]address:%u match failed!\n", i, mem_table[i]);
            break;
        }
        if(i % 32 == 0 && i)sys_move_cursor(0, print_location);
    }  
    sys_move_cursor(0, print_location); 
    for(i = 0; i < NUM_ADDRESS; i++){
        mem_table[i] = BASE2 + i * 0x1000;
    }
    for(i = 0; i < NUM_ADDRESS; i++){
        if(*mem_table[i] == (uint64_t)mem_table[i]){
            if(i % 8 == 0)
                printf("[%d]address:%u match success!\n", i, mem_table[i]);
        }
        else{
            printf("[%d]address:%u match failed!\n", i, mem_table[i]);
            break;
        }
        if(i % 32 == 0 && i)sys_move_cursor(0, print_location);
    }
    sys_move_cursor(0, print_location);
    for(i = 0; i < NUM_ADDRESS; i++){
        mem_table[i] = BASE1 + i * 0x1000;
    }
    for(i = 0; i < NUM_ADDRESS; i++){
        if(*mem_table[i] == (uint64_t)mem_table[i]){
            if(i % 8 == 0)
                printf("[%d]address:%u match success!\n", i, mem_table[i]);
        }
        else{
            printf("[%d]address:%u match failed!\n", i, mem_table[i]);
            break;
        }
        if(i % 32 == 0 && i)sys_move_cursor(0, print_location);
    } 
    return 0;
}
