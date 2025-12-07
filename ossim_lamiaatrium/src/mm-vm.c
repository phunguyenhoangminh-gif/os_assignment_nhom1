/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "syscall.h" 

int __swap_cp_page(struct memphy_struct *mpsrc, addr_t srcfpn,
                   struct memphy_struct *mpdst, addr_t dstfpn)
{
  int cellidx;
  addr_t addrsrc, addrdst;
  for (cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++)
  {
    addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING_PAGESZ + cellidx;

    BYTE data;
    MEMPHY_read(mpsrc, addrsrc, &data);
    MEMPHY_write(mpdst, addrdst, data);
  }

  return 0;
}

int __mm_swap_page(struct pcb_t *caller, addr_t vicfpn , addr_t swpfpn)
{
    __swap_cp_page(caller->krnl->mram, vicfpn, caller->krnl->active_mswp, swpfpn);
    return 0;
}

struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  struct vm_area_struct *pvma = mm->mmap;
  if (mm->mmap == NULL) return NULL;
  int vmait = pvma->vm_id;
  while (vmait < vmaid)
  {
    if (pvma == NULL) return NULL;
    pvma = pvma->vm_next;
    vmait = pvma->vm_id;
  }
  return pvma;
}

struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, addr_t size, addr_t alignedsz)
{
  struct vm_rg_struct * newrg = malloc(sizeof(struct vm_rg_struct));
  if (newrg == NULL) return NULL;
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
  if (cur_vma == NULL) return NULL;
  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end = newrg->rg_start + size;
  newrg->rg_next = NULL;
  return newrg;
}

int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, addr_t vmastart, addr_t vmaend)
{
  if (vmastart >= vmaend) return -1;
  struct vm_area_struct *vma = caller->krnl->mm->mmap;
  if (vma == NULL) return -1;
  while (vma != NULL)
  {
    if (vma->vm_id == vmaid) {
      vma = vma->vm_next;
      continue;
    }
    if (((vmastart >= vma->vm_start && vmastart < vma->vm_end) ||
        (vmaend > vma->vm_start && vmaend <= vma->vm_end) ||
        (vmastart <= vma->vm_start && vmaend >= vma->vm_end)))
    {
      return -1;
    }
    vma = vma->vm_next;
  }
  return 0;
}

int inc_vma_limit(struct pcb_t *caller, int vmaid, addr_t inc_sz)
{
  struct vm_rg_struct * newrg = malloc(sizeof(struct vm_rg_struct));
  int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
  int incnumpage =  inc_amt / PAGING_PAGESZ;
  struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
  if (cur_vma== NULL) return -1;
  int old_end = cur_vma->vm_end;
  if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0) return -1;
  if (vm_map_ram(caller, area->rg_start, area->rg_end, cur_vma->sbrk, incnumpage, newrg) < 0) return -1;
  enlist_vm_rg_node(&caller->krnl->mm->mmap->vm_freerg_list, area);
  cur_vma->sbrk += inc_amt;
  return inc_amt;
}

int find_victim_page(struct mm_struct *mm, addr_t *retpgn)
{
  struct pgn_t *pg = mm->fifo_pgn;
  if (!pg) return -1;
  
  struct pgn_t *prev = NULL;
  while (pg->pg_next) {
    prev = pg;
    pg = pg->pg_next;
  }
  
  *retpgn = pg->pgn;
  
  if (prev) {
      prev->pg_next = NULL;
  } else {
      mm->fifo_pgn = NULL;
  }
  free(pg);
  return 0;
}

int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
  uint32_t pte = pte_get_entry(caller, pgn);

  if (!PAGING_PAGE_PRESENT(pte))
  { 
    addr_t vicpgn, vicfpn, swpfpn; 
    uint32_t vicpte;

    if (find_victim_page(caller->krnl->mm, &vicpgn) == -1) {
       return -1; 
    }
    
    vicpte = pte_get_entry(caller, vicpgn);
    vicfpn = PAGING_FPN(vicpte);

    if (MEMPHY_get_freefp(caller->krnl->active_mswp, &swpfpn) == -1) {
      return -1; 
    }

    __swap_cp_page(caller->krnl->mram, vicfpn, caller->krnl->active_mswp, swpfpn);

    pte_set_swap(caller, vicpgn, 0, swpfpn); 

    if (PAGING_PTE_GET_SWAPPED(pte)) {
        addr_t tgt_swpfpn = PAGING_SWP(pte);
        __swap_cp_page(caller->krnl->active_mswp, tgt_swpfpn, caller->krnl->mram, vicfpn);
        MEMPHY_put_freefp(caller->krnl->active_mswp, tgt_swpfpn);
    } 
    
    pte_set_fpn(caller, pgn, vicfpn);

    enlist_pgn_node(&caller->krnl->mm->fifo_pgn, pgn);
  }

  *fpn = PAGING_FPN(pte_get_entry(caller, pgn));
  return 0;
}

int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  if (pg_getpage(mm, pgn, &fpn, caller) != 0) return -1;

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
  MEMPHY_read(caller->krnl->mram, phyaddr, data);
  return 0;
}

int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  if (pg_getpage(mm, pgn, &fpn, caller) != 0) return -1;

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
  MEMPHY_write(caller->krnl->mram, phyaddr, value);
  return 0;
}
