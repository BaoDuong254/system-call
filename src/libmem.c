/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c
 */

#include "string.h"
#include "mm.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
// int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
// {
//     struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

//     if (rg_elmt->rg_start >= rg_elmt->rg_end)
//         return -1;

//     if (rg_node != NULL)
//         rg_elmt->rg_next = rg_node;

//     /* Enlist the new region */
//     mm->mmap->vm_freerg_list = rg_elmt;

//     return 0;
// }
/*enlist_vm_freerg_list - add new rg to freerg_list, sorted + merged*/
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
    struct vm_rg_struct **prev = &mm->mmap->vm_freerg_list;
    struct vm_rg_struct *cur = mm->mmap->vm_freerg_list;

    if (rg_elmt->rg_start >= rg_elmt->rg_end)
        return -1;

    while (cur && cur->rg_start < rg_elmt->rg_start)
    {
        prev = &cur->rg_next;
        cur = cur->rg_next;
    }
    rg_elmt->rg_next = cur;
    *prev = rg_elmt;

    cur = mm->mmap->vm_freerg_list;
    while (cur && cur->rg_next)
    {
        if (cur->rg_end >= cur->rg_next->rg_start)
        {
            if (cur->rg_next->rg_end > cur->rg_end)
                cur->rg_end = cur->rg_next->rg_end;
            struct vm_rg_struct *tofree = cur->rg_next;
            cur->rg_next = tofree->rg_next;
            free(tofree);
        }
        else
        {
            cur = cur->rg_next;
        }
    }
    return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
    if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
        return NULL;

    return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
    pthread_mutex_lock(&mmvm_lock);
    struct vm_rg_struct rgnode;

    if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
    {
        caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
        caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;

        *alloc_addr = rgnode.rg_start;
    }
    else
    {
        struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

        int inc_sz = PAGING_PAGE_ALIGNSZ(size);

        int old_sbrk = cur_vma->sbrk;

        struct sc_regs regs;
        regs.a1 = SYSMEM_INC_OP;
        regs.a2 = vmaid;
        regs.a3 = inc_sz;

        int ret = syscall(caller, 17, &regs);
        if (ret != 0)
        {
            pthread_mutex_unlock(&mmvm_lock);
            return ret;
        }
        cur_vma->sbrk = old_sbrk + inc_sz;
        *alloc_addr = old_sbrk;
        if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
        {
            caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
            caller->mm->symrgtbl[rgid].rg_end = old_sbrk + inc_sz;
        }
    }
    pthread_mutex_unlock(&mmvm_lock);
    printf("===== PHYSICAL MEMORY AFTER ALLOCATION =====\n");
#ifdef IODUMP
    printf("PID=%d - Region=%d - Address=%08lx - Size=%d byte\n",
           caller->pid, rgid, (unsigned long)*alloc_addr, size);
#ifdef PAGETBL_DUMP
    print_pgtbl(caller, 0, -1);
#endif
#endif
    return 0;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
    // struct vm_rg_struct rgnode;

    // Dummy initialization for avoding compiler dummay warning
    // in incompleted TODO code rgnode will overwrite through implementing
    // the manipulation of rgid later

    if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
        return -1;

    /* TODO: Manage the collect freed region to freerg_list */

    /*enlist the obsoleted memory region */
    // enlist_vm_freerg_list();
    struct vm_rg_struct *freerg = (struct vm_rg_struct *)malloc(sizeof(struct vm_rg_struct));
    if (freerg == NULL)
        return -1;
    pthread_mutex_lock(&mmvm_lock);

    freerg->rg_start = caller->mm->symrgtbl[rgid].rg_start;
    freerg->rg_end = caller->mm->symrgtbl[rgid].rg_end;
    freerg->rg_next = NULL;

    enlist_vm_freerg_list(caller->mm, freerg);

    caller->mm->symrgtbl[rgid].rg_start = 0;
    caller->mm->symrgtbl[rgid].rg_end = 0;
    pthread_mutex_unlock(&mmvm_lock);
    printf("===== PHYSICAL MEMORY AFTER DEALLOCATION =====\n");
#ifdef IODUMP
    printf("PID=%d - Region=%d\n",
           caller->pid, rgid);
#ifdef PAGETBL_DUMP
    print_pgtbl(caller, 0, -1); // print max TBL
#endif
#endif
    return 0;
}

/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int liballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
    /* TODO Implement allocation on vm area 0 */
    int addr;

    /* By default using vmaid = 0 */
    return __alloc(proc, 0, reg_index, size, &addr);
}

