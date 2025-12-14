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
#include "mm64.h"

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  struct vm_area_struct *pvma = mm->mmap;

  if (mm->mmap == NULL)
    return NULL;

  int vmait = pvma->vm_id;

  while (vmait < vmaid)
  {
    if (pvma == NULL)
      return NULL;

    pvma = pvma->vm_next;
    if (pvma == NULL)
      return NULL;
    vmait = pvma->vm_id;
  }

  return pvma;
}

int __mm_swap_page(struct pcb_t *caller, addr_t vicfpn , addr_t swpfpn)
{
    __swap_cp_page(caller->krnl->mram, vicfpn, caller->krnl->active_mswp, swpfpn);
    return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, addr_t size, addr_t alignedsz)
{
  struct vm_rg_struct *newrg;
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (cur_vma == NULL)
    return NULL;

  newrg = malloc(sizeof(struct vm_rg_struct));
  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end = newrg->rg_start + size;
  newrg->rg_next = NULL;

  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, addr_t vmastart, addr_t vmaend)
{
  if (vmastart >= vmaend)
  {
    return -1;
  }

  struct vm_area_struct *vma = caller->mm->mmap;
  if (vma == NULL)
  {
    return -1;
  }

  struct vm_area_struct *cur_area = get_vma_by_num(caller->mm, vmaid);
  if (cur_area == NULL)
  {
    return -1;
  }

  while (vma != NULL)
  {
    if (vma != cur_area && OVERLAP(cur_area->vm_start, cur_area->vm_end, vma->vm_start, vma->vm_end))
    {
      return -1;
    }
    vma = vma->vm_next;
  }

  return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, addr_t inc_sz)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  
  if (cur_vma == NULL) {
    // printf("[ERROR] inc_vma_limit: VMA %d not found\n", vmaid);
    return -1;
  }

  addr_t inc_amt;
  int incnumpage;
  
#ifdef MM64
  inc_amt = PAGING64_PAGE_ALIGNSZ(inc_sz);
  incnumpage = inc_amt / PAGING64_PAGESZ;
  if (incnumpage == 0) incnumpage = 1;
#else
  inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
  incnumpage = inc_amt / PAGING_PAGESZ;
  if (incnumpage == 0) incnumpage = 1;
#endif

  addr_t old_sbrk = cur_vma->sbrk;
  addr_t old_end = cur_vma->vm_end;
  addr_t new_end = old_sbrk + inc_amt;

  /* Update VMA boundaries */
  cur_vma->vm_end = new_end;
  cur_vma->sbrk = new_end;

  if (validate_overlap_vm_area(caller, vmaid, cur_vma->vm_start, cur_vma->vm_end) < 0) {
    // printf("[ERROR] inc_vma_limit: Overlap detected\n");
    cur_vma->vm_end = old_end;
    cur_vma->sbrk = old_sbrk;
    return -1;
  }

  struct vm_rg_struct newrg;
  newrg.rg_start = old_sbrk;
  newrg.rg_end = new_end;
  newrg.rg_next = NULL;

  /* Map the new region to physical RAM */
  if (vm_map_ram(caller, newrg.rg_start, newrg.rg_end, 
                 old_sbrk, incnumpage, &newrg) < 0)
  {
    // printf("[ERROR] inc_vma_limit: vm_map_ram failed (OOM or Table Full)\n");
    cur_vma->vm_end = old_end;
    cur_vma->sbrk = old_sbrk;
    return -1;
  }

  return 0;
}

// #endif