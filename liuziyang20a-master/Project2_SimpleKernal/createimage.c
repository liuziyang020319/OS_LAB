#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>

#define IMAGE_FILE "./image"
#define ARGS "[--extended] [--vm] <bootblock> <executable-file> ..."

#define SECTOR_SIZE 512
#define BOOT_LOADER_SIG_OFFSET 0x1fe
#define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2)
#define BOOT_LOADER_SIG_1 0x55
#define BOOT_LOADER_SIG_2 0xaa

#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))


#define FIRST_HALF 255
#define SECOND_HALF 65280

/* TODO: [p1-task4] design your own task_info_t */
typedef struct {
    char name[32];//ANSI
    long TASK_ID;//MAYBE NOT NECESSARY
    long offset;
    long SIZE;
} task_info_t;

#define TASK_MAXNUM 16
static task_info_t taskinfo[TASK_MAXNUM];

/* structure to store command line options */
static struct {
    int vm;
    int extended;
} options;

/* prototypes of local functions */
static void create_image(int nfiles, char *files[]);
static void error(char *fmt, ...);
static void read_ehdr(Elf64_Ehdr *ehdr, FILE *fp);
static void read_phdr(Elf64_Phdr *phdr, FILE *fp, int ph, Elf64_Ehdr ehdr);
static uint64_t get_entrypoint(Elf64_Ehdr ehdr);
static uint32_t get_filesz(Elf64_Phdr phdr);
static uint32_t get_memsz(Elf64_Phdr phdr);
static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr);
static void write_padding(FILE *img, int *phyaddr, int new_phyaddr);
static void write_img_info(int nbytes_kernel, task_info_t *taskinfo,
                           short tasknum, FILE *img,int *phyaddr);

int main(int argc, char **argv)
{
    char *progname = argv[0];

    /* process command line options */
    options.vm = 0;
    options.extended = 0;
    while ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == '-')) {
        char *option = &argv[1][2];

        if (strcmp(option, "vm") == 0) {
            options.vm = 1;
        } else if (strcmp(option, "extended") == 0) {
            options.extended = 1;
        } else {
            error("%s: invalid option\nusage: %s %s\n", progname,
                  progname, ARGS);
        }
        argc--;
        argv++;
    }
    if (options.vm == 1) {
        error("%s: option --vm not implemented\n", progname);
    }
    if (argc < 3) {
        /* at least 3 args (createimage bootblock main) */
        error("usage: %s %s\n", progname, ARGS);
    }
    create_image(argc - 1, argv + 1);
    return 0;
}

/* TODO: [p1-task4] assign your task_info_t somewhere in 'create_image' */
static void create_image(int nfiles, char *files[])
{
    int tasknum = nfiles - 2;
    int nbytes_kernel = 0;
    int phyaddr = 0;
    FILE *fp = NULL, *img = NULL;
    Elf64_Ehdr ehdr;
    Elf64_Phdr phdr;

    /* open the image file */
    img = fopen(IMAGE_FILE, "w");
    assert(img != NULL);

    printf("path of img = %p\n",img);
    long batflag=0;
    for (int fidx = 0; fidx < nfiles; ++fidx) {
        int taskidx = fidx - 2 + batflag;
        if(strcmp(*files,"--bat")==0){
            printf("bat file finded %s\n",*files);
            batflag=-1;
            tasknum--;
            files++;
            continue;
        }
        if(batflag == -1){
            fp = fopen(*files,"r");
            printf("bat file name = %s\n",*files);
            assert(fp!=NULL);
            int len = strlen(*files);
            for(int i=0;i<len;++i){
                taskinfo[taskidx].name[i]=(*files)[i];
            }
            taskinfo[taskidx].name[len]='\n';
            taskinfo[taskidx].TASK_ID=-1;
            taskinfo[taskidx].offset=phyaddr;            
            char ch;
            while(1){
                ch = fgetc(fp);
                if(ch==EOF)break;
                fputc(ch, img);
                phyaddr++;                
            }
            taskinfo[taskidx].SIZE=phyaddr - taskinfo[taskidx].offset;
            fclose(fp);
            files++;
            continue;
        }
        //INITIAL
        if(taskidx >= 0){
            taskinfo[taskidx].name[0]='\n';
            taskinfo[taskidx].offset=0;
            taskinfo[taskidx].SIZE=0;
            taskinfo[taskidx].TASK_ID=0;
        }
        
        /* open input file */
        fp = fopen(*files, "r");
        assert(fp != NULL);

        /* read ELF header */
        read_ehdr(&ehdr, fp);
        printf("0x%04lx: %s\n", ehdr.e_entry, *files);
        /* Entry point virtual address */

        if(taskidx >= 0){
            int len = strlen(*files);
            for(int i=0;i<len;++i){
                taskinfo[taskidx].name[i]=(*files)[i];
            }
            
            taskinfo[taskidx].name[len]='\n';
            taskinfo[taskidx].TASK_ID=ehdr.e_entry;
            taskinfo[taskidx].offset=phyaddr;
        }

        /* for each program header */
        for (int ph = 0; ph < ehdr.e_phnum; ph++) {/* Program header table entry count */

            /* read program header */
            read_phdr(&phdr, fp, ph, ehdr);

            /* write segment to the image */
            write_segment(phdr, fp, img, &phyaddr);

            /* update nbytes_kernel */
            if (strcmp(*files, "main") == 0) {
                nbytes_kernel += get_filesz(phdr);
            }
        }
        if(taskidx >= 0){
            taskinfo[taskidx].SIZE=phyaddr-taskinfo[taskidx].offset;
        }
        /* write padding bytes */
        /**
         * TODO:
         * 1. [p1-task3] do padding so that the kernel and every app program
         *  occupies the same number of sectors
         * 2. [p1-task4] only padding bootblock is allowed!
         */       
        printf("PHY_ADDR = %d\n",phyaddr);
        
        if (strcmp(*files, "bootblock") == 0) {
            write_padding(img, &phyaddr, SECTOR_SIZE);
        }

        fclose(fp);
        files++;
    }
    
    
    write_img_info(nbytes_kernel, taskinfo, tasknum, img, &phyaddr);

    fclose(img);
}

