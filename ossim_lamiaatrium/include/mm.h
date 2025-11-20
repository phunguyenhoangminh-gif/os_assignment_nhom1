#ifndef MM_H

#include "common.h"
#include "bitops.h"

/* CPU Bus definition */
#define PAGING_CPU_BUS_WIDTH 22 /* 22bit bus - MAX SPACE 4MB */
#define PAGING_PAGESZ  256      /* 256B or 8-bits PAGE NUMBER */
#define PAGING_MEMRAMSZ BIT(21)
#define PAGING_PAGE_ALIGNSZ(sz) (DIV_ROUND_UP(sz,PAGING_PAGESZ)*PAGING_PAGESZ)

#define PAGING_MEMSWPSZ BIT(29)
#define PAGING_SWPFPN_OFFSET 5  
#define PAGING_MAX_PGN  (DIV_ROUND_UP(BIT(PAGING_CPU_BUS_WIDTH),PAGING_PAGESZ))

#define PAGING_SBRK_INIT_SZ PAGING_PAGESZ
/* When PRESENT = 1 (Page in RAM):
 *   - Bits 0-12: FPN (Frame Physical Number) - Physical frame address
 * 
 * When PRESENT = 0 and SWAPPED = 1 (Page in Swap):
 *   - Bits 0-4:  Swap Type (Device identifier)
 *   - Bits 5-25: Swap Offset (Location in swap file)
 * 
 * When PRESENT = 0 and SWAPPED = 0:
 *   - Page not allocated or not yet loaded
 */

/* Control flags - Bit positions */
#define PAGING_PTE_PRESENT_MASK BIT(31)  /* Bit 31: Page is in RAM */
#define PAGING_PTE_SWAPPED_MASK BIT(30)  /* Bit 30: Page is swapped to disk */
#define PAGING_PTE_RESERVE_MASK BIT(29)  /* Bit 29: Reserved for future use */
#define PAGING_PTE_DIRTY_MASK   BIT(28)  /* Bit 28: Page has been modified */
#define PAGING_PTE_EMPTY01_MASK BIT(14)  /* Bit 14: Reserved/unused */
#define PAGING_PTE_EMPTY02_MASK BIT(13)  /* Bit 13: Reserved/unused */

/* ----------------------------------------------------------------------------
 * BIT 31: PRESENT FLAG
 * Purpose: Indicates if the page data is currently in RAM
 * - Value 1: Page is loaded in RAM, FPN field is valid
 * - Value 0: Page is not in RAM (may be swapped or not allocated)
 * ----------------------------------------------------------------------------
 */
#define PAGING_PTE_SET_PRESENT(pte)  ((pte) = (pte) | PAGING_PTE_PRESENT_MASK)
#define PAGING_PTE_CLR_PRESENT(pte)  ((pte) = (pte) & ~PAGING_PTE_PRESENT_MASK)
#define PAGING_PTE_GET_PRESENT(pte)  ((pte) & PAGING_PTE_PRESENT_MASK)
#define PAGING_PAGE_PRESENT(pte)     PAGING_PTE_GET_PRESENT(pte)

/* ----------------------------------------------------------------------------
 * BIT 30: SWAPPED FLAG
 * Purpose: Indicates if the page has been swapped to disk
 * - Only meaningful when PRESENT = 0
 * - Value 1: Page data is on swap device, swap fields are valid
 * - Value 0: Page is not swapped (may not be allocated yet)
 * ----------------------------------------------------------------------------
 */
#define PAGING_PTE_SET_SWAPPED(pte)  ((pte) = (pte) | PAGING_PTE_SWAPPED_MASK)
#define PAGING_PTE_CLR_SWAPPED(pte)  ((pte) = (pte) & ~PAGING_PTE_SWAPPED_MASK)
#define PAGING_PTE_GET_SWAPPED(pte)  ((pte) & PAGING_PTE_SWAPPED_MASK)

/* ----------------------------------------------------------------------------
 * BITS 0-12: FPN (Frame Physical Number) - 13 bits
 * Purpose: Physical frame address when page is in RAM
 * - Only valid when PRESENT = 1
 * - Range: 0 to 8191 (2^13 - 1) frames
 * - Each frame is PAGING_PAGESZ bytes (256 bytes)
 * ----------------------------------------------------------------------------
 */
#define PAGING_PTE_SET_FPN(pte, fpn) \
    ((pte) = ((pte) & ~PAGING_PTE_FPN_MASK) | (((fpn) << PAGING_PTE_FPN_LOBIT) & PAGING_PTE_FPN_MASK))
