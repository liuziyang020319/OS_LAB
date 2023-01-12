/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                                   Memory Management
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */
#ifndef MM_H
#define MM_H

#include <type.h>
#include <pgtable.h>

#define MAP_KERNEL 1
#define MAP_USER 2
#define MEM_SIZE 32
#define PAGE_SIZE 4096 // 4K
#define INIT_KERNEL_STACK 0xffffffc052000000

#define FREEMEM_KERNEL (INIT_KERNEL_STACK+2*PAGE_SIZE)
#define FREEMEM_END INIT_KERNEL_STACK + 252*PAGE_SIZE
// #define FREEMEM_END INIT_KERNEL_STACK + 702*PAGE_SIZE

/* Rounding; only works for n = power of two */
#define ROUND(a, n)     (((((uint64_t)(a))+(n)-1)) & ~((n)-1))
#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n)-1))

extern ptr_t allocPage(int numPage);


// TODO [P4-task1] */
void freePage(ptr_t baseAddr);
typedef struct free_mem_node{
    int nxt;
    char valid;
}free_mem_node;
extern free_mem_node fm_pool[];
extern int fm_head;

// extern free_mem_node sw_mem_pool[];//available swap mem addr 

extern uint64_t padding_ADDR;

typedef struct alloc_mem_node{
    int pid;
    ptr_t pa;
    ptr_t va;
    PTE * pmd3;
    char valid;
}alloc_mem_node;

extern alloc_mem_node am_pool[];
extern int am_head;
extern int am_tail;
extern int am_siz;

typedef struct swap_mem_node{
    int pid;
    ptr_t va;
    PTE * pmd3;
    char valid;   
}swap_mem_node;

extern swap_mem_node sw_pool[];
extern int sw_top;
extern void init_swap_page();


// #define S_CORE
// NOTE: only need for S-core to alloc 2MB large page
#ifdef S_CORE
#define LARGE_PAGE_FREEMEM 0xffffffc056000000
#define USER_STACK_ADDR 0x400000
extern ptr_t allocLargePage(int numPage);
#else
// NOTE: A/C-core
#define USER_STACK_ADDR 0xf00010000
#endif

// TODO [P4-task1] */
extern void* kmalloc(size_t size);
extern void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir);
extern uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir, int swap);

// TODO [P4-task4]: shm_page_get/dt */
#define SHM_NUM 16
typedef struct SHM_NODE{
    uint64_t pa;
    uint64_t key;
    uint64_t siz;
    uint8_t valid;
}SHM_NODE;

void init_schm_page();
uintptr_t shm_page_get(int key);
void shm_page_dt(uintptr_t addr);
void shm_map(uintptr_t va, uintptr_t pa, uintptr_t pgdir);
uintptr_t get_shm_pa(uintptr_t va, uintptr_t pgdir);

uintptr_t get_va2pa(uintptr_t va);
void snapshot(uintptr_t va1,uintptr_t va2);
void set_read_only(uintptr_t va,uintptr_t pa, uintptr_t pgdir);

#endif /* MM_H */
