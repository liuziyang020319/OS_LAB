#include <os/mm.h>
#include <os/string.h>
#include <os/smp.h>
#include <os/sched.h>
#include <printk.h>
#include <os/kernel.h>
#include <assert.h>
// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;

free_mem_node fm_pool[4096] = {{-1,0}};
int fm_head = -1;

// free_mem_node sw_mem_pool[4096] = {{-1}};
// int sw_mem_head = -1;
// uint64_t sw_mem_top = 0;

// ptr_t allocSwapPos()
// {
//     ptr_t ret;
//     if(sw_mem_head >= 0){
//         int temp = sw_mem_head;
//         ret = sw_mem_head * PAGE_SIZE + padding_ADDR;
//         sw_mem_head = sw_mem_pool[sw_mem_head].nxt;
//         sw_mem_pool[temp].nxt = -1;
//         return ret;
//     }
//     ret = sw_mem_top * PAGE_SIZE + padding_ADDR;
//     sw_mem_top ++;
//     return ret;
// }


alloc_mem_node am_pool[4096] = {{0,0,0,0}};
int am_head=0;
int am_tail=0;
int am_siz =0;

swap_mem_node sw_pool[4096];
int sw_top =-1;

void swap_error(){
    printk("> [MEM] no page to swap\n");
    assert(0);
}

void init_swap_page(){
    for(int i=0; i<4096; i++){
        sw_pool[i].valid = 0;
        fm_pool[i].valid = 0;
    }
}

ptr_t swap_page(){
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0; 
    while(am_siz > 0 && am_pool[am_head].valid == 0){
        am_head ++;
        am_siz--;
    }
    if(am_siz == 0){
        swap_error();
        return;
    }
    for(int i=0; i<4096; i++){
        if(sw_pool[i].valid == 0){
            if(i > sw_top)
                sw_top = i;
            uint64_t pa = am_pool[am_head].pa;
            sw_pool[i].valid = 1;
            sw_pool[i].pid   = (*current_running)->parent_id ? (*current_running)->parent_id : (*current_running)->pid;
            sw_pool[i].va    = am_pool[am_head].va;
            sw_pool[i].pmd3  = am_pool[am_head].pmd3;
            am_head++;
            if(am_head == 4096)
                am_head = 0;
            am_siz--;
            *(sw_pool[i].pmd3) = (PTE) 0; 
            local_flush_tlb_all();
            uint64_t bias = padding_ADDR/512;
            bios_sdwrite(pa,8,bias+8*i); 
            printl("swap successful ! %ld\n",sw_pool[i].va);
            return pa2kva(pa);          
        }
    }    
    // swap_error();
    return NULL;
}

ptr_t allocPage(int numPage)
{
    // align PAGE_SIZE
    ptr_t ret;
    if(fm_head >= 0){
        int temp = fm_head;
        ret = fm_head * PAGE_SIZE + FREEMEM_KERNEL;
        fm_head = fm_pool[fm_head].nxt;
        fm_pool[temp].nxt = -1;
        fm_pool[temp].valid = 0;
        // printl("ret node %ld\n",ret);
        return ret;
    }
    
    ret = ROUND(kernMemCurr, PAGE_SIZE);
    if(ret + numPage * PAGE_SIZE <= FREEMEM_END){
        kernMemCurr = ret + numPage * PAGE_SIZE;
        return ret;        
    }
    ret = swap_page();
    return ret;
}

// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage)
{
    // align LARGE_PAGE_SIZE
    ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
    largePageMemCurr = ret + numPage * LARGE_PAGE_SIZE;
    return ret;    
}
#endif