/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int libfree(struct pcb_t *proc, uint32_t reg_index)
{
    /* TODO Implement free region */

    /* By default using vmaid = 0 */
    return __free(proc, 0, reg_index);
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
    pthread_mutex_lock(&mmvm_lock);
    uint32_t pte = mm->pgd[pgn];

    if (!PAGING_PAGE_PRESENT(pte))
    {
        int vicpgn, vicfpn, tgtfpn, swpfpn;
        uint32_t vicpte;

        find_victim_page(caller->mm, &vicpgn);
        vicpte = mm->pgd[vicpgn];
        vicfpn = PAGING_PTE_FPN(vicpte);

        MEMPHY_get_freefp(caller->active_mswp, &swpfpn);

        struct sc_regs regs_out;
        regs_out.a1 = SYSMEM_SWP_OP;
        regs_out.a2 = vicfpn;
        regs_out.a3 = swpfpn;
        int status = syscall(caller, 17, &regs_out);
        if (status != 0)
        {
            pthread_mutex_unlock(&mmvm_lock);
            return status;
        }

        pte_set_swap(&mm->pgd[vicpgn], caller->active_mswp_id, swpfpn);

        tgtfpn = vicfpn;

        int tgt_swpoff = PAGING_PTE_SWP(pte);

        struct sc_regs regs_in;
        regs_in.a1 = SYSMEM_SWP_OP;
        regs_in.a2 = tgt_swpoff;
        regs_in.a3 = tgtfpn;
        status = syscall(caller, 17, &regs_in);
        if (status != 0)
        {
            pthread_mutex_unlock(&mmvm_lock);
            return status;
        }

        pte_set_fpn(&mm->pgd[pgn], tgtfpn);
        PAGING_PTE_SET_PRESENT(mm->pgd[pgn]);

        enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
    }

    *fpn = PAGING_FPN(mm->pgd[pgn]);
    pthread_mutex_unlock(&mmvm_lock);
    return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
    int pgn = PAGING_PGN(addr);
    int off = PAGING_OFFST(addr);
    int fpn;

    /* Get the page to MEMRAM, swap from MEMSWAP if needed */
    if (pg_getpage(mm, pgn, &fpn, caller) != 0)
        return -1; /* invalid page access */

    /* TODO
     *  MEMPHY_read(caller->mram, phyaddr, data);
     *  MEMPHY READ
     *  SYSCALL 17 sys_memmap with SYSMEM_IO_READ
     */
    int phyaddr = fpn * PAGING_PAGESZ + off;
    return MEMPHY_read(caller->mram, phyaddr, data);
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
    int pgn = PAGING_PGN(addr);
    int off = PAGING_OFFST(addr);
    int fpn;

    /* Get the page to MEMRAM, swap from MEMSWAP if needed */
    if (pg_getpage(mm, pgn, &fpn, caller) != 0)
        return -1; /* invalid page access */

    /* TODO
     *  MEMPHY_write(caller->mram, phyaddr, value);
     *  MEMPHY WRITE
     *  SYSCALL 17 sys_memmap with SYSMEM_IO_WRITE
     */
    int phyaddr = fpn * PAGING_PAGESZ + off;
    return MEMPHY_write(caller->mram, phyaddr, value);
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
{
    struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

    if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
        return -1;

    pg_getval(caller->mm, currg->rg_start + offset, data, caller);
    return 0;
}

/*libread - PAGING-based read a region memory */
int libread(
    struct pcb_t *proc, // Process executing the instruction
    uint32_t source,    // Index of source register
    uint32_t offset,    // Source address = [source] + [offset]
    uint32_t *destination)
{
    BYTE data;
    int val = __read(proc, 0, source, offset, &data);

    /* TODO update result of reading action*/
    // destination
    *destination = data;
#ifdef IODUMP
    printf("===== PHYSICAL MEMORY AFTER READING =====\n");
    printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1); // print max TBL
#endif
    MEMPHY_dump(proc->mram);
#endif

    return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
{
    struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

    if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
        return -1;

    pg_setval(caller->mm, currg->rg_start + offset, value, caller);
    return 0;
}

/*libwrite - PAGING-based write a region memory */
int libwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register
    uint32_t offset)
{
    int write = __write(proc, 0, destination, offset, data);

#ifdef IODUMP
    printf("===== PHYSICAL MEMORY AFTER WRITING =====\n");
    printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1); // print max TBL
#endif
    MEMPHY_dump(proc->mram);
#endif
    write = 0;
    return write;
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
    int pagenum, fpn;
    uint32_t pte;

    for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
    {
        pte = caller->mm->pgd[pagenum];

        if (!PAGING_PAGE_PRESENT(pte))
        {
            fpn = PAGING_PTE_FPN(pte);
            MEMPHY_put_freefp(caller->mram, fpn);
        }
        else
        {
            fpn = PAGING_PTE_SWP(pte);
            MEMPHY_put_freefp(caller->active_mswp, fpn);
        }
    }

    return 0;
}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, int *retpgn)
{
    if (mm->fifo_pgn != NULL)
    {
        struct pgn_t *pPage = NULL;
        struct pgn_t *lPage = mm->fifo_pgn;
        while (lPage->pg_next != NULL)
        {
            pPage = lPage;
            lPage = lPage->pg_next;
        }
        *retpgn = lPage->pgn;
        if (pPage == NULL)
        {
            mm->fifo_pgn = lPage->pg_next;
        }
        else
        {
            pPage->pg_next = lPage->pg_next;
        }

        free(lPage);

        return 0;
    }
    *retpgn = -1;
    return -1;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    if (cur_vma == NULL)
        return -1;

    struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;
    if (rgit == NULL)
        return -1;

    /* Probe unintialized newrg */
    newrg->rg_start = newrg->rg_end = -1;

    /* TODO Traverse on list of free vm region to find a fit space */
    // while (...)
    //  ..
    while (rgit != NULL)
    {
        if (rgit->rg_start + size <= rgit->rg_end)
        {
            newrg->rg_start = rgit->rg_start;
            newrg->rg_end = rgit->rg_start + size;

            if (rgit->rg_start + size < rgit->rg_end)
            {
                rgit->rg_start = rgit->rg_start + size;
            }
            else
            {

                struct vm_rg_struct *nextrg = rgit->rg_next;

                if (nextrg != NULL)
                {
                    rgit->rg_start = nextrg->rg_start;
                    rgit->rg_end = nextrg->rg_end;
                    rgit->rg_next = nextrg->rg_next;
                    free(nextrg);
                }
                else
                {
                    rgit->rg_start = rgit->rg_end;
                    rgit->rg_next = NULL;
                }
            }
            break;
        }
        else
        {
            rgit = rgit->rg_next;
        }
    }

    if (newrg->rg_start == -1)
    {
        return -1;
    }

    return 0;
}

// #endif