#define PAGING_PTE_GET_FPN(pte) \
    (((pte) & PAGING_PTE_FPN_MASK) >> PAGING_PTE_FPN_LOBIT)
#define PAGING_PTE_FPN(pte) PAGING_PTE_GET_FPN(pte)

/* ----------------------------------------------------------------------------
 * BITS 0-4: SWAP TYPE - 5 bits
 * Purpose: Identifies which swap device holds the page
 * - Only valid when PRESENT = 0 and SWAPPED = 1
 * - Range: 0 to 31 (2^5 - 1) different swap devices
 * ----------------------------------------------------------------------------
 */
#define PAGING_PTE_SET_SWPTYP(pte, swptyp) \
    ((pte) = ((pte) & ~PAGING_PTE_SWPTYP_MASK) | (((swptyp) << PAGING_PTE_SWPTYP_LOBIT) & PAGING_PTE_SWPTYP_MASK))
#define PAGING_PTE_GET_SWPTYP(pte) \
    (((pte) & PAGING_PTE_SWPTYP_MASK) >> PAGING_PTE_SWPTYP_LOBIT)

/* ----------------------------------------------------------------------------
 * BITS 5-25: SWAP OFFSET - 21 bits
 * Purpose: Location of page within the swap device
 * - Only valid when PRESENT = 0 and SWAPPED = 1
 * - Points to the specific location in swap file/device
 * - Range: 0 to 2,097,151 (2^21 - 1) possible locations
 * ----------------------------------------------------------------------------
 */
#define PAGING_PTE_SET_SWPOFF(pte, swpoff) \
    ((pte) = ((pte) & ~PAGING_PTE_SWPOFF_MASK) | (((swpoff) << PAGING_PTE_SWPOFF_LOBIT) & PAGING_PTE_SWPOFF_MASK))
#define PAGING_PTE_GET_SWPOFF(pte) \
    (((pte) & PAGING_PTE_SWPOFF_MASK) >> PAGING_PTE_SWPOFF_LOBIT)
#define PAGING_PTE_SWP(pte) PAGING_PTE_GET_SWPOFF(pte)

/* ----------------------------------------------------------------------------
 * HELPER MACROS - Compound operations
 * These macros perform multiple operations at once for convenience
 * ----------------------------------------------------------------------------
 */

/* Set both swap type and offset in one operation */
#define PAGING_PTE_SET_SWAP_INFO(pte, swptyp, swpoff) \
    do { \
        PAGING_PTE_SET_SWPTYP(pte, swptyp); \
        PAGING_PTE_SET_SWPOFF(pte, swpoff); \
    } while(0)

/* Initialize PTE for a page that is present in RAM */
#define PAGING_PTE_INIT_PRESENT(pte, fpn) \
    do { \
        (pte) = 0; \
        PAGING_PTE_SET_PRESENT(pte); \
        PAGING_PTE_SET_FPN(pte, fpn); \
    } while(0)

/* Initialize PTE for a page that has been swapped out */
#define PAGING_PTE_INIT_SWAPPED(pte, swptyp, swpoff) \
    do { \
        (pte) = 0; \
        PAGING_PTE_SET_SWAPPED(pte); \
        PAGING_PTE_SET_SWAP_INFO(pte, swptyp, swpoff); \
    } while(0)
/* ============================================================================ */

/* USRNUM */
#define PAGING_PTE_USRNUM_LOBIT 15
#define PAGING_PTE_USRNUM_HIBIT 27
/* FPN */
#define PAGING_PTE_FPN_LOBIT 0
#define PAGING_PTE_FPN_HIBIT 12
/* SWPTYP */
#define PAGING_PTE_SWPTYP_LOBIT 0
#define PAGING_PTE_SWPTYP_HIBIT 4
/* SWPOFF */
#define PAGING_PTE_SWPOFF_LOBIT 5
#define PAGING_PTE_SWPOFF_HIBIT 25

/* PTE */
#define PAGING_PTE_USRNUM_MASK GENMASK(PAGING_PTE_USRNUM_HIBIT,PAGING_PTE_USRNUM_LOBIT)
#define PAGING_PTE_FPN_MASK    GENMASK(PAGING_PTE_FPN_HIBIT,PAGING_PTE_FPN_LOBIT)
#define PAGING_PTE_SWPTYP_MASK GENMASK(PAGING_PTE_SWPTYP_HIBIT,PAGING_PTE_SWPTYP_LOBIT)
#define PAGING_PTE_SWPOFF_MASK GENMASK(PAGING_PTE_SWPOFF_HIBIT,PAGING_PTE_SWPOFF_LOBIT)