void freePage(ptr_t baseAddr)
{
    // TODO [P4-task1] (design you 'freePage' here if you need):
    PTE *pgdir = (PTE *)baseAddr;
    PTE *pmd, *pmd2, *pmd3;
    int last = fm_head;
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    for(int i=0; i<512; i++){
        if(pgdir[i] % 2){
            pmd = (PTE *)pa2kva(get_pa(pgdir[i]));
            
            if(pmd < FREEMEM_KERNEL) continue; //kernal no release
            // printl("pmd %ld \n",pmd);

            for(int j = 0; j<512; ++j){
                if(pmd[j] % 2){
                    pmd2 = (PTE *)pa2kva(get_pa(pmd[j]));
                    for(int k = 0; k<512; ++k){
                        if(pmd2[k] % 2){
                            pmd3 = (PTE *)pa2kva(get_pa(pmd2[k]));
                            uint32_t node_index = ((uint64_t)pmd3 - FREEMEM_KERNEL)/PAGE_SIZE;
                            if(fm_pool[node_index].valid)continue;
                            fm_pool[node_index].valid = 1;
                            fm_pool[node_index].nxt = -1;
                            if(fm_head == -1){
                                fm_head = node_index;
                                last = fm_head;
                            }
                            else{
                                for(;fm_pool[last].nxt >= 0; last = fm_pool[last].nxt);
                                fm_pool[last].nxt = node_index;
                            }
                        }
                    }
                    uint32_t node_index = ((uint64_t)pmd2 - FREEMEM_KERNEL)/PAGE_SIZE;
                    if(fm_pool[node_index].valid)continue;
                    fm_pool[node_index].valid = 1;
                    fm_pool[node_index].nxt = -1;
                    if(fm_head == -1){
                        fm_head = node_index;
                        last = fm_head;
                    }
                    else{
                        for(;fm_pool[last].nxt >= 0;last = fm_pool[last].nxt);
                        fm_pool[last].nxt = node_index;
                    }
                }
            }
            uint32_t node_index = ((uint64_t)pmd - FREEMEM_KERNEL)/PAGE_SIZE;
            if(fm_pool[node_index].valid)continue;
            fm_pool[node_index].valid = 1;
            fm_pool[node_index].nxt = -1;
            if(fm_head == -1){
                fm_head = node_index;
                last = fm_head;
            }
            else{
                for(;fm_pool[last].nxt >= 0;last = fm_pool[last].nxt);
                fm_pool[last].nxt = node_index;
            }
        }
    }
    uint32_t node_index = ((uint64_t)pgdir - FREEMEM_KERNEL)/PAGE_SIZE;
    if(fm_pool[node_index].valid == 0){
        fm_pool[node_index].valid = 1;
        fm_pool[node_index].nxt = -1;
        if(fm_head == -1){
            fm_head = node_index;
            last = fm_head;
        }
        else{
            for(;fm_pool[last].nxt >= 0;last = fm_pool[last].nxt);
            fm_pool[last].nxt=node_index;
        }        
    }

    for(int i=0; i <= sw_top; i++){
        if(sw_pool[i].pid == (*current_running)->pid)
            sw_pool[i].valid = 0; 

    }
    while(sw_top >=0&&sw_pool[sw_top].valid ==0)
        sw_top--;
    for(int i=0; i < 4096; i++){
        if(am_pool[i].pid == (*current_running)->pid)
            am_pool[i].valid = 0;
    }
}

void *kmalloc(size_t size)
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):
    ptr_t ret = ROUND(kernMemCurr, size);
    kernMemCurr = ret + size;
    return (void *)ret;
}


/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // TODO [P4-task1] share_pgtable:
    memcpy((uint8_t *)dest_pgdir, (uint8_t *)src_pgdir, (uint32_t)PAGE_SIZE);
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
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

SHM_NODE shms[SHM_NUM];

void init_schm_page(){
    for(int i=0;i<SHM_NUM;i++){
        shms[i].key   = 0;
        shms[i].pa    = 0;
        shms[i].siz   = 0;
        shms[i].valid = 0;
    }
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    int find = 0;
    int id=0;
    for(int i=0;i<SHM_NUM;i++){
        if(shms[i].valid == 0)continue;
        if(shms[i].key == key){
            find = 1;
            id = i;
            break;
        }
    }
    if(find){
        shms[id].siz++;
        ((*current_running)->shm_num) +=1;
        uintptr_t va = USER_STACK_ADDR - (2 + ((*current_running)->shm_num)) * PAGE_SIZE;
        shm_map(va,shms[id].pa,(*current_running)->pgdir);
        local_flush_tlb_all();
        return va;
    }
    else{
        for(int i=0;i<SHM_NUM;i++){
            if(shms[i].valid == 0){
                id = i;
                break;
            }
        }
        shms[id].valid = 1;
        shms[id].key = key;
        shms[id].pa = kva2pa(allocPage(1));
        shms[id].siz = 1;
        ((*current_running)->shm_num) +=1;
        uintptr_t va = USER_STACK_ADDR - (2 + ((*current_running)->shm_num)) * PAGE_SIZE;
        printl("MAP pgdir = %ld addr = %d\n",(*current_running)->pgdir,va);
        shm_map(va,shms[id].pa,(*current_running)->pgdir);
        printl("va = %ld pa =%ld\n",va,shms[id].pa);
        local_flush_tlb_all();       
        return va; 
    }

}





