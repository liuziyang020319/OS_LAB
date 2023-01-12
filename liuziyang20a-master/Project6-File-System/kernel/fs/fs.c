#include <os/string.h>
#include <os/fs.h>
#include <os/mm.h>
#include <os/time.h>
#include <os/kernel.h>
#include <pgtable.h>
#include <assert.h>

static superblock_t superblock;
static fdesc_t fdesc_array[NUM_FDESCS];

uint8_t empty[512] = {0};
uint64_t current_dir = 0;

void init_fdesc_array(){
    for(int i=0; i<NUM_FDESCS; i++)
        fdesc_array[i].used = 0;
}

int strcheck(char *src, char c)
{
	while(*src){
		if(*src == c)
			return 1;
		src++;
	}
	return 0;
}


int check_fs(){
	uint8_t sector[512];
	bios_sdread(kva2pa((uintptr_t)sector), 1, padding_ADDR/512);
	superblock_t *superblock = (superblock_t *)sector;
	return superblock->magic == SUPERBLOCK_MAGIC;    
}

uint32_t countbit(uint64_t x){
    uint32_t cnt = 0;
    while(x){
        if(x&1)cnt++;
        x/=2;
    }
    return cnt;
}

//give ino return inode_t
inode_t * ino2inode_t(uint8_t * base, uint32_t ino){
	uint8_t sector[512];
	bios_sdread(kva2pa((uintptr_t)sector), 1, padding_ADDR/512);
	superblock_t *superblock = (superblock_t *)sector;
    uint32_t sum = (512/sizeof(inode_t));//inode per block
    bios_sdread(kva2pa((uintptr_t)base  ), 1, superblock->start + superblock->inodetable_offset + ino/sum);     
    return (inode_t *)(base + (ino*sizeof(inode_t)) % 512);
}

//write back inode
void write_inode(uint8_t * base, uint32_t ino){
	uint8_t sector[512];
	bios_sdread(kva2pa((uintptr_t)sector), 1, padding_ADDR/512);
	superblock_t *superblock = (superblock_t *)sector;

    uint32_t sum = (512/sizeof(inode_t));//inode per block
    bios_sdwrite(kva2pa((uintptr_t)base  ), 1, superblock->start + superblock->inodetable_offset + ino/sum);     

}

//check whether the dir is illegal
int check_dir(uint32_t ino){
	uint8_t sector[512];
	inode_t * inode = ino2inode_t(sector, ino);
	if(inode->type != INODE_DIR){
		printk("> [FS] Cannot cd to a file\n");
		return 0;
	}
	return 1;
}

//get block sector (each block contains 4 sectors)

void get_data_block2sector(uint8_t * base, uint32_t block_id, uint32_t offset){
	uint8_t sector[512];
	bios_sdread(kva2pa((uintptr_t)sector), 1, padding_ADDR/512);
	superblock_t *superblock = (superblock_t *)sector;

    bios_sdread(kva2pa((uintptr_t)base), 1, superblock->start + superblock->datablock_offset + (BLOCK_SIZ/512)*block_id + offset/512);
}

void store_data_block2sector(uint8_t * base, uint32_t block_id, uint32_t offset){
	uint8_t sector[512];
	bios_sdread(kva2pa((uintptr_t)sector), 1, padding_ADDR/512);
	superblock_t *superblock = (superblock_t *)sector;

    bios_sdwrite(kva2pa((uintptr_t)base), 1, superblock->start + superblock->datablock_offset + (BLOCK_SIZ/512)*block_id + offset/512);    
}

int ino_DFS(uint32_t base_ino, uint32_t * ino, char * name, uint32_t * dentry_idx){//1 S 0 F
    int len=strlen(name);
    if(!len){
        if(ino!=NULL)*ino = base_ino;
        return 1;
    }
    if(name[0] == '/'){
        base_ino = 0;
        name++;
        len--;
    }
	uint8_t sector[512];
	bios_sdread(kva2pa((uintptr_t)sector), 1, padding_ADDR/512);
	superblock_t *superblock = (superblock_t *)sector;

    int nxt = 0;
    for(;nxt<len;nxt++){
        if(name[nxt] == '/'){
            name[nxt] = 0;
            nxt ++;
            break;
        }
    }

    uint8_t sector2[512];
    uint8_t sector3[512];

    inode_t * base_t = ino2inode_t(sector2,base_ino);
    if(base_t->type != INODE_DIR){
        printk("> [FS] Can not open file %s\n",name);
        return 0;
    } 

    dentry_t * dentry = (dentry_t *)sector3;
    for(int i=0; i <base_t->siz; i++, dentry++ ){
        if(i%(512/sizeof(dentry_t)) == 0){
            dentry = (dentry_t *)sector3;
            uint32_t sum = (BLOCK_SIZ/sizeof(dentry_t));//dentry per block
            bios_sdread(kva2pa(sector3), 1, 
            superblock->start + superblock->datablock_offset +
            (BLOCK_SIZ/512) * base_t->direct[i / sum] +
            ((i*sizeof(dentry_t)) % BLOCK_SIZ) / 512
            );
        }
        // printk("%s %s %d %d\n",dentry->name,name,base_ino,base_t->siz);
        if(strcmp(dentry->name,name) == 0){
            if(ino!=NULL)
                *ino = dentry->ino;
            if(dentry_idx!=NULL)
                *dentry_idx = i;
            // if(nxt!=len){
            //     printk("ino %d %s\n",dentry->ino,name+nxt);
            // }
            return ino_DFS(dentry->ino, ino, name + nxt, NULL);
        }
    }
    return 0;
}

int getfa(char ** path_t, int32_t * pa_ino){
    int len = strlen(*path_t);
    if(len==0){
        printk("> [FS] Illegal name!\n");
        return 0;
    }
    if(pa_ino!=NULL)
        *pa_ino = current_dir;
    uint8_t name_copy[64];
    memcpy(name_copy, (uint8_t *)*path_t,len+1);
    int idx = len-1;
    int32_t dirino = current_dir;
    for(;idx >= 1; idx--){
        if(name_copy[idx] == '/'){
            name_copy[idx] = 0;
            if(name_copy[idx-1] == '/' || !ino_DFS(current_dir, &dirino, name_copy, NULL)){
                printk("> [FS] No such directory!\n");
                return 0;
            }
            idx++;
            break;
        }
    }    
    if(!check_dir(dirino)){
        return 0;
    }
    if(pa_ino!=NULL){
        *pa_ino = dirino;
    }
    *path_t += idx;
    return 1;
}

int get_first_zero_index(uint8_t x){
    if(x == 255){
        printk("> [FS] no zero!\n");
        assert(0);
        return -1;
    }
    for(int i = 0; i < 8; i++){
        if((x%2) == 0)return i;
        x/=2;
    }
}

uint32_t alloc_block(){
	uint8_t sector[512];
    uint8_t sector2[512];
	bios_sdread(kva2pa((uintptr_t)sector), 1, padding_ADDR/512);
	superblock_t *superblock = (superblock_t *)sector;
    
    for(int i = 0; i < superblock->blockbitmap_siz; i++){
        bios_sdread(kva2pa((uintptr_t)sector2), 1, superblock->start + superblock->blockbitmap_offset + i);
        for(int j = 0; j < 512; j++){
            if(sector2[j] == 255u)continue;//FULL
            int id = get_first_zero_index(sector2[j]);
            sector2[j] |= (uint8_t)(0x1<<id);
            bios_sdwrite(kva2pa((uintptr_t)sector2), 1,superblock->start + superblock->blockbitmap_offset + i);
            return ((i*512*8) + (j*8) + id);
        }
    }
    printk("> [FS] Disk is FULL!\n");
    return 0;    
}