/* Extract PTE */
#define PAGING_PTE_OFFST(pte) GETVAL(pte,PAGING_OFFST_MASK,PAGING_ADDR_OFFST_LOBIT)
#define PAGING_PTE_PGN(pte)   GETVAL(pte,PAGING_PGN_MASK,PAGING_ADDR_PGN_LOBIT)
#define PAGING_PTE_FPN(pte)   GETVAL(pte,PAGING_PTE_FPN_MASK,PAGING_PTE_FPN_LOBIT)
#define PAGING_PTE_SWP(pte)   GETVAL(pte,PAGING_PTE_SWPOFF_MASK,PAGING_SWPFPN_OFFSET)

/* OFFSET */
#define PAGING_ADDR_OFFST_LOBIT 0
#define PAGING_ADDR_OFFST_HIBIT (NBITS(PAGING_PAGESZ) - 1)

/* PAGE Num */
#define PAGING_ADDR_PGN_LOBIT NBITS(PAGING_PAGESZ)
#define PAGING_ADDR_PGN_HIBIT (PAGING_CPU_BUS_WIDTH - 1)

/* Frame PHY Num */
#define PAGING_ADDR_FPN_LOBIT NBITS(PAGING_PAGESZ)
#define PAGING_ADDR_FPN_HIBIT (NBITS(PAGING_MEMRAMSZ) - 1)

/* SWAPFPN */
#define PAGING_SWP_LOBIT NBITS(PAGING_PAGESZ)
#define PAGING_SWP_HIBIT (NBITS(PAGING_MEMSWPSZ) - 1)
#define PAGING_SWP(pte) ((pte&PAGING_PTE_SWPOFF_MASK) >> PAGING_SWPFPN_OFFSET)

/* Value operators */
#define SETBIT(v,mask) (v=v|mask)
#define CLRBIT(v,mask) (v=v&~mask)

#define SETVAL(v,value,mask,offst) (v=(v&~mask)|((value<<offst)&mask))
#define GETVAL(v,mask,offst) ((v&mask)>>offst)

/* Masks */
#define PAGING_OFFST_MASK  GENMASK(PAGING_ADDR_OFFST_HIBIT,PAGING_ADDR_OFFST_LOBIT)
#define PAGING_PGN_MASK  GENMASK(PAGING_ADDR_PGN_HIBIT,PAGING_ADDR_PGN_LOBIT)
#define PAGING_FPN_MASK  GENMASK(PAGING_ADDR_FPN_HIBIT,PAGING_ADDR_FPN_LOBIT)
#define PAGING_SWP_MASK  GENMASK(PAGING_SWP_HIBIT,PAGING_SWP_LOBIT)

/* Extract OFFSET */
//#define PAGING_OFFST(x)  ((x&PAGING_OFFST_MASK) >> PAGING_ADDR_OFFST_LOBIT)
#define PAGING_OFFST(x)  GETVAL(x,PAGING_OFFST_MASK,PAGING_ADDR_OFFST_LOBIT)
/* Extract Page Number*/
#define PAGING_PGN(x)  GETVAL(x,PAGING_PGN_MASK,PAGING_ADDR_PGN_LOBIT)
/* Extract FramePHY Number*/
#define PAGING_FPN(x)  GETVAL(x,PAGING_PTE_FPN_MASK,PAGING_PTE_FPN_LOBIT)
/* Extract SWAPFPN */
#define PAGING_PGN(x)  GETVAL(x,PAGING_PGN_MASK,PAGING_ADDR_PGN_LOBIT)
/* Extract SWAPTYPE */
#define PAGING_FPN(x)  GETVAL(x,PAGING_PTE_FPN_MASK,PAGING_PTE_FPN_LOBIT)

/* Memory range operator */
/* TODO implement the INCLUDE and OVERLAP checking mechanism */
#define INCLUDE(x1,x2,y1,y2) (0)
#define OVERLAP(x1,x2,y1,y2) (0)

/* VM region prototypes */
struct vm_rg_struct * init_vm_rg(addr_t rg_start, addr_t rg_end);
int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct* rgnode);
int enlist_pgn_node(struct pgn_t **pgnlist, addr_t pgn);
int vmap_pgd_memset(struct pcb_t *caller, addr_t addr, int pgnum);
addr_t vmap_page_range(struct pcb_t *caller, addr_t addr, int pgnum, 
                    struct framephy_struct *frames, struct vm_rg_struct *ret_rg);
