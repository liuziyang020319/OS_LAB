# 项目介绍
本次实验的主要要求是实现虚实地址空间得管理、换页机制。在基本实验的基础上，我也实现了双核的启动与运行。
## 完成的实验任务与重要BUG
### 实验任务1：启用虚存机制进入内核
本部分难度很小，因为启用虚存机制进入内核的大部分内容已经被实现。唯一值得注意的是，板卡要求每次读入至多64块。除此之外，还应当注意内存在0x50000000-0x51000000有一段直射，需要取消这个直射，否则在rw测试程序访问这一段地址会出错。

为了能够进入shell，需要设计分页函数，我设计的分页函数就是最普通的三级页表分页函数。
```c
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir, int swap)
{
    // TODO [P4-task1] alloc_page_helper:
    va &= VA_MASK;
    PTE * pgdir_t = (PTE *)pgdir;
    uint64_t vpn2 = (va   >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS));
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va   >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^
                    (vpn1 << (PPN_BITS)) ^
                    (va   >> (NORMAL_PAGE_SHIFT));
    
    if(pgdir_t[vpn2] % 2 == 0){
        // printl("IN ! %d\n",pgdir_t[vpn2]);
        // alloc second - level page
        pgdir_t[vpn2] = 0;
        set_pfn(&pgdir_t[vpn2], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgdir_t[vpn2],_PAGE_PRESENT | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY);
        clear_pgdir(pa2kva(get_pa(pgdir_t[vpn2])));
    }

    PTE *pmd = (PTE *)pa2kva(get_pa(pgdir_t[vpn2]));
    
    if(pmd[vpn1] % 2 == 0){
        // alloc third - level page
        pmd[vpn1] = 0;
        set_pfn(&pmd[vpn1], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd[vpn1],_PAGE_PRESENT | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY);
        clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
    }

    PTE *pmd2 = (PTE *)pa2kva(get_pa(pmd[vpn1]));    

    uint64_t pa = kva2pa(allocPage(1));
    pmd2[vpn0] = 0;
    
    set_pfn(&pmd2[vpn0],(pa >> NORMAL_PAGE_SHIFT));
    set_attribute(&pmd2[vpn0],_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC
                             |_PAGE_ACCESSED| _PAGE_DIRTY| _PAGE_USER);
    // printl("vpn2 %d %d vpn1 %d %d vpn0 %d %d pa %d | %d %d %d\n",vpn2,&pgdir_t[vpn2],vpn1,&pmd[vpn1],vpn0,&pmd2[vpn0],pa,pgdir_t[vpn2],pmd[vpn1],pmd2[vpn0]);                         
    
    if(swap && am_siz < 4096){
        am_pool[am_tail].pid  = (*current_running)->parent_id ? (*current_running)->parent_id : (*current_running)->pid;
        am_pool[am_tail].pa   = pa;
        am_pool[am_tail].pmd3 = &pmd2[vpn0];
        am_pool[am_tail].va   = (va >> 12) << 12;
        am_pool[am_tail].valid=1;
        am_tail++;
        if(am_tail == 4096)
            am_tail = 0;
        am_siz++;
        printl("get valid swap page ! %d %ld\n",va,pmd2[vpn0]);
    }
    
    return pa2kva(pa);
}
```
### 实验任务1续：执行用户程序
exec实验的修改值得注意，主要是新设计的argv应当是用户态程序看见的虚地址，但是应该是放在内核内存分配的页上。

