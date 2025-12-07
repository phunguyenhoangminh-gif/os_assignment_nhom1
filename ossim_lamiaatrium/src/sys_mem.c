/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "os-mm.h"
#include "syscall.h"
#include "libmem.h"
#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef MM64
#include "mm64.h"
#else
#include "mm.h"
#endif

int __sys_memmap(struct krnl_t *krnl, uint32_t pid, struct sc_regs* regs)
{
   int memop = regs->a1;
   BYTE value;
   struct pcb_t *caller = NULL;
   struct queue_t *q = krnl->running_list;
   
   for(int i = 0; i < q->size; i++) {
       if (q->proc[i]->pid == pid) {
           caller = q->proc[i];
           break;
       }
   }

   if (caller == NULL) {
       return -1; 
   }
	
   switch (memop) {
   case SYSMEM_MAP_OP:
            vmap_pgd_memset(caller, regs->a2, regs->a3);
            break;
   case SYSMEM_INC_OP:
            inc_vma_limit(caller, regs->a2, regs->a3);
            break;
   case SYSMEM_SWP_OP:
            __mm_swap_page(caller, regs->a2, regs->a3);
            break;
   case SYSMEM_IO_READ:
            if (pg_getval(caller->krnl->mm, regs->a2, &value, caller) == 0) {
                regs->a3 = (int)value; 
            } else {
                return -1;
            }
            break;
   case SYSMEM_IO_WRITE:
            pg_setval(caller->krnl->mm, regs->a2, (BYTE)regs->a3, caller);
            break;
   default:
            printf("Memop code: %d\n", memop);
            break;
   }
   
   return 0;
}