//free but not clean
void free_block(uint32_t block_id){
	uint8_t sector[512];
    uint8_t sector2[512];
	bios_sdread(kva2pa((uintptr_t)sector), 1, padding_ADDR/512);
	superblock_t *superblock = (superblock_t *)sector;

    bios_sdread(kva2pa((uintptr_t)sector2), 1, superblock->start + superblock->blockbitmap_offset + block_id/(512*8));

    uint32_t sector_id = (block_id % (512*8))/8;
    uint8_t mask = (0x1 << (block_id % 8));
    sector2[sector_id] ^= mask;

    bios_sdwrite(kva2pa((uintptr_t)sector2), 1, superblock->start + superblock->blockbitmap_offset + block_id/(512*8));
}


uint32_t alloc_inode(){
	uint8_t sector[512];
    uint8_t sector2[512];
	bios_sdread(kva2pa((uintptr_t)sector), 1, padding_ADDR/512);
	superblock_t *superblock = (superblock_t *)sector;
    
    for(int i = 0; i < superblock->inodebitmap_siz; i++){
        bios_sdread(kva2pa((uintptr_t)sector2), 1, superblock->start + superblock->inodebitmap_offset + i);
        for(int j = 0; j < 512; j++){
            if(sector2[j] == 255u)continue;//FULL
            int id = get_first_zero_index(sector2[j]);
            sector2[j] |= (uint8_t)(0x1<<id);
            bios_sdwrite(kva2pa((uintptr_t)sector2), 1,superblock->start + superblock->inodebitmap_offset + i);
            return ((i*512*8) + (j*8) + id);
        }
    }
    printk("> [FS] File inode is FULL!\n");
    return 0;       
}

void free_inode(uint32_t ino){
	uint8_t sector[512];
    uint8_t sector2[512];
	bios_sdread(kva2pa((uintptr_t)sector), 1, padding_ADDR/512);
	superblock_t *superblock = (superblock_t *)sector;

    bios_sdread(kva2pa((uintptr_t)sector2), 1, superblock->start + superblock->inodebitmap_offset + ino/(512*8));

    uint32_t sector_id = (ino % (512*8))/8;
    uint8_t mask = (0x1 << (ino % 8));
    sector2[sector_id] ^= mask;

    bios_sdwrite(kva2pa((uintptr_t)sector2), 1, superblock->start + superblock->inodebitmap_offset + ino/(512*8));    
}

int do_mkfs(void)
{
    // TODO [P6-task1]: Implement do_mkfs
    init_fdesc_array();

    uint8_t sector[512];
    uint8_t sector2[512];

    superblock_t *superblock = (superblock_t *) sector;

    if(check_fs()){
        printk("> [FS] File system is already running!\n");
        return 0;
    }

    printk("> [FS] Start initialize filesystem!\n");

    superblock->magic = SUPERBLOCK_MAGIC;
    superblock->start = padding_ADDR/512;

    superblock->blockbitmap_offset = 1;
    superblock->blockbitmap_siz = DATA_SIZ/BLOCK_SIZ/BLOCK_SIZ;

    superblock->inodebitmap_offset = superblock->blockbitmap_offset + superblock->blockbitmap_siz;
    superblock->inodebitmap_siz = 1;//512*8

    superblock->inodetable_offset = superblock->inodebitmap_offset + superblock->inodebitmap_siz;
    superblock->inodetable_siz = 8*sizeof(inode_t);//512*8*siz/512

    superblock->datablock_offset = superblock->inodetable_offset + superblock->inodetable_siz;
    superblock->datablock_siz = DATA_SIZ/512;

    superblock->siz = 1 + superblock->blockbitmap_siz + superblock->inodebitmap_siz +
                          superblock->inodetable_siz  + superblock->datablock_siz;

    printk("> [FS] Setting superblock...\n");
    bios_sdwrite(kva2pa((uintptr_t)superblock),1,padding_ADDR/512);
    
    memset(superblock,0,sizeof(superblock));    
    bios_sdread(kva2pa((uintptr_t)superblock),1,padding_ADDR/512);
	
    printk("       magic : 0x%x\n", superblock->magic);
    printk("       num sector : %d, start sector : %d\n", superblock->siz, superblock->start);
	printk("       block bitmap offset : %d (%d)\n", superblock->blockbitmap_offset,superblock->blockbitmap_siz);
	printk("       inode bitmap offset : %d (%d)\n", superblock->inodebitmap_offset,superblock->inodebitmap_siz);
	printk("       inode table offset : %d (%d)\n", superblock->inodetable_offset,superblock->inodetable_siz);
	printk("       data offset : %d (%d)\n", superblock->datablock_offset,superblock->datablock_siz);
	printk("       inode entry size : %dB dir entry size : %dB\n", sizeof(inode_t), sizeof(dentry_t));

    memset(empty,0,512);
    
    printk("> [FS] Setting inode-map...\n");
    for(int i=0; i<superblock->inodebitmap_siz; i++){
        bios_sdwrite(kva2pa((uintptr_t)empty),1,superblock->start + superblock->inodebitmap_offset + i);
    }

	printk("> [FS] Setting block-map...\n");
    for(int i = 0; i<superblock->blockbitmap_siz; i++)
	    bios_sdwrite(kva2pa((uintptr_t)empty),1,superblock->start + superblock->blockbitmap_offset + i);

    printk("> [FS] Setting inode...\n");

    memset(sector2,0,512);
    inode_t *inode = (inode_t *) sector2;
    inode->type = INODE_DIR;
    inode->mode = P_RDWR;
    inode->ino = 0;
    inode->direct[0] = 0;
    inode->siz = 1;
    inode->mtime = get_timer();
    bios_sdwrite(kva2pa((uintptr_t)inode),1,superblock->start + superblock->inodetable_offset);    
    
    //inode bitmap
    memset(sector2,0,512);
    sector2[0] = 1;
    bios_sdwrite(kva2pa((uintptr_t)sector2),1,superblock->start + superblock->inodebitmap_offset);    

    memset(sector2,0,512);
    dentry_t *dentry = (dentry_t *)sector2;
    dentry->ino = 0;
    memcpy((uint8_t *)dentry->name, (uint8_t *)".", 2);
    
    bios_sdwrite(kva2pa((uintptr_t)dentry),1,superblock->start + superblock->datablock_offset);

    memset(sector2,0,512);
    sector2[0] = 1;
    bios_sdwrite(kva2pa((uintptr_t)sector2),1,superblock->start + superblock->blockbitmap_offset);    

    printk("> Initialize filesystem finished!\n");

    return 0;  // do_mkfs succeeds
}



