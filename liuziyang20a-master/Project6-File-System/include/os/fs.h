#ifndef __INCLUDE_OS_FS_H__
#define __INCLUDE_OS_FS_H__

#include <type.h>

/* macros of file system */
#define SUPERBLOCK_MAGIC 0x20221205
#define NUM_FDESCS 16
#define BLOCK_SIZ 4096lu      
#define DATA_SIZ 0x40000000//1G 

/* data structures of file system */
typedef struct superblock_t{
    // TODO [P6-task1]: Implement the data structure of superblock
    uint32_t magic;
    uint32_t siz;
    uint32_t start;

    uint32_t blockbitmap_offset;
    uint32_t blockbitmap_siz;

    uint32_t inodebitmap_offset;
    uint32_t inodebitmap_siz;

    uint32_t inodetable_offset;
    uint32_t inodetable_siz;

    uint32_t datablock_offset;
    uint32_t datablock_siz;

} superblock_t;

//32B
typedef struct dentry_t{
    // TODO [P6-task1]: Implement the data structure of directory entry
    uint32_t ino;
    char name[28];
} dentry_t;

#define num_direct 8
#define num_indirect1 3
#define num_indirect2 2
#define num_indirect3 1

//128B
typedef struct inode_t{ 
    // TODO [P6-task1]: Implement the data structure of inode
    uint8_t type;
    uint8_t mode;//1
    
    uint32_t ino;//2
    uint32_t direct[num_direct];
    uint32_t indirect_1[num_indirect1];
    uint32_t indirect_2[num_indirect2];
    uint32_t indirect_3;
    uint32_t links;
    uint32_t siz;//sum of byte
    uint32_t mtime;

    uint64_t __aligned0;
    uint64_t __aligned1;
    uint64_t __aligned2;
    uint64_t __aligned3;
    uint64_t __aligned4;
    uint64_t __aligned5;
    // uint64_t __aligned6;
    // uint64_t __aligned7; 
} inode_t;

#define INODE_DIR 0
#define INODE_FILE 1

#define P_RDONLY 1
#define P_WRONLY 2
#define P_RDWR   3

typedef struct fdesc_t{
    // TODO [P6-task2]: Implement the data structure of file descriptor
    uint32_t used;
    uint32_t ino;
    uint32_t r_cursor;
    uint32_t w_cursor;
} fdesc_t;

/* modes of do_fopen */
#define O_RDONLY 1  /* read only open */
#define O_WRONLY 2  /* write only open */
#define O_RDWR   3  /* read/write open */

/* whence of do_lseek */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* fs function declarations */
extern int do_mkfs(void);
extern int do_statfs(void);
extern int do_cd(char *path);
extern int do_mkdir(char *path);
extern int do_rmdir(char *path);
extern int do_ls(char *path, int option);
extern int do_touch(char *path);
extern int do_cat(char *path);
extern int do_fopen(char *path, int mode);
extern int do_fread(int fd, char *buff, int length);
extern int do_fwrite(int fd, char *buff, int length);
extern int do_fclose(int fd);
extern int do_ln(char *src_path, char *dst_path);
extern int do_rm(char *path);
extern int do_lseek(int fd, int offset, int whence);

#endif