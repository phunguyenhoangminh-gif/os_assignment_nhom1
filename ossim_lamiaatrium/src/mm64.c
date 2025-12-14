/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

#include "mm64.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#if defined(MM64)

/*
 * init_pte - Initialize PTE entry
 */
int init_pte(addr_t *pte,
             int pre,    // present
             addr_t fpn,    // FPN
             int drt,    // dirty
             int swp,    // swap
             int swptyp, // swap type
             addr_t swpoff) // swap offset
{
  if (pre != 0) {
    if (swp == 0) { // Non swap ~ page online
      if (fpn == 0)
        return -1;  // Invalid setting

      /* Valid setting with FPN */
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
    }
    else
    { // page swapped
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    }
  }

  return 0;
}


/*
 * get_pd_from_pagenum - Parse address to 5 page directory level
 * @pgn   : pagenumer
 * @pgd   : page global directory
 * @p4d   : page level directory
 * @pud   : page upper directory
 * @pmd   : page middle directory
 * @pt    : page table 
 */
int get_pd_from_address(addr_t addr, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt)
{
	/* Extract page direactories */
	*pgd = (addr&PAGING64_ADDR_PGD_MASK)>>PAGING64_ADDR_PGD_LOBIT;
	*p4d = (addr&PAGING64_ADDR_P4D_MASK)>>PAGING64_ADDR_P4D_LOBIT;
	*pud = (addr&PAGING64_ADDR_PUD_MASK)>>PAGING64_ADDR_PUD_LOBIT;
	*pmd = (addr&PAGING64_ADDR_PMD_MASK)>>PAGING64_ADDR_PMD_LOBIT;
	*pt = (addr&PAGING64_ADDR_PT_MASK)>>PAGING64_ADDR_PT_LOBIT;

	/* Validate page directory indices are within valid range (0-511 for 512-entry tables) */
	if (*pgd >= 512 || *p4d >= 512 || *pud >= 512 || *pmd >= 512 || *pt >= 512) {
		return -1;  /* Invalid page directory index */
	}

	return 0;
}

/*
 * get_pd_from_pagenum - Parse page number to 5 page directory level
 * @pgn   : pagenumer
 * @pgd   : page global directory
 * @p4d   : page level directory
 * @pud   : page upper directory
 * @pmd   : page middle directory
 * @pt    : page table 
 */
int get_pd_from_pagenum(addr_t pgn, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt)
{
	/* Shift the address to get page num and perform the mapping*/
	return get_pd_from_address(pgn << PAGING64_ADDR_PT_SHIFT,
                         pgd,p4d,pud,pmd,pt);
}


/*
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
int pte_set_swap(struct pcb_t *caller, addr_t pgn, int swptyp, addr_t swpoff)
{
  struct mm_struct *mm = caller->mm;
  addr_t pgd_idx, p4d_idx, pud_idx, pmd_idx, pt_idx;
  int ret;
  
  ret = get_pd_from_pagenum(pgn, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);
  if (ret != 0) {
    return ret;  /* Invalid page directory index */
  }
  
  if (mm->pt == NULL) {
    mm->pt = (uint64_t*)calloc(512, sizeof(uint64_t));
  }
  
  addr_t *pte = &mm->pt[pt_idx];
	
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
  SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
  SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

  return 0;
}

/*
 * pte_set_fpn - Set PTE entry for on-line page
 * @pte   : target page table entry (PTE)
 * @fpn   : frame page number (FPN)
 */
int pte_set_fpn(struct pcb_t *caller, addr_t pgn, addr_t fpn)
{
  struct mm_struct *mm = caller->mm;
  addr_t pgd_idx, p4d_idx, pud_idx, pmd_idx, pt_idx;
  int ret;
  
  ret = get_pd_from_pagenum(pgn, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);
  if (ret != 0) {
    return ret;  /* Invalid page directory index */
  }
  
  if (mm->pt == NULL) {
    mm->pt = (uint64_t*)calloc(512, sizeof(uint64_t));
  }
  if (mm->pgd) mm->pgd[pgd_idx] |= PAGING_PTE_PRESENT_MASK;
  if (mm->p4d) mm->p4d[p4d_idx] |= PAGING_PTE_PRESENT_MASK;
  if (mm->pud) mm->pud[pud_idx] |= PAGING_PTE_PRESENT_MASK;
  if (mm->pmd) mm->pmd[pmd_idx] |= PAGING_PTE_PRESENT_MASK;
  addr_t *pte = &mm->pt[pt_idx];

  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
  SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

  return 0;
}