int do_statfs(void)
{
    // TODO [P6-task1]: Implement do_statfs
    if(!check_fs()){
        printk("> [FS] Error: file system is not running!\n");
        assert(0);
        return 0;
    }
    uint8_t sector[512];
    bios_sdread(kva2pa((uintptr_t)sector), 1, padding_ADDR/512);
    superblock_t *superblock = (superblock_t *)sector;

    int used_inode = 0;
    int used_block = 0;
    uint8_t sector2[512];
    
    for(int i=0;i<superblock->inodebitmap_siz;++i){
        bios_sdread(kva2pa((uintptr_t)sector2),1,superblock->start + superblock->inodebitmap_offset + i);
        for(int j=0;j<512;++j){
            used_inode += countbit(sector2[j]);
        }
    }

    for(int i=0;i<superblock->blockbitmap_siz;++i){
        bios_sdread(kva2pa((uintptr_t)sector2),1,superblock->start + superblock->blockbitmap_offset + i);
        for(int j=0;j<512;++j){
            used_block += countbit(sector2[j]);
        }
    }

    printk("       magic : 0x%x\n", superblock->magic);
    printk("       num sector : %d, start sector : %d\n", superblock->siz, superblock->start);
	printk("       block bitmap offset : %d (%d)\n", superblock->blockbitmap_offset,superblock->blockbitmap_siz);
	printk("       inode bitmap offset : %d (%d)\n", superblock->inodebitmap_offset,superblock->inodebitmap_siz);
	printk("       inode table offset : %d (%d/%d)\n", superblock->inodetable_offset,used_inode,superblock->inodetable_siz);
	printk("       data offset : %d (%d/%d)\n", superblock->datablock_offset,used_block,superblock->datablock_siz);
	printk("       inode entry size : %dB dir entry size : %dB\n", sizeof(inode_t), sizeof(dentry_t));

    return 0;  // do_statfs succeeds
}

int do_cd(char *path)
{
    // TODO [P6-task1]: Implement do_cd
    if(!check_fs()){
        printk("> [FS] Error: file system is not running!\n");
        assert(0);
        return 0;
    }
    uint32_t ino;
    if(!ino_DFS(current_dir, &ino, path, NULL)){
        printk("> [FS] no such directory!\n");
        return 0;
    }
    else{
        if(!check_dir(ino)){
            return 0;
        }
        current_dir = ino;
    }
    return 1;  // do_cd succeeds
}

int do_mkdir(char *path)
{
    // TODO [P6-task1]: Implement do_mkdir
    if(!check_fs()){
        printk("> [FS] Error: file system is not running!\n");
        assert(0);
        return 0;
    }
    int32_t dir_ino;
    if(!getfa(&path, &dir_ino)){
        return 0;
    }
    int32_t len = strlen(path);//name length
    if(len == 0 || strcheck(path, '/') || strcheck(path, ' ') || path[0] == '-' || path[0] == '.'){
        printk("> [FS] Illegal name!\n");
        return 0;
    }

    uint32_t name[64];
    memcpy(name,path,len+1);
    if(ino_DFS(dir_ino,NULL,name,NULL)){
        printk("> [FS] name already existed!\n");
        return 0;
    }

    uint8_t sector[512];
    inode_t * pa_inode_t = ino2inode_t(sector, dir_ino);
    if(pa_inode_t->mode == P_RDONLY){
        printk("> [FS] read-only!\n");
        return 0;
    }

    int idx = pa_inode_t->siz;
    pa_inode_t->siz ++;
    pa_inode_t->mtime = get_timer();
    if(pa_inode_t->siz % ((BLOCK_SIZ/sizeof(dentry_t))) == 0){
        pa_inode_t->direct[pa_inode_t->siz / (BLOCK_SIZ/sizeof(dentry_t))] = alloc_block();
    }    

    write_inode(sector, pa_inode_t->ino);


    //mkdir inode 
    uint8_t sector2[512];
    uint32_t new_ino = alloc_inode();
    inode_t * new_t = ino2inode_t(sector2, new_ino);
    
    new_t->type = INODE_DIR;
    new_t->mode = P_RDWR;
    new_t->ino  = new_ino;

    uint32_t dentry = alloc_block();
    new_t->direct[0] = dentry;
    new_t->siz = 2;
    new_t->links = 1;
    new_t->mtime = get_timer();
    write_inode(sector2, new_ino);

    //pa
    uint32_t data_id = pa_inode_t->direct[idx / (BLOCK_SIZ/sizeof(dentry_t))];

    get_data_block2sector(sector2, data_id, (idx % (BLOCK_SIZ/sizeof(dentry_t))) * sizeof(dentry_t));
    dentry_t *pa_dentry = ((dentry_t *)sector2) + (idx % (512/sizeof(dentry_t)));
    pa_dentry->ino = new_ino;
    memcpy(pa_dentry->name, name, len+1);
    store_data_block2sector(sector2, data_id, (idx % (BLOCK_SIZ/sizeof(dentry_t))) * sizeof(dentry_t));

    get_data_block2sector(sector2, dentry, 0);
    
    //.
    dentry_t *dentry_now = (dentry_t *)sector2;
    dentry_now->ino = new_ino;
    memcpy(dentry_now->name, ".", 2);

    //..
    dentry_now++;
    dentry_now->ino = dir_ino;
    memcpy(dentry_now->name, "..", 3);

    store_data_block2sector(sector2, dentry, 0);

    return 1;  // do_mkdir succeeds
}

int do_rmdir(char *path)
{
    // TODO [P6-task1]: Implement do_rmdir
    if(!check_fs()){
        printk("> [FS] Error: file system is not running!\n");
        assert(0);
        return 0;
    }

    int32_t dir_ino;
    if(!getfa(&path, &dir_ino)){
        return 0;
    }

    int32_t len = strlen(path);//name length
    if(len == 0 || strcheck(path, '/') || strcheck(path, ' ') || path[0]== '-' || path[0] == '.'){
        printk("> [FS] Illegal name!\n");
        return 0;
    }

    int32_t my_ino;
    int32_t dentry_idx;
    uint8_t name_copy[64];
    memcpy(name_copy, path, len+1);
    if(!ino_DFS(dir_ino, &my_ino, name_copy, &dentry_idx)){
        printk("> [FS] No such file/directory!\n");
        return 0;
    }

    uint8_t sector[512];
    inode_t * dir_inode_t = ino2inode_t(sector, dir_ino);
    if(dir_inode_t->mode == P_RDONLY){
        printk("> [FS] Read only, denied\n");
        return 0;
    }

    uint32_t oldsiz = dir_inode_t->siz;
    dir_inode_t->siz--;
    dir_inode_t->mtime = get_timer();

    //free pre alloc but not in used block
    if(oldsiz % (BLOCK_SIZ/sizeof(dentry_t)) == 0){
        free_block(dir_inode_t->direct[oldsiz / (BLOCK_SIZ/sizeof(dentry_t))]);
    }
    write_inode(sector, dir_inode_t->ino);
    
    if(oldsiz == 1){
        printk("del ./.. bug detected!\n");
        assert(0);
        return 0;
    }

    uint8_t temp0[512];
    uint8_t temp1[512];

    //copy the last entry to the del place
    if(dentry_idx != dir_inode_t->siz){
        get_data_block2sector(temp0, dir_inode_t->direct[dir_inode_t->siz / (BLOCK_SIZ/sizeof(dentry_t))],
                            (dir_inode_t->siz) % (BLOCK_SIZ/sizeof(dentry_t)) * sizeof(dentry_t));        
        get_data_block2sector(temp1, dir_inode_t->direct[dentry_idx/(BLOCK_SIZ/sizeof(dentry_t))],
                            (dentry_idx) % (BLOCK_SIZ/sizeof(dentry_t)) * sizeof(dentry_t));
        memcpy(temp1 + (dentry_idx) % (BLOCK_SIZ/sizeof(dentry_t)) * sizeof(dentry_t),
               temp0 + (dir_inode_t->siz) % (BLOCK_SIZ/sizeof(dentry_t)) * sizeof(dentry_t),
               sizeof(dentry_t));
        
        store_data_block2sector(temp0, dir_inode_t->direct[dir_inode_t->siz / (BLOCK_SIZ/sizeof(dentry_t))],
                            (dir_inode_t->siz) % (BLOCK_SIZ/sizeof(dentry_t)) * sizeof(dentry_t));
        store_data_block2sector(temp1, dir_inode_t->direct[dentry_idx/(BLOCK_SIZ/sizeof(dentry_t))],
                            (dentry_idx) % (BLOCK_SIZ/sizeof(dentry_t)) * sizeof(dentry_t));

    }

    uint8_t sector2[512];
    inode_t * my_inode_t = ino2inode_t(sector2, my_ino);

    for(int i = 0; i <= my_inode_t->siz; i++){
        if(i % (BLOCK_SIZ/sizeof(dentry_t)) == 0){
            free_block(my_inode_t->direct[i / (BLOCK_SIZ/sizeof(dentry_t))]);
        }
    }

    free_inode(my_ino);

    return 1;  // do_rmdir succeeds
}