void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    printl("DT pgdir = %ld addr = %d\n",(*current_running)->pgdir,addr);
    uintptr_t pa = get_shm_pa(addr,(*current_running)->pgdir);
    local_flush_tlb_all();

    for(int i=0;i<SHM_NUM;i++){
        if(shms[i].valid == 0)continue;
        printl("%ld %ld\n",shms[i].pa,pa);
        if(shms[i].pa == pa){
            printl("HAPPEN!\n");
            shms[i].siz--;
            if(shms[i].siz == 0)shms[i].valid = 0;
            break;
        }
    }
}

void shm_map(uintptr_t va, unsigned long pa, uintptr_t pgdir){
    va &= VA_MASK;
    PTE * pgdir_t = (PTE *)pgdir;
    uint64_t vpn2 = (va   >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS));
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va   >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^
                    (vpn1 << (PPN_BITS)) ^
                    (va   >> (NORMAL_PAGE_SHIFT)); 
    if(pgdir_t[vpn2] % 2 == 0){
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

    if(pmd2[vpn0] % 2 == 1){
        printk("> WRONG SHM PGAE VADDR CHOICE!");
        assert(0);
    }

    pmd2[vpn0] = 0;
    
    set_pfn(&pmd2[vpn0],(pa >> NORMAL_PAGE_SHIFT));
    set_attribute(&pmd2[vpn0],_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC
                             |_PAGE_ACCESSED| _PAGE_DIRTY| _PAGE_USER);      
    printl("SET PA = %ld %ld\n",pmd2[vpn0],pa);
}

uintptr_t get_shm_pa(uintptr_t va, uintptr_t pgdir){
    va &= VA_MASK;
    PTE * pgdir_t = (PTE *)pgdir;
    uint64_t vpn2 = (va   >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS));
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va   >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^
                    (vpn1 << (PPN_BITS)) ^
                    (va   >> (NORMAL_PAGE_SHIFT)); 
    if(pgdir_t[vpn2] % 2 == 0){
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
    
    if(pmd2[vpn0] % 2 == 0){
        printk("> WRONG SHM PGAE VADDR CHECK!");
        assert(0);
    }   
    
    uint64_t ret = get_pa(pmd2[vpn0]);
    printl("DETEC !!=%ld %ld\n",ret,pmd2[vpn0]);
    pmd2[vpn0] = 0;
    
    return ret;     
}


uintptr_t get_va2pa(uintptr_t va){
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    PTE * pgdir_t = (PTE *)((*current_running)->pgdir);
    va &= VA_MASK;
    uint64_t vpn2 = (va   >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS));
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va   >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^
                    (vpn1 << (PPN_BITS)) ^
                    (va   >> (NORMAL_PAGE_SHIFT)); 
    if(pgdir_t[vpn2] % 2 == 0){
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
    
    if(pmd2[vpn0] % 2 == 0){
        printk("> WRONG SHM PGAE VADDR CHECK!");
        assert(0);
    }   
    
    uintptr_t ret = get_pa(pmd2[vpn0]);
    return ret;    
}

void snapshot(uintptr_t va1,uintptr_t va2){
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    uintptr_t pa1 = get_va2pa(va1);
    set_read_only(va1,pa1,(*current_running)->pgdir);
    local_flush_tlb_all();
    set_read_only(va2,pa1,(*current_running)->pgdir);
    local_flush_tlb_all();
}

void set_read_only(uintptr_t va,uintptr_t pa, uintptr_t pgdir){
    va &= VA_MASK;
    PTE * pgdir_t = (PTE *)pgdir;
    uint64_t vpn2 = (va   >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS));
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va   >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^
                    (vpn1 << (PPN_BITS)) ^
                    (va   >> (NORMAL_PAGE_SHIFT));
    
    if(pgdir_t[vpn2] % 2 == 0){
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
    
    pmd2[vpn0] = 0;
    set_pfn(&pmd2[vpn0],(pa >> NORMAL_PAGE_SHIFT));
    set_attribute(&pmd2[vpn0],_PAGE_PRESENT | _PAGE_READ | _PAGE_ACCESSED| _PAGE_DIRTY| _PAGE_USER);
}