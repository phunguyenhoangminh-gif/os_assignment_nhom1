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
 * PAGING based Memory Management
 * Memory physical module mm/mm-memphy.c
 */

#include "mm.h"
#include "mm64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 *  MEMPHY_mv_csr - move MEMPHY cursor
 *  @mp: memphy struct
 *  @offset: offset
 */
int MEMPHY_mv_csr(struct memphy_struct *mp, addr_t offset)
{
   int numstep = 0;
   mp->cursor = 0;
   while (numstep < offset && numstep < mp->maxsz)
   {
      mp->cursor = (mp->cursor + 1) % mp->maxsz;
      numstep++;
   }
   return 0;
}

/*
 *  MEMPHY_seq_read - read MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int MEMPHY_seq_read(struct memphy_struct *mp, addr_t addr, BYTE *value)
{
   if (mp == NULL) return -1;
   if (!mp->rdmflg) return -1; 
   MEMPHY_mv_csr(mp, addr);
   *value = (BYTE)mp->storage[addr];
   return 0;
}

/*
 *  MEMPHY_read read MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int MEMPHY_read(struct memphy_struct *mp, addr_t addr, BYTE *value)
{
   if (mp == NULL) return -1;

   if (mp->rdmflg) {
      if (addr >= (addr_t)mp->maxsz) {
         //  printf("[ERROR] MEMPHY_read: Address out of bounds (0x%lx >= 0x%x)\n", (unsigned long)addr, mp->maxsz);
          return -1;
      }
      *value = mp->storage[addr];
   }
   else 
      return MEMPHY_seq_read(mp, addr, value);

   return 0;
}

/*
 *  MEMPHY_seq_write - write MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int MEMPHY_seq_write(struct memphy_struct *mp, addr_t addr, BYTE value)
{
   if (mp == NULL) return -1;
   if (!mp->rdmflg) return -1; 
   MEMPHY_mv_csr(mp, addr);
   mp->storage[addr] = value;
   return 0;
}

/*
 *  MEMPHY_write-write MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int MEMPHY_write(struct memphy_struct *mp, addr_t addr, BYTE data)
{
   if (mp == NULL) return -1;

   if (mp->rdmflg) {
      if (addr >= (addr_t)mp->maxsz) {
         //  printf("[ERROR] MEMPHY_write: Address out of bounds (0x%lx >= 0x%x)\n", (unsigned long)addr, mp->maxsz);
          return -1;
      }
      mp->storage[addr] = data;
   }
   else 
      return MEMPHY_seq_write(mp, addr, data);

   return 0;
}

/*
 *  MEMPHY_format-format MEMPHY device
 *  @mp: memphy struct
 */
int MEMPHY_format(struct memphy_struct *mp, int pagesz)
{
   int numfp = mp->maxsz / pagesz;
   struct framephy_struct *newfst, *fst;
   int iter = 0;

   if (numfp <= 0) return -1;

   fst = malloc(sizeof(struct framephy_struct));
   fst->fpn = iter;
   mp->free_fp_list = fst;

   for (iter = 1; iter < numfp; iter++)
   {
      newfst = malloc(sizeof(struct framephy_struct));
      newfst->fpn = iter;
      newfst->fp_next = NULL;
      fst->fp_next = newfst;
      fst = newfst;
   }

   return 0;
}

int MEMPHY_get_freefp(struct memphy_struct *mp, addr_t *retfpn)
{
   struct framephy_struct *fp = mp->free_fp_list;
   if (fp == NULL) return -1;
   *retfpn = fp->fpn;
   mp->free_fp_list = fp->fp_next;

   /* MEMPHY is iteratively used up until its exhausted
    * No garbage collector acting then it not been released
    */
   free(fp);
   return 0;
}

int MEMPHY_dump(struct memphy_struct *mp)
{
   if (mp == NULL || mp->storage == NULL) return -1;
   // printf("MEMPHY_dump: maxsz=%d\n", mp->maxsz);
   return 0;
}

int MEMPHY_put_freefp(struct memphy_struct *mp, addr_t fpn)
{
   struct framephy_struct *fp = mp->free_fp_list;
   struct framephy_struct *newnode = malloc(sizeof(struct framephy_struct));
   newnode->fpn = fpn;
   newnode->fp_next = fp;
   mp->free_fp_list = newnode;
   return 0;
}

/*
 *  Init MEMPHY struct
 */
int init_memphy(struct memphy_struct *mp, addr_t max_size, int randomflg)
{
   mp->storage = (BYTE *)malloc(max_size * sizeof(BYTE));
   mp->maxsz = max_size;
   mp->free_fp_list = NULL;
   memset(mp->storage, 0, max_size * sizeof(BYTE));

#ifdef MM64
   MEMPHY_format(mp, PAGING64_PAGESZ);
#else
   MEMPHY_format(mp, PAGING_PAGESZ);
#endif

   mp->rdmflg = (randomflg != 0) ? 1 : 0;
   if (!mp->rdmflg) mp->cursor = 0;

   return 0;
}
// #endif