int do_ls(char *path, int option)
{
    // TODO [P6-task1]: Implement do_ls
    // Note: argument 'option' serves for 'ls -l' in A-core
    if(!check_fs()){
        printk("> [FS] Error: file system is not running!\n");
        assert(0);
        return 0;
    }
    
    uint32_t dir_ino;

    if(path == NULL){
        dir_ino = current_dir;
    }
    else{
        if(!ino_DFS(current_dir, &dir_ino, path, NULL)){
            printk("> [FS] No such directory\n");
			return 0;
        }
    }

    uint8_t sector[512];
    inode_t *inode = ino2inode_t(sector, dir_ino);
    if(inode->type != INODE_DIR){
        printk("current dir is not a INODE_DIR!\n");
        assert(0);
        return 0;
    }

    printk("total entires: %d\n",inode->siz);
    uint8_t temp0[512];
    uint8_t temp1[512];
    dentry_t * dentry;
    inode_t * my_inode;
    if(option == 1){
        printk("[name] [type] [ino] [size] [links]\n");
        for(int i = 0; i < inode->siz; i++){
            if(i%(512/sizeof(dentry_t)) == 0){
                get_data_block2sector(temp0, inode->direct[i / (BLOCK_SIZ/sizeof(dentry_t))], 
                (i % (BLOCK_SIZ/sizeof(dentry_t))) * sizeof(dentry_t));
            }
            dentry = ((dentry_t *)temp0) + i % (512/sizeof(dentry_t));
            my_inode = ino2inode_t(temp1,dentry->ino);
            printk("%s, %s, %d, %d, %d\n", dentry->name, ((my_inode->type == 0) ? "DIR " : "FILE"), my_inode->ino,my_inode->siz,my_inode->links);
        }    
    }
    else{
        for(int i = 0; i < inode->siz; i++){
            if(i%(512/sizeof(dentry_t)) == 0){
                get_data_block2sector(temp0, inode->direct[i / (BLOCK_SIZ/sizeof(dentry_t))], 
                (i % (BLOCK_SIZ/sizeof(dentry_t))) * sizeof(dentry_t));
            }
            dentry = ((dentry_t *)temp0) + i % (512/sizeof(dentry_t));
            my_inode = ino2inode_t(temp1,dentry->ino);
            printk("%s ", dentry->name);
        }      
        printk("\n");   
    }
    return 1;  // do_ls succeeds
}

int do_touch(char *path)
{
    // TODO [P6-task2]: Implement do_touch
    if(!check_fs()){
        printk("> [FS] Error: file system is not running!\n");
        assert(0);
        return 0;
    }
    
    int32_t dir_ino;
    if(!getfa(&path, &dir_ino)){
        return 0;
    }
    int32_t len = strlen(path);//name length
    if(len == 0 || strcheck(path, '/') || strcheck(path, ' ') || path[0] == '-' || path[0] == '.'){
        printk("> [FS] Illegal name!\n");
        return 0;
    }

    uint32_t name[64];
    memcpy(name,path,len+1);
    if(ino_DFS(dir_ino,NULL,name,NULL)){
        printk("> [FS] name already existed!\n");
        return 0;
    }

    uint8_t sector[512];
    inode_t * pa_inode_t = ino2inode_t(sector, dir_ino);
    if(pa_inode_t->mode == P_RDONLY){
        printk("> [FS] read-only!\n");
        return 0;
    }

    int idx = pa_inode_t->siz;
    pa_inode_t->siz ++;
    pa_inode_t->mtime = get_timer();
    if(pa_inode_t->siz % ((BLOCK_SIZ/sizeof(dentry_t))) == 0){
        pa_inode_t->direct[pa_inode_t->siz / (BLOCK_SIZ/sizeof(dentry_t))] = alloc_block();
    }    
    write_inode(sector, pa_inode_t->ino);

    //mkdir inode 
    uint8_t sector2[512];
    uint32_t new_ino = alloc_inode();
    inode_t * new_t = ino2inode_t(sector2, new_ino);
    
    new_t->type = INODE_FILE;
    new_t->mode = P_RDWR;
    new_t->ino  = new_ino;

    uint32_t data_start = alloc_block();
    new_t->direct[0] = data_start;
    new_t->siz = 0;
    new_t->links = 1;
    new_t->mtime = get_timer();
    write_inode(sector2, new_ino);

    //pa
    uint32_t data_id = pa_inode_t->direct[idx / (BLOCK_SIZ/sizeof(dentry_t))];

    get_data_block2sector(sector2, data_id, (idx % (BLOCK_SIZ/sizeof(dentry_t))) * sizeof(dentry_t));
    dentry_t *pa_dentry = ((dentry_t *)sector2) + (idx % (512/sizeof(dentry_t)));
    pa_dentry->ino = new_ino;
    memcpy(pa_dentry->name, name, len+1);
    store_data_block2sector(sector2, data_id, (idx % (BLOCK_SIZ/sizeof(dentry_t))) * sizeof(dentry_t));

    get_data_block2sector(sector2, data_start, 0);
    
    memset(sector2,0,sizeof(sector2));

    store_data_block2sector(sector2, data_start, 0);

    return 1;  // do_touch succeeds
}

