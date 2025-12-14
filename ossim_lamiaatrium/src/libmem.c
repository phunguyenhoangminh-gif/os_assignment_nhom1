/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
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
#include "mm64.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/* Helper to calculate PGN/OFFSET correctly based on mode */
static void get_pgn_offset(addr_t addr, int *pgn, int *off) {
#ifdef MM64
    *pgn = addr >> PAGING64_ADDR_PT_SHIFT;
    *off = addr & PAGING64_ADDR_OFFST_MASK;
#else
    *pgn = PAGING_PGN(addr);
    *off = PAGING_OFFST(addr);
#endif
}

/* Helper to calculate PHYADDR correctly */
static int get_phyaddr(int fpn, int off) {
#ifdef MM64
    return (fpn << PAGING64_ADDR_PT_SHIFT) + off;
#else
    return (fpn << PAGING_ADDR_FPN_LOBIT) + off;
#endif
}

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;
  if (rg_elmt->rg_start >= rg_elmt->rg_end) return -1;
  if (rg_node != NULL) rg_elmt->rg_next = rg_node;
  mm->mmap->vm_freerg_list = rg_elmt;
  return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ) return NULL;
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
int __alloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr)
{
  pthread_mutex_lock(&mmvm_lock);
  struct vm_rg_struct rgnode;
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  int inc_sz=0;

  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
    *alloc_addr = rgnode.rg_start;
    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }

#ifdef MM64
  inc_sz = (uint32_t)(size/(int)PAGING64_PAGESZ);
  inc_sz = inc_sz + 1;
#else
  inc_sz = PAGING_PAGE_ALIGNSZ(size);
#endif
  int old_sbrk = cur_vma->sbrk;
  inc_sz = inc_sz + 1;

  struct sc_regs regs;
  regs.a1 = SYSMEM_INC_OP;
  regs.a2 = vmaid;
#ifdef MM64
  regs.a3 = size;
#else
  regs.a3 = PAGING_PAGE_ALIGNSZ(size);
#endif 

  if (syscall(caller->krnl, caller->pid, 17, &regs) == -1) {
      pthread_mutex_unlock(&mmvm_lock);
      return -1; 
  }

  caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
  caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size;
  *alloc_addr = old_sbrk;

  pthread_mutex_unlock(&mmvm_lock);
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
  pthread_mutex_lock(&mmvm_lock);
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  struct vm_rg_struct *rgnode = get_symrg_byid(caller->mm, rgid);
  if (rgnode->rg_start == 0 && rgnode->rg_end == 0) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  struct vm_rg_struct *freerg_node = malloc(sizeof(struct vm_rg_struct));
  freerg_node->rg_start = rgnode->rg_start;
  freerg_node->rg_end = rgnode->rg_end;
  freerg_node->rg_next = NULL;
  rgnode->rg_start = rgnode->rg_end = 0;
  rgnode->rg_next = NULL;
  enlist_vm_freerg_list(caller->mm, freerg_node);
  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int liballoc(struct pcb_t *proc, addr_t size, uint32_t reg_index)
{
  printf("%s:%d\n", __func__, __LINE__);
  fflush(stdout);
  addr_t addr;
  int val = __alloc(proc, 0, reg_index, size, &addr);
  if (val == -1) {
    // printf("[ERROR] liballoc failed for process %d\n", proc->pid);
    return -1;
  }
#ifdef IODUMP
  /* TODO dump IO content (if needed) */
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif
  
  return val;
}

