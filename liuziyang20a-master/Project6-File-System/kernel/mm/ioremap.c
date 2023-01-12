#include <os/ioremap.h>
#include <os/mm.h>
#include <pgtable.h>
#include <type.h>
#include <os/smp.h>
#include <os/sched.h>

// maybe you can map it to IO_ADDR_START ?
static uintptr_t io_base = IO_ADDR_START;

void *ioremap(unsigned long phys_addr, unsigned long size)
{
    // TODO: [p5-task1] map one specific physical region to virtual address
    int i = 0;
    uint64_t va_start;
    while(size > 0){
        uint64_t va = io_base;
        if(i == 0){
            va_start = va;
        }
        va &= VA_MASK;
        PTE *pgdir_t = (PTE *)pa2kva(PGDIR_PA);
        uint64_t vpn2 = (va   >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS));
        uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                        (va   >> (NORMAL_PAGE_SHIFT + PPN_BITS));
        uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^
                        (vpn1 << (PPN_BITS)) ^
                        (va   >> (NORMAL_PAGE_SHIFT));

        if(pgdir_t[vpn2] % 2 == 0){
            // alloc second - level page
            pgdir_t[vpn2] = 0;
            set_pfn(&pgdir_t[vpn2], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
            set_attribute(&pgdir_t[vpn2],_PAGE_PRESENT);
            clear_pgdir(pa2kva(get_pa(pgdir_t[vpn2])));
        }

        PTE *pmd = (PTE *)pa2kva(get_pa(pgdir_t[vpn2]));
        
        if(pmd[vpn1] % 2 == 0){
            // alloc third - level page
            pmd[vpn1] = 0;
            set_pfn(&pmd[vpn1], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
            set_attribute(&pmd[vpn1],_PAGE_PRESENT);
            clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
        }

        PTE *pmd2 = (PTE *)pa2kva(get_pa(pmd[vpn1]));    

        uint64_t pa = phys_addr;
        pmd2[vpn0] = 0;
        
        set_pfn(&pmd2[vpn0],(pa >> NORMAL_PAGE_SHIFT));
        set_attribute(&pmd2[vpn0],_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC
                                |_PAGE_ACCESSED| _PAGE_DIRTY);
        io_base += PAGE_SIZE;
        phys_addr += PAGE_SIZE;
        size -= PAGE_SIZE;

        i++;
    }

    local_flush_tlb_all();

    return (void *)va_start;
}

void iounmap(void *io_addr)
{
    // TODO: [p5-task1] a very naive iounmap() is OK
    // maybe no one would call this function?
}