int do_cat(char *path)
{
    // TODO [P6-task2]: Implement do_cat
    if(!check_fs()){
        printk("> [FS] Error: file system is not running!\n");
        assert(0);
        return 0;
    }

    uint32_t file_ino;
    if(!ino_DFS(current_dir,&file_ino,path,NULL)){
        printk("> [FS] No such file/directory \n");
        return 0;
    }

    uint8_t sector[512];
    inode_t *inode = ino2inode_t(sector, file_ino);
    if(inode->type == INODE_DIR){
        printk("> [FS] Cannot open a directory!\n");
        return 0;
    }

    uint32_t siz_idx = 0;
    uint8_t temp_data[513];
    uint8_t temp0[512];
    uint8_t temp1[512];
    uint8_t temp2[512];

    for(siz_idx = 0; siz_idx < inode->siz; siz_idx += 512){
        uint32_t block_id = siz_idx/BLOCK_SIZ;
        uint32_t dir_siz = (DATA_SIZ / 4);
        if(block_id < num_direct){
            get_data_block2sector(temp_data, inode->direct[siz_idx / BLOCK_SIZ], siz_idx % BLOCK_SIZ);
        }
        else if(block_id < num_direct + num_indirect1 * dir_siz){
            uint32_t idx0 = (block_id - num_direct) / dir_siz;
            uint32_t offset0 = (block_id - num_direct) % dir_siz * 4;
            uint32_t * inode0;
            get_data_block2sector(temp0, inode->indirect_1[idx0], offset0);

            inode0 = (uint32_t *)(temp0 + (offset0 % 512)); 

            get_data_block2sector(temp_data, *inode0, siz_idx % BLOCK_SIZ);
        }
        else if(block_id < num_direct + num_indirect1 * dir_siz + num_indirect2 *dir_siz * dir_siz){
            uint32_t idx0 = (block_id - num_direct - num_indirect1 * dir_siz) / (dir_siz * dir_siz);
            uint32_t offset0 = ((block_id - num_direct - num_indirect1 * dir_siz) % (dir_siz * dir_siz)) / dir_siz * 4;
            uint32_t * inode0;
            uint32_t * inode1;
            get_data_block2sector(temp0, inode->indirect_2[idx0], offset0);

            inode0 = (uint32_t *)(temp0 + (offset0 % 512));

            uint32_t offset1 = (((block_id - num_direct - num_indirect1 * dir_siz) % (dir_siz * dir_siz)) % dir_siz * 4);
            get_data_block2sector(temp1, *inode0, offset1);

            
            inode1 = (uint32_t *)(temp1 + (offset1 % 512));

            get_data_block2sector(temp_data, *inode1, siz_idx % BLOCK_SIZ);
        }
        else {
            uint32_t offset0 = (block_id - num_direct - num_indirect1 * dir_siz - num_indirect2 * dir_siz *dir_siz) / (dir_siz * dir_siz) * 4;
            uint32_t * inode0;
            uint32_t * inode1;
            uint32_t * inode2;
            get_data_block2sector(temp0, inode->indirect_3, offset0);

            inode0 = (uint32_t *)(temp0 + (offset0 % 512));

            uint32_t offset1 = ((block_id - num_direct - num_indirect1 * dir_siz - num_indirect2 * dir_siz *dir_siz) % (dir_siz * dir_siz)) / dir_siz * 4;
            get_data_block2sector(temp1, *inode0, offset1);

            inode1 = (uint32_t *)(temp1 + (offset1 % 512));

            uint32_t offset2 = ((block_id - num_direct - num_indirect1 * dir_siz - num_indirect2 * dir_siz *dir_siz) % (dir_siz * dir_siz)) % dir_siz * 4;
            get_data_block2sector(temp2, *inode1, offset2);

            inode2 = (uint32_t *)(temp2 + (offset2 % 512));
            
            get_data_block2sector(temp_data, *inode2, siz_idx % BLOCK_SIZ);
        }
        temp_data[512] = 0;
        printk("%s", (char *)temp_data);
    }

    printk("\n");
    return 1;  // do_cat succeeds
}

int do_fopen(char *path, int mode)
{
    // TODO [P6-task2]: Implement do_fopen
    int fd = -1;
    if(!check_fs()){
        printk("> [FS] Error: file system is not running!\n");
        assert(0);
        return fd;
    }

    uint32_t file_ino;
    if(!ino_DFS(current_dir,&file_ino,path,NULL)){
        printk("> [FS] No such file/directory \n");
        return fd;
    }

    uint8_t sector[512];
    inode_t *inode = ino2inode_t(sector, file_ino);
    if(inode->type == INODE_DIR){
        printk("> [FS] Cannot open a directory!\n");
        return fd;
    }

    for(int i=0; i<NUM_FDESCS; i++){
        if(fdesc_array[i].used == 0){
            fd = i;
            break;
        }
    }

    if(fd == -1){
        printk("> [FS] No available file descriptor!\n");
        return fd;
    }

    fdesc_array[fd].used = 1;
    fdesc_array[fd].ino = inode->ino;
    fdesc_array[fd].r_cursor = 0;
    fdesc_array[fd].w_cursor = 0;

    return fd;  // return the id of file descriptor
}

int do_fread(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_fread
    if(!check_fs()){
        printk("> [FS] Error: file system is not running!\n");
        assert(0);
        return 0;
    }

    if(fd < 0 || fd >= NUM_FDESCS || fdesc_array[fd].used == 0){
        printk("> [FS] invalid fd number!\n");
        return 0;
    }

    uint32_t file_ino = fdesc_array[fd].ino;
    uint8_t sector[512];
    inode_t * inode = ino2inode_t(sector,file_ino);

    uint32_t buff_idx = 0;
    uint32_t free_siz = inode->siz - fdesc_array[fd].r_cursor;
    uint32_t remain;
    uint32_t sector_remain;

    uint32_t actuall_read;

    uint32_t r_offset = fdesc_array[fd].r_cursor;
    uint32_t flag = 1;
    if(r_offset + length > inode->siz)flag=0;

    uint32_t siz_idx = 0;
    uint8_t temp_data[512];
    uint8_t temp0[512];
    uint8_t temp1[512];
    uint8_t temp2[512];

    for(siz_idx = 0; siz_idx < (flag ? length : free_siz); siz_idx += actuall_read){
        
        r_offset = fdesc_array[fd].r_cursor + siz_idx;
        sector_remain = 512 - r_offset % 512;
        remain = length - siz_idx;

        actuall_read = (remain >= sector_remain) ? sector_remain : remain;

        uint32_t block_id = r_offset/BLOCK_SIZ;
        uint32_t dir_siz = (DATA_SIZ / 4);
        if(block_id < num_direct){
            get_data_block2sector(temp_data, inode->direct[r_offset / BLOCK_SIZ], r_offset % BLOCK_SIZ);
        }
        else if(block_id < num_direct + num_indirect1 * dir_siz){
            uint32_t idx0 = (block_id - num_direct) / dir_siz;
            uint32_t offset0 = (block_id - num_direct) % dir_siz * 4;
            uint32_t * inode0;
            get_data_block2sector(temp0, inode->indirect_1[idx0], offset0);

            inode0 = (uint32_t *)(temp0 + (offset0 % 512)); 

            get_data_block2sector(temp_data, *inode0, r_offset % BLOCK_SIZ);
        }
        else if(block_id < num_direct + num_indirect1 * dir_siz + num_indirect2 *dir_siz * dir_siz){
            uint32_t idx0 = (block_id - num_direct - num_indirect1 * dir_siz) / (dir_siz * dir_siz);
            uint32_t offset0 = ((block_id - num_direct - num_indirect1 * dir_siz) % (dir_siz * dir_siz)) / dir_siz * 4;
            uint32_t * inode0;
            uint32_t * inode1;
            get_data_block2sector(temp0, inode->indirect_2[idx0], offset0);

            inode0 = (uint32_t *)(temp0 + (offset0 % 512));

            uint32_t offset1 = (((block_id - num_direct - num_indirect1 * dir_siz) % (dir_siz * dir_siz)) % dir_siz * 4);
            get_data_block2sector(temp1, *inode0, offset1);

            
            inode1 = (uint32_t *)(temp1 + (offset1 % 512));

            get_data_block2sector(temp_data, *inode1, r_offset % BLOCK_SIZ);
        }
        else {
            uint32_t offset0 = (block_id - num_direct - num_indirect1 * dir_siz - num_indirect2 * dir_siz *dir_siz) / (dir_siz * dir_siz) * 4;
            uint32_t * inode0;
            uint32_t * inode1;
            uint32_t * inode2;
            get_data_block2sector(temp0, inode->indirect_3, offset0);

            inode0 = (uint32_t *)(temp0 + (offset0 % 512));

            uint32_t offset1 = ((block_id - num_direct - num_indirect1 * dir_siz - num_indirect2 * dir_siz *dir_siz) % (dir_siz * dir_siz)) / dir_siz * 4;
            get_data_block2sector(temp1, *inode0, offset1);

            inode1 = (uint32_t *)(temp1 + (offset1 % 512));

            uint32_t offset2 = ((block_id - num_direct - num_indirect1 * dir_siz - num_indirect2 * dir_siz *dir_siz) % (dir_siz * dir_siz)) % dir_siz * 4;
            get_data_block2sector(temp2, *inode1, offset2);

            inode2 = (uint32_t *)(temp2 + (offset2 % 512));
            
            get_data_block2sector(temp_data, *inode2, r_offset % BLOCK_SIZ);
        }
        memcpy((uint8_t *)(buff + buff_idx), temp_data + r_offset % 512, actuall_read);
        buff_idx += actuall_read;
    }

    fdesc_array[fd].r_cursor += flag ? length : free_siz;

    return flag ? length : free_siz;  // return the length of trully read data
}

