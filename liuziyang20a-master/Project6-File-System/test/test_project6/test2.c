#include <stdio.h>
#include <string.h>
#include <unistd.h>
uint32_t * write_buffer = 0x10000000;
int main(void)
{
    sys_touch("1.txt");
    int fd = sys_fopen("1.txt", O_RDWR);

    // write 'hello world!' * 10
    
    printf("test in !\n");

    // read

    uint32_t b;

// 1, 32k
    sys_lseek(fd,32768,SEEK_SET);
    
    for (int i = 0; i < 10; i++)
    {
        sys_fwrite(fd, &i, sizeof(uint32_t));
    }    
    
    for (int i = 8192; i < 8196; i++)
    {
        sys_fread(fd, &b, sizeof(uint32_t));
        printf("%d\n",b);
    }

// 2, 32k+4*2mb
    sys_lseek(fd,4194304*2 + 32768,SEEK_SET);

    for (int i =10 + 0; i <10 + 10; i++)
    {
        sys_fwrite(fd, &i, sizeof(uint32_t));
    } 

    for (int i = 1048576*2 + 8192; i < 1048576*2 + 8192 + 4; i++)
    {
        sys_fread(fd, &b, sizeof(uint32_t));
        printf("%d\n",b);        
    }

    sys_fclose(fd);

    return 0;
}