static void read_ehdr(Elf64_Ehdr * ehdr, FILE * fp)
{
    int ret;

    ret = fread(ehdr, sizeof(*ehdr), 1, fp);
    assert(ret == 1);
    assert(ehdr->e_ident[EI_MAG1] == 'E');
    assert(ehdr->e_ident[EI_MAG2] == 'L');
    assert(ehdr->e_ident[EI_MAG3] == 'F');
}

static void read_phdr(Elf64_Phdr * phdr, FILE * fp, int ph,
                      Elf64_Ehdr ehdr)
{
    int ret;

    fseek(fp, ehdr.e_phoff + ph * ehdr.e_phentsize, SEEK_SET);
    ret = fread(phdr, sizeof(*phdr), 1, fp);
    assert(ret == 1);
    if (options.extended == 1) {
        printf("\tsegment %d\n", ph);
        printf("\t\toffset 0x%04lx", phdr->p_offset);
        printf("\t\tvaddr 0x%04lx\n", phdr->p_vaddr);
        printf("\t\tfilesz 0x%04lx", phdr->p_filesz);
        printf("\t\tmemsz 0x%04lx\n", phdr->p_memsz);
    }
}

static uint64_t get_entrypoint(Elf64_Ehdr ehdr)
{
    return ehdr.e_entry;
}

static uint32_t get_filesz(Elf64_Phdr phdr)
{
    return phdr.p_filesz;
}

static uint32_t get_memsz(Elf64_Phdr phdr)
{
    return phdr.p_memsz;
}

static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr)
{
    if (phdr.p_memsz != 0 && phdr.p_type == PT_LOAD) {
        /* write the segment itself */
        /* NOTE: expansion of .bss should be done by kernel or runtime env! */
        if (options.extended == 1) {
            printf("\t\twriting 0x%04lx bytes\n", phdr.p_filesz);
        }
        fseek(fp, phdr.p_offset, SEEK_SET);
        while (phdr.p_filesz-- > 0) {
            fputc(fgetc(fp), img);
            (*phyaddr)++;
        }
    }
}

static void write_padding(FILE *img, int *phyaddr, int new_phyaddr)
{
    if (options.extended == 1 && *phyaddr < new_phyaddr) {
        printf("\t\twrite 0x%04lx bytes for padding\n", new_phyaddr - *phyaddr);
    }

    while (*phyaddr < new_phyaddr) {
        fputc(0, img);
        (*phyaddr)++;
    }
}

int GET_SEC(int x){
    return x/512;
}