int do_fwrite(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_fwrite
    if(!check_fs()){
        printk("> [FS] Error: file system is not running!\n");
        assert(0);
        return 0;
    }

    if(fd < 0 || fd >= NUM_FDESCS || fdesc_array[fd].used == 0){
        printk("> [FS] invalid fd number!\n");
        return 0;
    }

    // uint32_t * write_buffer;
    // write_buffer = buff;
    // printk("%d %d\n",write_buffer[0],write_buffer[1]); 

    uint32_t file_ino = fdesc_array[fd].ino;
    uint8_t sector[512];
    inode_t * inode = ino2inode_t(sector,file_ino);

    uint32_t oldsiz = inode->siz;
    uint32_t newsiz = fdesc_array[fd].w_cursor + length;
    uint32_t dir_siz = (DATA_SIZ / 4);

    uint8_t temp_data[512];
    uint8_t temp0[512];
    uint8_t temp1[512];
    uint8_t temp2[512];

    // printk("%d %d\n",newsiz,oldsiz);

    if(newsiz > oldsiz){
        inode->siz = newsiz;
        uint32_t oldblock = (oldsiz / BLOCK_SIZ) + 1;
        uint32_t newblock = (newsiz / BLOCK_SIZ) + 1;
        for(int idx = oldblock; idx<newblock; idx++){
            // printk("idx = %d\n",idx);
            if(idx < num_direct){
                inode->direct[idx] = alloc_block();
                // for(uint32_t offset = 0; offset < BLOCK_SIZ; offset += 512){
                //     get_data_block2sector(temp_data,inode->direct[idx],offset);
                //     memset(temp_data,0,sizeof(temp_data));
                //     store_data_block2sector(temp_data,inode->direct[idx],offset);
                // } 
            }
            else if(idx < num_direct + num_indirect1 * dir_siz){
                uint32_t idx0 = (idx - num_direct) / dir_siz;
                uint32_t offset0 = (idx - num_direct) % dir_siz * 4;     
                if(offset0 == 0)inode->indirect_1[idx0] = alloc_block();         

                uint32_t * inode0;
                get_data_block2sector(temp0, inode->indirect_1[idx0], offset0);
                inode0 = (uint32_t *)(temp0 + (offset0 % 512)); 
                *inode0 = alloc_block();
                store_data_block2sector(temp0, inode->indirect_1[idx0], offset0);

                // for(uint32_t offset = 0; offset < BLOCK_SIZ; offset += 512){
                //     get_data_block2sector(temp_data,*inode0,offset);
                //     memset(temp_data,0,sizeof(temp_data));
                //     store_data_block2sector(temp_data,*inode0,offset);
                // } 

            }
            else if(idx < num_direct + num_indirect1 * dir_siz + num_indirect2 * dir_siz * dir_siz){
                uint32_t idx0 = (idx - num_direct - num_indirect1 * dir_siz) / (dir_siz * dir_siz);
                uint32_t offset0 = ((idx - num_direct - num_indirect1 * dir_siz) % (dir_siz * dir_siz)) / dir_siz * 4;
                uint32_t * inode0;
                uint32_t * inode1;      
                if(((idx - num_direct - num_indirect1 * dir_siz) % (dir_siz * dir_siz)) == 0)inode->indirect_2[idx0] = alloc_block();

                get_data_block2sector(temp0, inode->indirect_2[idx0], offset0);
                inode0 = (uint32_t *)(temp0 + (offset0 % 512));                 
                uint32_t offset1 = (((idx - num_direct - num_indirect1 * dir_siz) % (dir_siz * dir_siz)) % dir_siz * 4);
                
                if(offset1 == 0){
                    *inode0 = alloc_block();
                    store_data_block2sector(temp0, inode->indirect_2[idx0], offset0);    
                } 

                get_data_block2sector(temp1, *inode0, offset1);                
                inode1 = (uint32_t *)(temp1 + (offset1 % 512));
                *inode1 = alloc_block();
                store_data_block2sector(temp1, *inode0, offset1);

                // for(uint32_t offset = 0; offset < BLOCK_SIZ; offset += 512){
                //     get_data_block2sector(temp_data,*inode1,offset);
                //     memset(temp_data,0,sizeof(temp_data));
                //     store_data_block2sector(temp_data,*inode1,offset);
                // }                 
            }
            else{
                uint32_t offset0 = (idx - num_direct - num_indirect1 * dir_siz - num_indirect2 * dir_siz *dir_siz) / (dir_siz * dir_siz) * 4;
                uint32_t * inode0;
                uint32_t * inode1;
                uint32_t * inode2;    
                if(idx == num_direct + num_indirect1 * dir_siz + num_indirect2 * dir_siz * dir_siz) inode->indirect_3 = alloc_block();

                get_data_block2sector(temp0, inode->indirect_3, offset0);
                inode0 = (uint32_t *)(temp0 + (offset0 % 512));
                if(((idx - num_direct - num_indirect1 * dir_siz - num_indirect2 * dir_siz *dir_siz) % (dir_siz * dir_siz)) == 0){
                    *inode0 = alloc_block();
                    store_data_block2sector(temp0, inode->indirect_3, offset0);    
                } 

                uint32_t offset1 = ((idx - num_direct - num_indirect1 * dir_siz - num_indirect2 * dir_siz *dir_siz) % (dir_siz * dir_siz)) / dir_siz * 4;
                get_data_block2sector(temp1, *inode0, offset1);
                inode1 = (uint32_t *)(temp1 + (offset1 % 512)); 
                uint32_t offset2 = ((idx - num_direct - num_indirect1 * dir_siz - num_indirect2 * dir_siz *dir_siz) % (dir_siz * dir_siz)) % dir_siz * 4;
                if(offset2 == 0){
                    *inode1 = alloc_block();
                    store_data_block2sector(temp1, *inode0, offset1);
                }               

                get_data_block2sector(temp2, *inode1, offset2);
                inode2 = (uint32_t *)(temp2 + (offset2 % 512));                
                *inode2 = alloc_block();
                store_data_block2sector(temp2, *inode1, offset2);

                // for(uint32_t offset = 0; offset < BLOCK_SIZ; offset += 512){
                //     get_data_block2sector(temp_data,*inode2,offset);
                //     memset(temp_data,0,sizeof(temp_data));
                //     store_data_block2sector(temp_data,*inode2,offset);
                // }

            }
        }
    }

    write_inode(sector, inode->ino);

    uint32_t buff_idx = 0;
    uint32_t remain;
    uint32_t sector_remain;
    uint32_t actuall_write;    
    uint32_t w_offset = fdesc_array[fd].w_cursor;

    for(uint32_t siz_idx = 0; siz_idx < length; siz_idx += actuall_write){
        
        w_offset = fdesc_array[fd].w_cursor + siz_idx;
        sector_remain = 512 - w_offset % 512;
        remain = length - siz_idx;

        actuall_write = (remain >= sector_remain) ? sector_remain : remain;

        uint32_t block_id = w_offset/BLOCK_SIZ;
        uint32_t dir_siz = (DATA_SIZ / 4);
        if(block_id < num_direct){
            get_data_block2sector(temp_data, inode->direct[w_offset / BLOCK_SIZ], w_offset % BLOCK_SIZ);
            memcpy(temp_data + w_offset % 512, (uint8_t *)(buff + buff_idx), actuall_write);
            store_data_block2sector(temp_data, inode->direct[w_offset / BLOCK_SIZ], w_offset % BLOCK_SIZ);
        }
        else if(block_id < num_direct + num_indirect1 * dir_siz){
            uint32_t idx0 = (block_id - num_direct) / dir_siz;
            uint32_t offset0 = (block_id - num_direct) % dir_siz * 4;
            uint32_t * inode0;
            get_data_block2sector(temp0, inode->indirect_1[idx0], offset0);

            inode0 = (uint32_t *)(temp0 + (offset0 % 512)); 

            // if(idx0 == 0 && offset0 == 0){
            //     printk("inode0 %d *inode0 %d %d  woffset = %d \n",inode0,*inode0,*((uint32_t *)(buff + buff_idx)),w_offset);
            // }

            get_data_block2sector(temp_data, *inode0, w_offset % BLOCK_SIZ);
            memcpy(temp_data + w_offset % 512, (uint8_t *)(buff + buff_idx), actuall_write);
            store_data_block2sector(temp_data, *inode0, w_offset % BLOCK_SIZ);
        }
        else if(block_id < num_direct + num_indirect1 * dir_siz + num_indirect2 *dir_siz * dir_siz){
            uint32_t idx0 = (block_id - num_direct - num_indirect1 * dir_siz) / (dir_siz * dir_siz);
            uint32_t offset0 = ((block_id - num_direct - num_indirect1 * dir_siz) % (dir_siz * dir_siz)) / dir_siz * 4;
            uint32_t * inode0;
            uint32_t * inode1;
            get_data_block2sector(temp0, inode->indirect_2[idx0], offset0);

            inode0 = (uint32_t *)(temp0 + (offset0 % 512));

            uint32_t offset1 = (((block_id - num_direct - num_indirect1 * dir_siz) % (dir_siz * dir_siz)) % dir_siz * 4);
            get_data_block2sector(temp1, *inode0, offset1);

            
            inode1 = (uint32_t *)(temp1 + (offset1 % 512));

            get_data_block2sector(temp_data, *inode1, w_offset % BLOCK_SIZ);
            memcpy(temp_data + w_offset % 512, (uint8_t *)(buff + buff_idx), actuall_write);
            store_data_block2sector(temp_data, *inode1, w_offset % BLOCK_SIZ);
        }
        else {
            uint32_t offset0 = (block_id - num_direct - num_indirect1 * dir_siz - num_indirect2 * dir_siz *dir_siz) / (dir_siz * dir_siz) * 4;
            uint32_t * inode0;
            uint32_t * inode1;
            uint32_t * inode2;
            get_data_block2sector(temp0, inode->indirect_3, offset0);

            inode0 = (uint32_t *)(temp0 + (offset0 % 512));

            uint32_t offset1 = ((block_id - num_direct - num_indirect1 * dir_siz - num_indirect2 * dir_siz *dir_siz) % (dir_siz * dir_siz)) / dir_siz * 4;
            get_data_block2sector(temp1, *inode0, offset1);

            inode1 = (uint32_t *)(temp1 + (offset1 % 512));

            uint32_t offset2 = ((block_id - num_direct - num_indirect1 * dir_siz - num_indirect2 * dir_siz *dir_siz) % (dir_siz * dir_siz)) % dir_siz * 4;
            get_data_block2sector(temp2, *inode1, offset2);

            inode2 = (uint32_t *)(temp2 + (offset2 % 512));
            
            get_data_block2sector(temp_data, *inode2, w_offset % BLOCK_SIZ);
            memcpy(temp_data + w_offset % 512, (uint8_t *)(buff + buff_idx), actuall_write);
            store_data_block2sector(temp_data, *inode2, w_offset % BLOCK_SIZ);
        }
        buff_idx += actuall_write;
    }
    fdesc_array[fd].w_cursor += length;

    return length;  // return the length of trully written data
}