/* Get PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
uint32_t pte_get_entry(struct pcb_t *caller, addr_t pgn)
{
  struct mm_struct *mm = caller->mm;
  addr_t pgd_idx, p4d_idx, pud_idx, pmd_idx, pt_idx;
  int ret;
  
  ret = get_pd_from_pagenum(pgn, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);
  if (ret != 0) {
    return 0;  /* Invalid page directory index - return 0 */
  }
  
  if (mm->pt == NULL) {
    return 0;
  }
  
  return (uint32_t)mm->pt[pt_idx];
}

/* Set PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
int pte_set_entry(struct pcb_t *caller, addr_t pgn, uint32_t pte_val)
{
	struct mm_struct *mm = caller->mm;
	addr_t pgd_idx, p4d_idx, pud_idx, pmd_idx, pt_idx;
	int ret;
	
	ret = get_pd_from_pagenum(pgn, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);
	if (ret != 0) {
		return ret;  /* Invalid page directory index */
	}
	
	if (mm->pt == NULL) {
		mm->pt = (uint64_t*)calloc(512, sizeof(uint64_t));
	}
	
	mm->pt[pt_idx] = pte_val;
	
	return 0;
}


/*
 * vmap_pgd_memset - map a range of page at aligned address
 */
int vmap_pgd_memset(struct pcb_t *caller,           // process call
                    addr_t addr,                       // start address which is aligned to pagesz
                    int pgnum)                      // num of mapping page
{
  struct mm_struct *mm = caller->mm;
  int ret;
  
  /* Allocate page directories if not exist */
  if (mm->pgd == NULL) {
    mm->pgd = (uint64_t*)calloc(512, sizeof(uint64_t));
  }
  if (mm->p4d == NULL) {
    mm->p4d = (uint64_t*)calloc(512, sizeof(uint64_t));
  }
  if (mm->pud == NULL) {
    mm->pud = (uint64_t*)calloc(512, sizeof(uint64_t));
  }
  if (mm->pmd == NULL) {
    mm->pmd = (uint64_t*)calloc(512, sizeof(uint64_t));
  }
  if (mm->pt == NULL) {
    mm->pt = (uint64_t*)calloc(512, sizeof(uint64_t));
  }
  
  /* Memset page table entries with pattern */
  for (int pgit = 0; pgit < pgnum; pgit++)
  {
    addr_t current_addr = addr + (pgit * PAGING64_PAGESZ);
    addr_t pgd_idx, p4d_idx, pud_idx, pmd_idx, pt_idx;
    
    ret = get_pd_from_address(current_addr, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);
    if (ret != 0) {
      return ret;  /* Invalid page directory index */
    }
    
    /* Mark entries as present with dummy pattern */
    mm->pgd[pgd_idx] = 0xdeadbeef | PAGING_PTE_PRESENT_MASK;
    mm->p4d[p4d_idx] = 0xdeadbeef | PAGING_PTE_PRESENT_MASK;
    mm->pud[pud_idx] = 0xdeadbeef | PAGING_PTE_PRESENT_MASK;
    mm->pmd[pmd_idx] = 0xdeadbeef | PAGING_PTE_PRESENT_MASK;
    mm->pt[pt_idx]   = 0xdeadbeef | PAGING_PTE_PRESENT_MASK;
  }

  return 0;
}

/*
 * vmap_page_range - map a range of page at aligned address
 */
addr_t vmap_page_range(struct pcb_t *caller,           // process call
                    addr_t addr,                       // start address which is aligned to pagesz
                    int pgnum,                      // num of mapping page
                    struct framephy_struct *frames, // list of the mapped frames
                    struct vm_rg_struct *ret_rg)    // return mapped region, the real mapped fp
{
  struct mm_struct *mm = caller->mm;
  struct framephy_struct *fpit = frames;
  int pgit = 0;
  int ret;
  
