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
      /* Traverse sequentially */
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
   if (mp == NULL)
      return -1;

   if (!mp->rdmflg)
      return -1; /* Not compatible mode for sequential read */

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
   if (mp == NULL)
      return -1;

   if (mp->rdmflg)
      *value = mp->storage[addr];
   else /* Sequential access device */
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

   if (mp == NULL)
      return -1;

   if (!mp->rdmflg)
      return -1; /* Not compatible mode for sequential read */

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
   if (mp == NULL)
      return -1;

   if (mp->rdmflg)
      mp->storage[addr] = data;
   else /* Sequential access device */
      return MEMPHY_seq_write(mp, addr, data);

   return 0;
}

/*
 *  MEMPHY_format-format MEMPHY device
 *  @mp: memphy struct
 */
int MEMPHY_format(struct memphy_struct *mp, int pagesz)
{
   /* This setting come with fixed constant PAGESZ */
   int numfp = mp->maxsz / pagesz;
   struct framephy_struct *newfst, *fst;
   int iter = 0;

   if (numfp <= 0)
      return -1;

   /* Init head of free framephy list */
   fst = malloc(sizeof(struct framephy_struct));
   if (fst == NULL)
      return -1; /* allocation failed */

   /* First frame (fpn = 0) */
   fst->fpn = iter;
   fst->fp_next = NULL;
   fst->owner = NULL;
   mp->free_fp_list = fst;
   mp->used_fp_list = NULL; /* no used frames at start */

   /* We have list with first element, fill in the rest num-1 element member*/
   for (iter = 1; iter < numfp; iter++)
   {
      newfst = malloc(sizeof(struct framephy_struct));
      if (newfst == NULL)
         return -1; /* allocation failed */

      newfst->fpn = iter;
      newfst->fp_next = NULL;
      newfst->owner = NULL;
      fst->fp_next = newfst;
      fst = newfst;
   }

   return 0;
}

int MEMPHY_get_freefp(struct memphy_struct *mp, addr_t *retfpn)
{
   struct framephy_struct *fp = mp->free_fp_list;

   /* No free frame available -> trigger swap/out of memory */
   if (fp == NULL)
      return -1;

   *retfpn = fp->fpn;
   mp->free_fp_list = fp->fp_next;

   // if (mp->used_fp_list == NULL)
   // mp->used_fp_list = fp; 
   // else
   // {
   //    struct framephy_struct *fp1 = mp->used_fp_list;
   //    while (fp1->fp_next != NULL)
   //    fp1 = fp1->fp_next;
   //    fp1->fp_next = fp;
   // }

   /* MEMPHY is iteratively used up until its exhausted
    * No garbage collector acting then it not been released
    */
   free(fp);

   return 0;
}

int MEMPHY_dump(struct memphy_struct *mp)
{
  /*TODO dump memphy contnt mp->storage
   *     for tracing the memory content
   */
   if( mp == NULL || mp->maxsz <=0 || mp->storage == NULL) return 0;
   printf("===== PHYSICAL MEMORY DUMP =====\n");
   for (int i = 0; i < mp->maxsz; ++i)
   {
      if (mp->storage[i] != 0)
      {
         printf("BYTE %08x: %d\n", i, mp->storage[i]);
      }
   }
   printf("===== PHYSICAL MEMORY END-DUMP =====\n");
   printf("================================================================\n");
   return 0;
}

int MEMPHY_put_freefp(struct memphy_struct *mp, addr_t fpn)
{
   struct framephy_struct *fp = mp->free_fp_list;
   struct framephy_struct *newnode = malloc(sizeof(struct framephy_struct));

   /* Create new node with value fpn */
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
   if (mp == NULL)
      return -1;

   /* Allocate backing storage for memory device */
   mp->storage = (BYTE *)malloc(max_size * sizeof(BYTE));
   if (mp->storage == NULL)
      return -1; /* allocation failed */

   mp->maxsz = max_size;
   /* Zero the storage so memory starts cleared */
   memset(mp->storage, 0, max_size * sizeof(BYTE));

   /* Initialize frame lists based on page/frame size */
   if (MEMPHY_format(mp, PAGING_PAGESZ) != 0)
      return -1;

   /* Random (rdmflg) determines whether device supports random access */
   mp->rdmflg = (randomflg != 0) ? 1 : 0;

   if (!mp->rdmflg) /* sequential device: initialize cursor */
      mp->cursor = 0;

   return 0;
}

/*
 * MEMPHY_create - allocate and initialize a new memphy device
 * @ret: pointer to receive allocated memphy_struct*
 * @max_size: size in bytes of the backing storage
 * @randomflg: non-zero if random access device
 *
 * Returns 0 on success, -1 on failure. Caller must free the memphy and
 * its storage when no longer needed.
 */
int MEMPHY_create(struct memphy_struct **ret, addr_t max_size, int randomflg)
{
   struct memphy_struct *mp;

   if (ret == NULL)
      return -1;

   mp = malloc(sizeof(struct memphy_struct));
   if (mp == NULL)
      return -1;

   /* Initialize fields to safe defaults */
   mp->storage = NULL;
   mp->maxsz = 0;
   mp->rdmflg = 0;
   mp->cursor = 0;
   mp->free_fp_list = NULL;
   mp->used_fp_list = NULL;

   if (init_memphy(mp, max_size, randomflg) != 0)
   {
      if (mp->storage)
         free(mp->storage);
      free(mp);
      return -1;
   }

   *ret = mp;
   return 0;
}

   /*
    * MEMPHY_destroy - release memphy internal resources
    * @mp: memphy instance whose internal resources will be freed
    *
    * This function frees the backing storage buffer and all nodes in both
    * `free_fp_list` and `used_fp_list`. It does NOT free the `mp` pointer
    * itself (caller should free it if it was dynamically allocated by
    * `MEMPHY_create`). This avoids accidental free of stack-allocated structs.
    */
   int MEMPHY_destroy(struct memphy_struct *mp)
   {
      struct framephy_struct *cur, *tmp;

      if (mp == NULL)
         return -1;

      /* Free backing storage */
      if (mp->storage) {
         free(mp->storage);
         mp->storage = NULL;
      }

      /* Free all nodes in free_fp_list */
      cur = mp->free_fp_list;
      while (cur) {
         tmp = cur->fp_next;
         free(cur);
         cur = tmp;
      }
      mp->free_fp_list = NULL;

      /* Free all nodes in used_fp_list */
      cur = mp->used_fp_list;
      while (cur) {
         tmp = cur->fp_next;
         free(cur);
         cur = tmp;
      }
      mp->used_fp_list = NULL;

      /* Reset other fields */
      mp->maxsz = 0;
      mp->rdmflg = 0;
      mp->cursor = 0;

      return 0;
   }

// #endif