int do_fclose(int fd)
{
    // TODO [P6-task2]: Implement do_fclose
    if(!check_fs()){
        printk("> [FS] Error: file system is not running!\n");
        assert(0);
        return 0;
    }

    if(fd < 0 || fd >= NUM_FDESCS || fdesc_array[fd].used == 0){
        printk("> [FS] invalid fd number!\n");
        return 0;
    }

    fdesc_array[fd].used = 0;

    return 1;  // do_fclose succeeds
}

int do_ln(char *src_path, char *dst_path)
{
    // TODO [P6-task2]: Implement do_ln
    if(!check_fs()){
        printk("> [FS] Error: file system is not running!\n");
        assert(0);
        return 0;
    }

    uint32_t src_paino;
    uint32_t dst_paino;

    if(!getfa(&src_path, &src_paino) || !getfa(&dst_path, &dst_paino)){
        printk("> [FS] Illegal directory!\n");
        return 0;
    }

    // printk("IN!\n");

    int len_dst = strlen(dst_path);
    int len_src = strlen(src_path);

    uint32_t src_ino;
    uint8_t name_copy[64];
    memcpy(name_copy,src_path,len_src);
    if(!ino_DFS(src_paino,&src_ino,src_path,NULL)){
        printk("> [FS] No such source file/dirctory!\n");
        return 0;
    }

    if(len_dst == 0 || strcheck(dst_path, '/') || strcheck(dst_path, ' ') || dst_path[0] == '-' || dst_path[0] == '.'
     ||len_src == 0 || strcheck(src_path, '/') || strcheck(src_path, ' ') || src_path[0] == '-' || src_path[0] == '.'){
        printk("> [FS] Illegal name!\n");
        return 0;
    }    

    //src 
    uint8_t src_sector[512];
    inode_t * src_inode = ino2inode_t(src_sector,src_ino);
    if(src_inode->type != INODE_FILE){
        printk("> [FS] Cannot link a directory!\n");
        return 0;
    }

    //dst fa
    uint8_t dstfa_sector[512];
    inode_t * dstfa_inode = ino2inode_t(dstfa_sector,dst_paino);
    if(dstfa_inode->mode == P_RDONLY){
        printk("> [FS] Souce directory is ready-only!\n");
        return 0;
    }
    src_inode->links ++;
    write_inode(src_sector,src_ino);

    int idx = dstfa_inode->siz;
    dstfa_inode->siz++;
    dstfa_inode->mtime = get_timer();
    if(dstfa_inode->siz % ((BLOCK_SIZ/sizeof(dentry_t))) == 0){
        dstfa_inode->direct[dstfa_inode->siz / (BLOCK_SIZ/sizeof(dentry_t))] = alloc_block();
    }    
    write_inode(dstfa_sector, dstfa_inode->ino);

    //entry
    uint8_t sector2[512];
    uint32_t data_id = dstfa_inode->direct[idx / (BLOCK_SIZ/sizeof(dentry_t))];
    get_data_block2sector(sector2, data_id, (idx % (BLOCK_SIZ/sizeof(dentry_t))) * sizeof(dentry_t));
    dentry_t *pa_dentry = ((dentry_t *)sector2) + (idx % (512/sizeof(dentry_t)));
    pa_dentry->ino = src_ino;
    memcpy(pa_dentry->name, dst_path, len_dst+1);
    store_data_block2sector(sector2, data_id, (idx % (BLOCK_SIZ/sizeof(dentry_t))) * sizeof(dentry_t));    

    return 1;  // do_ln succeeds 
}

