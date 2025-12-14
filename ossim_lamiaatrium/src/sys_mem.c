/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "syscall.h" 
#include "os-mm.h"
#include "libmem.h"
#include "queue.h"
#include <stdlib.h>
#include <stdio.h>
#include "sched.h"

#ifdef MM64
#include "mm64.h"
#else
#include "mm.h"
#endif

//typedef char BYTE;

int __sys_memmap(struct krnl_t *krnl, uint32_t pid, struct sc_regs* regs)
{
   int memop = regs->a1;
   int ret = 0; 
   BYTE value;
   
   struct pcb_t *caller = find_process_by_pid(krnl, pid);
   
   /* Safety check */
   if (caller == NULL) {
       printf("[ERROR] __sys_memmap: Process PID %d not found in kernel\n", pid);
       return -1;
   }

   /*
    * @bksysnet: Please note in the dual spacing design
    *            syscall implementations are in kernel space.
    */
   
   switch (memop) {
   case SYSMEM_MAP_OP:
            ret = vmap_pgd_memset(caller, regs->a2, regs->a3);
            break;
            
   case SYSMEM_INC_OP:
            ret = inc_vma_limit(caller, regs->a2, regs->a3);
            break;
            
   case SYSMEM_SWP_OP:
            ret = __mm_swap_page(caller, regs->a2, regs->a3);
            break;
            
   case SYSMEM_IO_READ:
            ret = MEMPHY_read(caller->krnl->mram, regs->a2, &value);
            if (ret == 0) {
                regs->a3 = (unsigned int)value; 
            }
            break;
            
   case SYSMEM_IO_WRITE:
            ret = MEMPHY_write(caller->krnl->mram, regs->a2, (BYTE)regs->a3);
            break;
            
   default:
            ret = -1;
   }
   
   return ret; 
}