addr_t vm_map_ram(struct pcb_t *caller, addr_t astart, addr_t aend, addr_t mapstart, int incpgnum, struct vm_rg_struct *ret_rg);
addr_t alloc_pages_range(struct pcb_t *caller, int incpgnum, struct framephy_struct **frm_lst);
int __swap_cp_page(struct memphy_struct *mpsrc, addr_t srcfpn,
                struct memphy_struct *mpdst, addr_t dstfpn) ;
int get_pd_from_address(addr_t addr, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt);
int get_pd_from_pagenum(addr_t pgn, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt);
int pte_set_fpn(struct pcb_t *caller, addr_t pgn, addr_t fpn);
int pte_set_swap(struct pcb_t *caller, addr_t pgn, int swptyp, addr_t swpoff);
uint32_t pte_get_entry(struct pcb_t *caller, addr_t pgn);
int pte_set_entry(struct pcb_t *caller, addr_t pgn, uint32_t pte_val);
int init_pte(addr_t *pte,
             int pre,    // present
             addr_t fpn,    // FPN
             int drt,    // dirty
             int swp,    // swap
             int swptyp, // swap type
             addr_t swpoff); //swap offset
int __alloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr);
int __free(struct pcb_t *caller, int vmaid, int rgid);
int __read(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data);
int __write(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value);
int init_mm(struct mm_struct *mm, struct pcb_t *caller);

/* VM prototypes */
int pgalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index);
int pgfree_data(struct pcb_t *proc, uint32_t reg_index);
int pgread(
		struct pcb_t * proc, // Process executing the instruction
		uint32_t source, // Index of source register
		addr_t offset, // Source address = [source] + [offset]
		uint32_t destination);
int pgwrite(
		struct pcb_t * proc, // Process executing the instruction
		BYTE data, // Data to be wrttien into memory
		uint32_t destination, // Index of destination register
		addr_t offset);
/* Local VM prototypes */
struct vm_rg_struct * get_symrg_byid(struct mm_struct* mm, int rgid);
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, addr_t vmastart, addr_t vmaend);
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg);
int inc_vma_limit(struct pcb_t *caller, int vmaid, addr_t inc_sz);
int find_victim_page(struct mm_struct* mm, addr_t *pgn);
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid);

/* MEM/PHY protypes */
/* Allocate a free physical frame
 * - Returns the frame number via `fpn`.
 * - Moves the node from `free_fp_list` to `used_fp_list`.
 * - Returns -1 if no free frames are available (trigger swap/oom).
 */
int MEMPHY_get_freefp(struct memphy_struct *mp, addr_t *fpn);

/* Release a physical frame
 * - Finds the frame in `used_fp_list` and moves it back to `free_fp_list`.
 * - If not found, creates a new free node to keep lists consistent.
 */
int MEMPHY_put_freefp(struct memphy_struct *mp, addr_t fpn);

/* Read one byte from the memphy storage at `addr`. */
int MEMPHY_read(struct memphy_struct * mp, addr_t addr, BYTE *value);

/* Write one byte into the memphy storage at `addr`. */
int MEMPHY_write(struct memphy_struct * mp, addr_t addr, BYTE data);

/* Dump memphy contents for debugging/tracing. */
int MEMPHY_dump(struct memphy_struct * mp);

/* Initialize memphy: allocate `storage` of `max_size` and split into frames
 * sized by `PAGING_PAGESZ`, building free/used lists.
 */
int init_memphy(struct memphy_struct *mp, addr_t max_size, int randomflg);

/* Allocate and initialize a new memphy instance. Returns via `ret`. */
int MEMPHY_create(struct memphy_struct **ret, addr_t max_size, int randomflg);

/* Free internal memphy resources
 * - Frees `storage` and all nodes in `free_fp_list` and `used_fp_list`.
 * - Does NOT free the `mp` pointer itself (caller should free if allocated).
 */
int MEMPHY_destroy(struct memphy_struct *mp);

/* print list */
int print_list_fp(struct framephy_struct *fp);
int print_list_rg(struct vm_rg_struct *rg);
int print_list_vma(struct vm_area_struct *rg);


int print_list_pgn(struct pgn_t *ip);
int print_pgtbl(struct pcb_t *ip, addr_t start, addr_t end);
#endif