int do_rm(char *path)
{
    // TODO [P6-task2]: Implement do_rm
    if(!check_fs()){
        printk("> [FS] Error: file system is not running!\n");
        assert(0);
        return 0;
    }
    uint32_t dir_ino;
    if(!getfa(&path, &dir_ino)){
        return 0;
    }

    int len = strlen(path);
    if(len == 0 || strcheck(path, '/') || strcheck(path, ' ') || path[0] == '-' || path[0] == '.'){
        printk("> [FS] Illegal name!\n");
        return 0;
    }

    uint8_t name_copy[64];
    uint32_t my_ino;
    uint32_t dentry_idx;
    memcpy(name_copy,path,len+1);
    if(!ino_DFS(dir_ino,&my_ino,(char *)name_copy,&dentry_idx)){
        printk("> No such file!\n");
        return 0;
    }

    uint8_t sector[512];
    inode_t * dir_inode_t = ino2inode_t(sector,dir_ino);
    if(dir_inode_t->mode == P_RDONLY){
        printk("> [FS] Ready-only!\n");
        return 0;
    }

    uint32_t oldsiz = dir_inode_t->siz;
    dir_inode_t->siz--;
    dir_inode_t->mtime = get_timer();

    //free pre alloc but not in used block
    if(oldsiz % (BLOCK_SIZ/sizeof(dentry_t)) == 0){
        free_block(dir_inode_t->direct[oldsiz / (BLOCK_SIZ/sizeof(dentry_t))]);
    }
    write_inode(sector, dir_inode_t->ino);

    uint8_t temp0[512];
    uint8_t temp1[512];
    uint8_t temp2[512];

    // printk("%d!\n",dir_inode_t->direct[0]);
    
    //copy the last entry to the del place
    if(dentry_idx != dir_inode_t->siz){
        get_data_block2sector(temp0, dir_inode_t->direct[dir_inode_t->siz / (BLOCK_SIZ/sizeof(dentry_t))],
                            (dir_inode_t->siz) % (BLOCK_SIZ/sizeof(dentry_t)) * sizeof(dentry_t));        
        get_data_block2sector(temp1, dir_inode_t->direct[dentry_idx/(BLOCK_SIZ/sizeof(dentry_t))],
                            (dentry_idx) % (BLOCK_SIZ/sizeof(dentry_t)) * sizeof(dentry_t));
        memcpy(temp1 + (dentry_idx) % (BLOCK_SIZ/sizeof(dentry_t)) * sizeof(dentry_t),
               temp0 + (dir_inode_t->siz) % (BLOCK_SIZ/sizeof(dentry_t)) * sizeof(dentry_t),
               sizeof(dentry_t));
        
        store_data_block2sector(temp0, dir_inode_t->direct[dir_inode_t->siz / (BLOCK_SIZ/sizeof(dentry_t))],
                            (dir_inode_t->siz) % (BLOCK_SIZ/sizeof(dentry_t)) * sizeof(dentry_t));
        store_data_block2sector(temp1, dir_inode_t->direct[dentry_idx/(BLOCK_SIZ/sizeof(dentry_t))],
                            (dentry_idx) % (BLOCK_SIZ/sizeof(dentry_t)) * sizeof(dentry_t));

    }    

    inode_t * my_inode = ino2inode_t(sector, my_ino);
    if(my_inode->links > 1){
        my_inode->links --;
        write_inode(sector,my_ino);
        return 1;
    }

    uint32_t rm_idx;

    for(rm_idx = 0; rm_idx < my_inode->siz; rm_idx += 512){
        uint32_t block_id = rm_idx/BLOCK_SIZ;
        uint32_t dir_siz = (DATA_SIZ / 4);
        if(block_id < num_direct){
            free_block(my_inode->direct[rm_idx / BLOCK_SIZ]);
        }
        else if(block_id < num_direct + num_indirect1 * dir_siz){
            uint32_t idx0 = (block_id - num_direct) / dir_siz;
            uint32_t offset0 = (block_id - num_direct) % dir_siz * 4;
            uint32_t * inode0;
            get_data_block2sector(temp0, my_inode->indirect_1[idx0], offset0);

            inode0 = (uint32_t *)(temp0 + (offset0 % 512)); 

            free_block(*inode0);
        }
        else if(block_id < num_direct + num_indirect1 * dir_siz + num_indirect2 *dir_siz * dir_siz){
            uint32_t idx0 = (block_id - num_direct - num_indirect1 * dir_siz) / (dir_siz * dir_siz);
            uint32_t offset0 = ((block_id - num_direct - num_indirect1 * dir_siz) % (dir_siz * dir_siz)) / dir_siz * 4;
            uint32_t * inode0;
            uint32_t * inode1;
            get_data_block2sector(temp0, my_inode->indirect_2[idx0], offset0);

            inode0 = (uint32_t *)(temp0 + (offset0 % 512));

            uint32_t offset1 = (((block_id - num_direct - num_indirect1 * dir_siz) % (dir_siz * dir_siz)) % dir_siz * 4);
            get_data_block2sector(temp1, *inode0, offset1);

            
            inode1 = (uint32_t *)(temp1 + (offset1 % 512));

            free_block(*inode1);
        }
        else {
            uint32_t offset0 = (block_id - num_direct - num_indirect1 * dir_siz - num_indirect2 * dir_siz *dir_siz) / (dir_siz * dir_siz) * 4;
            uint32_t * inode0;
            uint32_t * inode1;
            uint32_t * inode2;
            get_data_block2sector(temp0, my_inode->indirect_3, offset0);

            inode0 = (uint32_t *)(temp0 + (offset0 % 512));

            uint32_t offset1 = ((block_id - num_direct - num_indirect1 * dir_siz - num_indirect2 * dir_siz *dir_siz) % (dir_siz * dir_siz)) / dir_siz * 4;
            get_data_block2sector(temp1, *inode0, offset1);

            inode1 = (uint32_t *)(temp1 + (offset1 % 512));

            uint32_t offset2 = ((block_id - num_direct - num_indirect1 * dir_siz - num_indirect2 * dir_siz *dir_siz) % (dir_siz * dir_siz)) % dir_siz * 4;
            get_data_block2sector(temp2, *inode1, offset2);

            inode2 = (uint32_t *)(temp2 + (offset2 % 512));
            
            free_block(*inode2);
        }
    }

    free_inode(my_ino);

    return 1;  // do_rm succeeds 
}

int do_lseek(int fd, int offset, int whence)
{
    // TODO [P6-task2]: Implement do_lseek
    if(!check_fs()){
        printk("> [FS] Error: file system is not running!\n");
        assert(0);
        return 0;
    }

    if(fd < 0 || fd >= NUM_FDESCS || fdesc_array[fd].used == 0){
        printk("> [FS] invalid fd number!\n");
        return 0;
    }

    uint32_t file_ino = fdesc_array[fd].ino;
    uint8_t sector[512];
    inode_t * inode = ino2inode_t(sector,file_ino);

    switch(whence){
        case SEEK_SET:
            fdesc_array[fd].r_cursor = offset;
            fdesc_array[fd].w_cursor = offset;
            break;
        case SEEK_CUR:
            fdesc_array[fd].r_cursor += offset;
            fdesc_array[fd].w_cursor += offset;
            break;
        case SEEK_END:
            fdesc_array[fd].r_cursor = inode->siz + offset;
            fdesc_array[fd].w_cursor = inode->siz + offset;
            break;
        default:
            break;
    }    

    return fdesc_array[fd].r_cursor;  // the resulting offset location from the beginning of the file
}
