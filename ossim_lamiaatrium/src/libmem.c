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

/* enlist_vm_freerg_list - add new rg to freerg_list */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1;

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = rg_elmt;

  return 0;
}

/* get_symrg_byid - get mem region by region ID */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/* __alloc - allocate a region memory */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr)
{
  /*Allocate at the toproof */
  pthread_mutex_lock(&mmvm_lock);
  struct vm_rg_struct rgnode;
  // struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid); // Removed direct access
  

  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    caller->krnl->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->krnl->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
 
    *alloc_addr = rgnode.rg_start;

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }

  /* Attempt to increase limit to get space */
  struct sc_regs regs;
  regs.a1 = SYSMEM_INC_OP;
  regs.a2 = vmaid;
#ifdef MM64
  regs.a3 = size;
#else
  regs.a3 = PAGING_PAGE_ALIGNSZ(size);
#endif  
  
  /* Gọi System Call để tăng kích thước Heap */
  syscall(caller->krnl, caller->pid, 17, &regs); 

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
  int old_sbrk = cur_vma->sbrk - regs.a3; // Giả sử INC đã thành công và sbrk đã tăng

  caller->krnl->mm->symrgtbl[rgid].rg_start = old_sbrk;
  caller->krnl->mm->symrgtbl[rgid].rg_end = old_sbrk + size;

  *alloc_addr = old_sbrk;

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/* __free - remove a region memory */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  pthread_mutex_lock(&mmvm_lock);

  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  struct vm_rg_struct *rgnode = get_symrg_byid(caller->krnl->mm, rgid);

  if (rgnode->rg_start == 0 && rgnode->rg_end == 0)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  struct vm_rg_struct *freerg_node = malloc(sizeof(struct vm_rg_struct));
  freerg_node->rg_start = rgnode->rg_start;
  freerg_node->rg_end = rgnode->rg_end;
  freerg_node->rg_next = NULL;

  rgnode->rg_start = rgnode->rg_end = 0;
  rgnode->rg_next = NULL;

  enlist_vm_freerg_list(caller->krnl->mm, freerg_node);

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/* liballoc - PAGING-based allocate a region memory */
int liballoc(struct pcb_t *proc, addr_t size, uint32_t reg_index)
{
  addr_t  addr;
  /* __alloc includes syscall SYSMEM_INC_OP */
  int val = __alloc(proc, 0, reg_index, size, &addr);
  if (val == -1)
  {
    return -1;
  }
#ifdef IODUMP
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); 
#endif
#endif
  return val;
}

/* libfree - PAGING-based free a region memory */
int libfree(struct pcb_t *proc, uint32_t reg_index)
{
  int val = __free(proc, 0, reg_index);
  if (val == -1)
  {
    return -1;
  }
  return 0;
}

/* libread - PAGING-based read a region memory */
int libread(
    struct pcb_t *proc, 
    uint32_t source,    
    uint32_t offset,    
    uint32_t *destination) 
{
  struct sc_regs regs;
  
  addr_t vaddr = proc->regs[source] + offset; 

  regs.a1 = SYSMEM_IO_READ;
  regs.a2 = vaddr;
  regs.a3 = 0; // Placeholder

  /* Gọi System Call 17 (memmap) để đọc bộ nhớ */
  if (syscall(proc->krnl, proc->pid, 17, &regs) == 0) {
      *destination = (uint32_t)regs.a3; // Dữ liệu đọc được kernel trả về qua a3
      return 0;
  }
  return -1;
}

/* libwrite - PAGING-based write a region memory */
int libwrite(
    struct pcb_t *proc,   
    BYTE data,            
    uint32_t destination, 
    uint32_t offset)
{
  struct sc_regs regs;
  
  addr_t vaddr = proc->regs[destination] + offset;

  regs.a1 = SYSMEM_IO_WRITE;
  regs.a2 = vaddr;
  regs.a3 = (int)data; 

  /* Gọi System Call 17 (memmap) để ghi bộ nhớ */
  return syscall(proc->krnl, proc->pid, 17, &regs);
}

/* Các hàm helper cũ không còn dùng trực tiếp từ userspace nữa nhưng giữ lại khung để tránh lỗi link */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller) { return 0; }
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller) { return 0; }
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller) { return 0; }
int __read(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data) { return 0; }
int __write(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value) { return 0; }

/* Giữ lại các hàm tiện ích khác dùng cho __alloc/__free */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;
  if (rgit == NULL) return -1;
  newrg->rg_start = newrg->rg_end = -1;
  while (rgit != NULL)
  {
    if (rgit->rg_start + size <= rgit->rg_end)
    { 
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
    }
    else
    {
      rgit = rgit->rg_next; 
    }
  }
  if (newrg->rg_start == -1) return -1;
  return 0;
}
// #endif
