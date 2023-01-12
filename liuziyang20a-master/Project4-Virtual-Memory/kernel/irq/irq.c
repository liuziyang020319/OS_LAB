#include <os/irq.h>
#include <os/time.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/kernel.h>
#include <os/smp.h>
#include <os/mm.h>
#include <printk.h>
#include <assert.h>
#include <screen.h>

handler_t irq_table[IRQC_COUNT];
handler_t exc_table[EXCC_COUNT];

void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task3] & [p2-task4] interrupt handler.
    // call corresponding handler by the value of `scause`
    uint64_t irq_type = scause &  (((uint64_t)1) << 63);
    uint64_t irq_code = scause & ~(((uint64_t)1) << 63);
    // if(irq_code != 5 && irq_code!=8){
    //     printk("irq code %ld %d %d %d %d\n",irq_code,exc_table[irq_code],handle_other,handle_syscall,handle_irq_timer);
    //     // assert(0);
    // }
        
    if(irq_type){
        irq_table[irq_code](regs,stval,scause);
    }
    else{
        exc_table[irq_code](regs,stval,scause);
    }
}

void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task4] clock interrupt handler.
    // Note: use bios_set_timer to reset the timer and remember to reschedule
    screen_reflush();
    check_sleeping();
    
    bios_set_timer(get_ticks() + TIMER_INTERVAL);

    do_scheduler();
}
// 12,13,15
void init_exception()
{
    /* TODO: [p2-task3] initialize exc_table */
    /* NOTE: handle_syscall, handle_other, etc.*/
    int i;
    for(i = 0;i < EXCC_COUNT; i++){
        exc_table[i] = handle_other;
    }
    
    exc_table[EXCC_INST_PAGE_FAULT] = handle_pagefault;
    exc_table[EXCC_LOAD_PAGE_FAULT] = handle_pagefault;
    exc_table[EXCC_STORE_PAGE_FAULT] = handle_pagefault;

    exc_table[EXCC_SYSCALL] = handle_syscall;
    /* TODO: [p2-task4] initialize irq_table */
    /* NOTE: handle_int, handle_other, etc.*/

    for(i = 0;i < IRQC_COUNT; i++){
        irq_table[i] = handle_other;
    }
    
    irq_table[IRQC_S_TIMER] = handle_irq_timer;
    /* TODO: [p2-task3] set up the entrypoint of exceptions */

    setup_exception();
}

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

void handle_other(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    char* reg_name[] = {
        "zero "," ra  "," sp  "," gp  "," tp  ",
        " t0  "," t1  "," t2  ","s0/fp"," s1  ",
        " a0  "," a1  "," a2  "," a3  "," a4  ",
        " a5  "," a6  "," a7  "," s2  "," s3  ",
        " s4  "," s5  "," s6  "," s7  "," s8  ",
        " s9  "," s10 "," s11 "," t3  "," t4  ",
        " t5  "," t6  "
    };
    for (int i = 0; i < 32; i += 3) {
        for (int j = 0; j < 3 && i + j < 32; ++j) {
            printk("%s : %016lx ",reg_name[i+j], regs->regs[i+j]);
        }
        printk("\n\r");
    }
    printk("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lu\n\r",
           regs->sstatus, regs->sbadaddr, regs->scause);
    printk("sepc: 0x%lx\n\r", regs->sepc);
    printk("tval: 0x%lx cause: 0x%lx\n", stval, scause);
    assert(0);
}
