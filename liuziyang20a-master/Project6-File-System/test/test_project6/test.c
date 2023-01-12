#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char buff[64];
static uint32_t write_buffer[1000];
int main(void)
{
    int fd = sys_fopen("1.txt", O_RDWR);

    // write 'hello world!' * 10
    
    printf("test in !\n");

    for (int i = 0; i < 10000; i+=1000)
    {
        for(int j=0;j<1000;++j)write_buffer[j]=i+j;
        sys_fwrite(fd, write_buffer, sizeof(uint32_t) * 1000);
    }

    // read
    uint32_t b;
    for (int i = 0; i < 4; i++)
    {
        sys_fread(fd, &b, sizeof(uint32_t));
        printf("%d\n",b);
    }

// 1, 32k
    sys_lseek(fd,32768,SEEK_SET);
    for (int i = 8192; i < 8196; i++)
    {
        sys_fread(fd, &b, sizeof(uint32_t));
        printf("%d\n",b);
    }
    sys_fclose(fd);

    return 0;
}