/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int libfree(struct pcb_t *proc, uint32_t reg_index)
{
  printf("%s:%d\n", __func__, __LINE__);
  fflush(stdout);
  int val = __free(proc, 0, reg_index);
  if (val == -1) {
    // printf("[ERROR] libfree failed for process %d\n", proc->pid);
    return -1;
  }
#ifdef IODUMP
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); 
#endif
#endif

  return val;
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
  uint32_t pte = pte_get_entry(caller, pgn);

  if (!PAGING_PAGE_PRESENT(pte))
  { 
    addr_t tgtfpn;
    if (MEMPHY_get_freefp(caller->krnl->mram, &tgtfpn) == 0)
    {
      if (pte != 0 && (pte & PAGING_PTE_SWAPPED_MASK))
      {
        addr_t old_swpfpn = PAGING_SWP(pte);
        struct sc_regs regs;
        regs.a1 = SYSMEM_SWP_OP;
        regs.a2 = old_swpfpn;
        regs.a3 = tgtfpn;
        syscall(caller->krnl, caller->pid, 17, &regs);
        MEMPHY_put_freefp(caller->krnl->active_mswp, old_swpfpn);
      }
      pte_set_fpn(caller, pgn, tgtfpn);
      enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
    }
    else
    {
      addr_t vicpgn, swpfpn, vicfpn;
      if (find_victim_page(caller->mm, &vicpgn) == -1) return -1;
      if (MEMPHY_get_freefp(caller->krnl->active_mswp, &swpfpn) == -1) return -1;

      uint32_t vicpte = pte_get_entry(caller, vicpgn);
      vicfpn = PAGING_FPN(vicpte);

      struct sc_regs regs;
      regs.a1 = SYSMEM_SWP_OP;
      regs.a2 = vicfpn;
      regs.a3 = swpfpn;
      syscall(caller->krnl, caller->pid, 17, &regs);

      pte_set_swap(caller, vicpgn, 0, swpfpn);
      tgtfpn = vicfpn;

      if (pte != 0 && (pte & PAGING_PTE_SWAPPED_MASK))
      {
        addr_t old_swpfpn = PAGING_SWP(pte);
        regs.a1 = SYSMEM_SWP_OP;
        regs.a2 = old_swpfpn;
        regs.a3 = tgtfpn;
        syscall(caller->krnl, caller->pid, 17, &regs);
        MEMPHY_put_freefp(caller->krnl->active_mswp, old_swpfpn);
      }
      pte_set_fpn(caller, pgn, tgtfpn);
      enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
    }
  }
  *fpn = PAGING_FPN(pte_get_entry(caller, pgn));
  return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, addr_t addr, BYTE *data, struct pcb_t *caller)
{
  int pgn, off, fpn;
  get_pgn_offset(addr, &pgn, &off); 

  if (pg_getpage(mm, pgn, &fpn, caller) != 0) return -1;

  int phyaddr = get_phyaddr(fpn, off); 

  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_READ;
  regs.a2 = phyaddr;
  regs.a3 = 0;
  
  if (syscall(caller->krnl, caller->pid, 17, &regs) == 0) {
      *data = (BYTE)regs.a3;
      return 0;
  }
  return -1;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, addr_t addr, BYTE value, struct pcb_t *caller)
{
  int pgn, off, fpn;
  get_pgn_offset(addr, &pgn, &off); 

  if (pg_getpage(mm, pgn, &fpn, caller) != 0) return -1;

  int phyaddr = get_phyaddr(fpn, off); 

  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_WRITE;
  regs.a2 = phyaddr;
  regs.a3 = (unsigned int)value;
  
  if (syscall(caller->krnl, caller->pid, 17, &regs) == 0) {
      return 0;
  }
  return -1;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
  pthread_mutex_lock(&mmvm_lock);
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  if (currg->rg_start + offset >= currg->rg_end) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1; 
  }

  pg_getval(caller->mm, currg->rg_start + offset, data, caller);
  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*libread - PAGING-based read a region memory */
int libread(
    struct pcb_t *proc, // Process executing the instruction
    uint32_t source,    // Index of source register
    addr_t offset,    // Source address = [source] + [offset]
    uint32_t* destination)
{
  printf("%s:%d\n", __func__, __LINE__);
  fflush(stdout);
  BYTE data;
  int val = __read(proc, 0, source, offset, &data);
  if (val == -1) {
      // printf("[ERROR] libread failed for process %d\n", proc->pid);
      return -1;
  }
  *destination = (uint32_t)data;
#ifdef IODUMP
  /* TODO dump IO content (if needed) */
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
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
int __write(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
  pthread_mutex_lock(&mmvm_lock);
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  if (currg->rg_start + offset >= currg->rg_end) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  pg_setval(caller->mm, currg->rg_start + offset, value, caller);
  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*libwrite - PAGING-based write a region memory */
int libwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register
    addr_t offset)
{
  printf("%s:%d\n", __func__, __LINE__);
  fflush(stdout);
  int val = __write(proc, 0, destination, offset, data);
  
#ifdef IODUMP
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
  MEMPHY_dump(proc->krnl->mram);
#endif
  
  if (val == -1) {
    // printf("[ERROR] libwrite failed for process %d\n", proc->pid);
    return -1;
  }

  return val;
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  pthread_mutex_lock(&mmvm_lock);
  int pagenum, fpn;
  uint32_t pte;

  for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte = caller->mm->pgd[pagenum];
    if (PAGING_PAGE_PRESENT(pte)) {
      fpn = PAGING_FPN(pte);
      MEMPHY_put_freefp(caller->krnl->mram, fpn);
    } else {
      fpn = PAGING_SWP(pte);
      MEMPHY_put_freefp(caller->krnl->active_mswp, fpn);
    }
  }
  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, addr_t *retpgn)
{
  struct pgn_t *pg = mm->fifo_pgn;
  if (pg == NULL) return -1;
  if (pg->pg_next == NULL) {
    *retpgn = pg->pgn;
    mm->fifo_pgn = NULL;
    free(pg);
    return 0;
  }
  struct pgn_t *prev = NULL;
  while (pg->pg_next != NULL) {
    prev = pg;
    pg = pg->pg_next;
  }
  *retpgn = pg->pgn;
  if (prev != NULL) prev->pg_next = NULL;
  free(pg);
  return 0;
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
  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;
  if (rgit == NULL) return -1;

  newrg->rg_start = newrg->rg_end = -1;

  while (rgit != NULL)
  {
    if (rgit->rg_start + size <= rgit->rg_end) { 
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;
      if (rgit->rg_start + size < rgit->rg_end) {
        rgit->rg_start = rgit->rg_start + size;
      } else { 
        struct vm_rg_struct *nextrg = rgit->rg_next;
        if (nextrg != NULL) {
          rgit->rg_start = nextrg->rg_start;
          rgit->rg_end = nextrg->rg_end;
          rgit->rg_next = nextrg->rg_next;
          free(nextrg);
        } else {                                
          rgit->rg_start = rgit->rg_end; 
          rgit->rg_next = NULL;
        }
      }
      break;
    } else {
      rgit = rgit->rg_next;
    }
  }
  if (newrg->rg_start == -1) return -1;
  return 0;
}
// #endif