核心代码：
```c
    uintptr_t va = alloc_page_helper(pcb[id].user_sp - PAGE_SIZE,pcb[id].pgdir, 0) + PAGE_SIZE;
    uintptr_t argv_va_base = va - sizeof(uintptr_t)*(argc + 1);
    uintptr_t argv_pa_base = USER_STACK_ADDR - sizeof(uintptr_t)*(argc + 1);

    uint64_t user_sp_va = argv_va_base;
    uint64_t user_sp_pa = argv_pa_base;
    uintptr_t * argv_ptr = (ptr_t)argv_va_base;
    for(int i=0; i<argc; i++){
        uint32_t len = strlen(argv[i]);
        user_sp_va = user_sp_va - len - 1;
        user_sp_pa = user_sp_pa - len - 1;
        (*argv_ptr) = user_sp_pa;
        strcpy(user_sp_va,argv[i]);
        argv_ptr ++;
    }
    (*argv_ptr) = 0;
    uint64_t tot = va - user_sp_va;
    uint64_t siz_aliagnment = ((tot/128)+((tot%128==0)?0:1))*128;
    user_sp_va = va - siz_aliagnment;
    pcb[id].user_sp = pcb[id].user_sp - siz_aliagnment;

    // load_task_img(tasks[i].name);
    uint64_t ENTRY_POINT = load_task_img(tasks[i].name,pcb[id].pgdir);
    init_pcb_stack(pcb[id].kernel_sp,pcb[id].user_sp,ENTRY_POINT,pcb+id,argc,argv_pa_base);
```
其次是应该修改制作镜像的设计，因为读入bios_read的读入地址不再是一个固定的值。这个修改并不困难，只需要记录一下ELF文件的关键数据即可。
修改后的loader文件如下：
```c
    uint64_t ENTRY_POINT = 0;
    for(int id=0; id<tasknum; ++id){
        if(strcmp(tasks[id].name,taskname) == 0){
            uint64_t offset = tasks[id].offset;
            for(int i = 0; i < tasks[id].p_memsz; i += NORMAL_PAGE_SIZE, offset += NORMAL_PAGE_SIZE){
                if(i < tasks[id].p_filesz){
                    uint64_t va = alloc_page_helper(tasks[id].p_vaddr + i,pgdir,0);
                    uint64_t block_id = offset / 512;
                    uint64_t block_offset = offset - block_id * 512;
                    uint64_t SIZENOW = (tasks[id].p_filesz - i >= NORMAL_PAGE_SIZE) ? NORMAL_PAGE_SIZE : tasks[id].p_filesz - i;
                    uint64_t block_size = (offset + SIZENOW)/512 - (offset/512) + 1;
                    // printl("va: %ld\n",va);
                    uint64_t bias = va - INIT_KERNEL_STACK;
                    bios_sdread(INIT_KERNEL_STACK,block_size,block_id);
                    uint64_t to;
                    uint64_t from;
                    for(int j=0; j < SIZENOW; ++j){
                        to = va + j;
                        from = va + j + block_offset - bias;
                        char * toptr = to;
                        char * fromptr = from;
                        *toptr = *fromptr;
                    }
                    if(SIZENOW != NORMAL_PAGE_SIZE){
                        for(int j=SIZENOW; j < NORMAL_PAGE_SIZE; ++j){
                            to = va + j;
                            char * toptr = to;
                            *toptr = 0;
                        }
                    }
                }
                else{
                    uint64_t va = alloc_page_helper(tasks[id].p_vaddr + i,pgdir,0);
                    uint64_t to;
                    for(int j=0; j < NORMAL_PAGE_SIZE; ++j){
                        to = va + j;
                        char * toptr = to;
                        *toptr = 0;
                    }
                }
            }
            return tasks[id].p_vaddr;
        }
    }
```

由于bios对不同应用程序的存储并不要求对4KB做对齐，所以读入程序在添加了换页逻辑后应当不能像原来那样往后写，然后向前移位。我的选择是将其存储在一个可以肯定是空的位置，然后再选择移位。



### 实验任务2 动态页表和按需调页
本部分只需要修改相关缺页逻辑，然后为其分配一个新的页即可。
```c
void handle_pagefault(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    
    int pid =(*current_running)->parent_id ? (*current_running)->parent_id : (*current_running)->pid;
    
    for(int i=0; i<=sw_top; i++){
        if(sw_pool[i].valid && pid == sw_pool[i].pid && ((stval >> 12) << 12) == sw_pool[i].va){
            uint64_t kva = allocPage(1);
            uint64_t bias = padding_ADDR/512;
            bios_sdread(kva2pa(kva), 8, bias + 8*i);
            PTE * pmd3 = sw_pool[i].pmd3;
            set_pfn(pmd3,kva2pa(kva) >> NORMAL_PAGE_SHIFT);
            set_attribute(pmd3,_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC
                             |_PAGE_ACCESSED| _PAGE_DIRTY| _PAGE_USER);
            local_flush_tlb_all();
            sw_pool[i].valid = 0;
            if(i == sw_top){
                sw_top --;
            }
            printl("retain swap page successful! %ld\n",sw_pool[i].va);
            if(am_siz < 4096){
                am_pool[am_tail].pid = (*current_running)->pid;
                am_pool[am_tail].pa = kva2pa(kva);
                am_pool[am_tail].pmd3 = pmd3;
                am_pool[am_tail].va = ((stval >> 12) << 12);
                am_pool[am_tail].valid = 1;
                am_tail++;
                if(am_tail == 4096)
                    am_tail = 0;
                am_siz++;
            }
            return;
        }
    }

    alloc_page_helper(stval,(*current_running)->pgdir,1);
    local_flush_tlb_all();
}
```
### 实验任务3/4/5/6 
对于多线程部分，其和前面实验的设计是类似的，只需要将逻辑复用即可。对于换页逻辑，只需要记录下可以换取的页，然后每当内存不够，就执行换页逻辑。
内存共享、和写时复制的核心都是实现内存共享，具体实现方式是复用分配页逻辑，然后再获得实际PA的时候，用需要共享的PA即可。

## 运行代码的流程：
### 脚本运行
```
make all
make run
```