static void write_img_info(int nbytes_kern, task_info_t *taskinfo,
                           short tasknum, FILE * img,int *phyaddr)
{
    // 0x502001fc
    // TODO: [p1-task3] & [p1-task4] write image info to some certain places
    // NOTE: os size, infomation about app-info sector(s) ...
    //TASK 3
    int APP_ADDR = *phyaddr;
    printf("APP INFO START ADDR = %hx\n",APP_ADDR);
    
    int PRE_ADDR = APP_ADDR + 8 + 56 * tasknum;
    long NUM_SECTOR = GET_SEC(PRE_ADDR) - GET_SEC(APP_ADDR) + 1;  
    long SECTOR_ID  = 0;
    SECTOR_ID = GET_SEC(APP_ADDR); 

    long SECTOR_offset = 0;
    SECTOR_offset = APP_ADDR - GET_SEC(APP_ADDR)*512;

    long NUM_TASK = tasknum;
    fwrite(&NUM_TASK,sizeof(unsigned long),1,img);
    (*phyaddr)+=8;
    
    printf("NUM_SECTOR = %d SECTOR_ID = %d SECTOR_offset = %d PRE_ADDR = %hx\n",NUM_SECTOR,SECTOR_ID,SECTOR_offset,PRE_ADDR);

    for(int task_id = 0;task_id < tasknum; ++task_id){
        printf("TASK %d\n",task_id);
        printf("PHYADDR = %hx(HX) %d\n",(*phyaddr),(*phyaddr));
        for(int i=0;i<32;++i){
            char ch = taskinfo[task_id].name[i];
            fwrite(&ch,sizeof(unsigned char),1,img);
            (*phyaddr)+=1;
        }        
        printf("task name = %s\n",taskinfo[task_id].name);
        fwrite(&(taskinfo[task_id].TASK_ID),sizeof(unsigned long),1,img);
        (*phyaddr)+=8;
        fwrite(&(taskinfo[task_id].offset) ,sizeof(unsigned long),1,img);
        (*phyaddr)+=8;        
        fwrite(&(taskinfo[task_id].SIZE  ) ,sizeof(unsigned long),1,img);
        (*phyaddr)+=8;

        /*
        unsigned char task_id_fir = (taskinfo[task_id].TASK_ID)  & FIRST_HALF ;
        unsigned char task_id_sec = (1LL*((taskinfo[task_id].TASK_ID) & SECOND_HALF)>>8);
        unsigned char offset_fir  = (taskinfo[task_id].offset)   & FIRST_HALF ;
        unsigned char offset_sec  = (1LL*((taskinfo[task_id].offset)  & SECOND_HALF)>>8);
        unsigned char size_fir    = taskinfo[task_id].SIZE     & FIRST_HALF ;
        unsigned char size_sec    = (1LL*((taskinfo[task_id].SIZE)    & SECOND_HALF)>>8);
        
        fputc(task_id_fir,img);
        (*phyaddr)++;
        fputc(task_id_sec,img);
        (*phyaddr)++;
        fputc(offset_fir,img);
        (*phyaddr)++;
        fputc(offset_sec,img);s
        (*phyaddr)++;
        fputc(size_fir,img);
        (*phyaddr)++;
        fputc(size_sec,img);
        (*phyaddr)++;
        */
        printf("task_id = %d HEX = %hx\n",taskinfo[task_id].TASK_ID,taskinfo[task_id].TASK_ID);
        printf("offset  = %d HEX = %hx\n",taskinfo[task_id].offset ,taskinfo[task_id].offset);
        printf("size    = %d HEX = %hx\n",taskinfo[task_id].SIZE   ,taskinfo[task_id].SIZE  );
        printf("================================================\n");
    }

    printf("APP INFO END ADDR = %hx(HX) %d\n",(*phyaddr),(*phyaddr));

    unsigned short SECTOR_NUMS[1] = {(nbytes_kern/512+1)};
    printf("TASK_NUMS = %d SECTOR_NUMS = %d KERNAL_SIZE = %d IMG_ID = 0x%04lx\n",tasknum,SECTOR_NUMS[0],nbytes_kern,img);
    fseek(img,0x000001fc,SEEK_SET);
    fwrite(SECTOR_NUMS,sizeof(unsigned short),1,img);
    fseek(img,0x000001f0,SEEK_SET);
    fwrite(&SECTOR_offset,  sizeof(unsigned long ),1,img);
    
  
    fseek(img,0x000001e8,SEEK_SET);
    fwrite(&NUM_SECTOR,sizeof(unsigned long),1,img);

    fseek(img,0x000001e0,SEEK_SET);
    fwrite(&SECTOR_ID,sizeof(unsigned long),1,img);
}

/* print an error message and exit */
static void error(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    if (errno != 0) {
        perror(NULL);
    }
    exit(EXIT_FAILURE);
}