  /* Update return region boundaries */
  if (ret_rg != NULL) {
    ret_rg->rg_start = addr;
    ret_rg->rg_end = addr + (pgnum * PAGING64_PAGESZ);
  }
  
  /* Ensure page directories exist */
  if (mm->pgd == NULL) mm->pgd = (uint64_t*)calloc(512, sizeof(uint64_t));
  if (mm->p4d == NULL) mm->p4d = (uint64_t*)calloc(512, sizeof(uint64_t));
  if (mm->pud == NULL) mm->pud = (uint64_t*)calloc(512, sizeof(uint64_t));
  if (mm->pmd == NULL) mm->pmd = (uint64_t*)calloc(512, sizeof(uint64_t));
  if (mm->pt == NULL)  mm->pt  = (uint64_t*)calloc(512, sizeof(uint64_t));
  
  /* Map range of frames to address space */
  for (pgit = 0; pgit < pgnum; pgit++)
  {
    addr_t current_addr = addr + (pgit * PAGING64_PAGESZ);
    addr_t pgn = current_addr >> PAGING64_ADDR_PT_SHIFT;
    addr_t fpn = (fpit != NULL) ? fpit->fpn : pgit;
    
    /* Set page table entry */
    ret = pte_set_fpn(caller, pgn, fpn);
    if (ret != 0) {
      return ret;  /* Error setting page table entry */
    }
    
    /* Tracking for page replacement (FIFO) */
    enlist_pgn_node(&mm->fifo_pgn, pgn);
    
    if (fpit != NULL)
      fpit = fpit->fp_next;
  }
  
  return 0;
}

/*
 * alloc_pages_range - allocate req_pgnum of frame in ram
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 */

addr_t alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct **frm_lst)
{
  addr_t fpn;
  int pgit;
  struct framephy_struct *newfp_str = NULL;
  struct framephy_struct *prev_fp = NULL;
  
  for (pgit = 0; pgit < req_pgnum; pgit++)
  {
    if (MEMPHY_get_freefp(caller->krnl->mram, &fpn) == 0)
    {
      newfp_str = malloc(sizeof(struct framephy_struct));
      newfp_str->fpn = fpn;
      newfp_str->fp_next = NULL;
      
      if (*frm_lst == NULL) {
        *frm_lst = newfp_str;
      } else {
        prev_fp->fp_next = newfp_str;
      }
      prev_fp = newfp_str;
    }
    else
    {
      /* Not enough frames - return error code */
      return -3000;
    }
  }
  
  return 0;
}

/*
 * vm_map_ram - do the mapping all vm are to ram storage device
 * @caller    : caller
 * @astart    : vm area start
 * @aend      : vm area end
 * @mapstart  : start mapping point
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 */
addr_t vm_map_ram(struct pcb_t *caller, addr_t astart, addr_t aend, addr_t mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{
  struct framephy_struct *frm_lst = NULL;
  addr_t ret_alloc = 0;
  
  /*@bksysnet: author provides a feasible solution of getting frames
   *FATAL logic in here, wrong behaviour if we have not enough page
   *i.e. we request 1000 frames meanwhile our RAM has size of 3 frames
   *Don't try to perform that case in this simple work, it will result
   *in endless procedure of swap-off to get frame and we have not provide
   *duplicate control mechanism, keep it simple
   */
  
  /* Allocate physical frames */
  ret_alloc = alloc_pages_range(caller, incpgnum, &frm_lst);
  
  if (ret_alloc < 0 && ret_alloc != -3000)
    return -1;
  
  /* Out of memory */
  if (ret_alloc == -3000)
  {
    return -1;
  }
  
  /* Map pages to address space */
  vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);
  
  return 0;
}

/* Swap copy content page from source frame to destination frame
 * @mpsrc  : source memphy
 * @srcfpn : source physical page number (FPN)
 * @mpdst  : destination memphy
 * @dstfpn : destination physical page number (FPN)
 **/
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

/*
 *Initialize a empty Memory Management instance
 * @mm:     self mm
 * @caller: mm owner
 */
int init_mm(struct mm_struct *mm, struct pcb_t *caller)
{
  struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));
  
  /* Initialize 5-level page table directories */
  mm->pgd = (uint64_t*)calloc(512, sizeof(uint64_t));
  mm->p4d = (uint64_t*)calloc(512, sizeof(uint64_t));
  mm->pud = (uint64_t*)calloc(512, sizeof(uint64_t));
  mm->pmd = (uint64_t*)calloc(512, sizeof(uint64_t));
  mm->pt  = (uint64_t*)calloc(512, sizeof(uint64_t));

  /* By default the owner comes with at least one vma */
  vma0->vm_id = 0;
  vma0->vm_start = 0;
  vma0->vm_end = vma0->vm_start;
  vma0->sbrk = vma0->vm_start;
  
  struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end);
  enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);

  /* Update VMA0 next */
  vma0->vm_next = NULL;
  
  /* Point vma owner backward */
  vma0->vm_mm = mm; 

  /* Update mmap */
  mm->mmap = vma0;
  
  /* Initialize symbol table */
  for (int i = 0; i < PAGING_MAX_SYMTBL_SZ; i++) {
    mm->symrgtbl[i].rg_start = 0;
    mm->symrgtbl[i].rg_end = 0;
    mm->symrgtbl[i].rg_next = NULL;
  }
  
  mm->fifo_pgn = NULL;

  return 0;
}

struct vm_rg_struct *init_vm_rg(addr_t rg_start, addr_t rg_end)
{
  struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));

  rgnode->rg_start = rg_start;
  rgnode->rg_end = rg_end;
  rgnode->rg_next = NULL;

  return rgnode;
}

int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct *rgnode)
{
  rgnode->rg_next = *rglist;
  *rglist = rgnode;

  return 0;
}

int enlist_pgn_node(struct pgn_t **plist, addr_t pgn)
{
  struct pgn_t *pnode = malloc(sizeof(struct pgn_t));

  pnode->pgn = pgn;
  pnode->pg_next = *plist;
  *plist = pnode;

  return 0;
}

int print_list_fp(struct framephy_struct *ifp)
{
  struct framephy_struct *fp = ifp;

  printf("print_list_fp: ");
  if (fp == NULL) { printf("NULL list\n"); return -1;}
  printf("\n");
  while (fp != NULL)
  {
    printf("fp[" FORMAT_ADDR "]\n", fp->fpn);
    fp = fp->fp_next;
  }
  printf("\n");
  return 0;
}

int print_list_rg(struct vm_rg_struct *irg)
{
  struct vm_rg_struct *rg = irg;

  printf("print_list_rg: ");
  if (rg == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (rg != NULL)
  {
    printf("rg[" FORMAT_ADDR "->"  FORMAT_ADDR "]\n", rg->rg_start, rg->rg_end);
    rg = rg->rg_next;
  }
  printf("\n");
  return 0;
}

int print_list_vma(struct vm_area_struct *ivma)
{
  struct vm_area_struct *vma = ivma;

  printf("print_list_vma: ");
  if (vma == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (vma != NULL)
  {
    printf("va[" FORMAT_ADDR "->" FORMAT_ADDR "]\n", vma->vm_start, vma->vm_end);
    vma = vma->vm_next;
  }
  printf("\n");
  return 0;
}

int print_list_pgn(struct pgn_t *ip)
{
  printf("print_list_pgn: ");
  if (ip == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (ip != NULL)
  {
    printf("va[" FORMAT_ADDR "]-\n", ip->pgn);
    ip = ip->pg_next;
  }
  printf("n");
  return 0;
}

int print_pgtbl(struct pcb_t *caller, addr_t start, addr_t end)
{
  struct mm_struct *mm = caller->mm;
  
  /* Expected output format shows memory addresses of page directories
   * NOT the actual PTE values
   * Format: "PDG=<addr> P4g=<addr> PUD=<addr> PMD=<addr>"
   */
  
  printf("print_pgtbl:\n PDG=%p P4g=%p PUD=%p PMD=%p\n",
         (void*)mm->pgd,
         (void*)mm->p4d,
         (void*)mm->pud,
         (void*)mm->pmd);
  fflush(stdout);
  return 0;
}

#endif  //def MM64
