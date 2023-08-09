/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-21 divingkatae and maximum
                      (theweirdo)     spatium

(Contact divingkatae#1017 or powermax#2286 on Discord for more info)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/** @file PowerPC Memory Management Unit definitions. */

#ifndef PPCMMU_H
#define PPCMMU_H

#include <devices/memctrl/memctrlbase.h>

#include <cinttypes>
#include <functional>

class MMIODevice;

/* Uncomment this to exhaustive MMU integrity checks. */
//#define MMU_INTEGRITY_CHECKS

/** generic PowerPC BAT descriptor (MMU internal state) */
typedef struct PPC_BAT_entry {
    bool        valid;   /* BAT entry valid for MPC601 */
    uint8_t     access;  /* copy of Vs | Vp bits */
    uint8_t     prot;    /* copy of PP bits */
    uint32_t    phys_hi; /* high-order bits for physical address generation */
    uint32_t    hi_mask; /* mask for high-order logical address bits */
    uint32_t    bepi;    /* copy of Block effective page index */
} PPC_BAT_entry;

/** Block address translation types. */
enum BATType : int {
    IBAT,
    DBAT
};

/** TLB types. */
enum TLBType : int {
    ITLB,
    DTLB
};

/** Result of the block address translation. */
typedef struct BATResult {
    bool        hit;
    uint8_t     prot;
    uint32_t    phys;
} BATResult;

/** Result of the page address translation. */
typedef struct PATResult {
    uint32_t    phys;
    uint8_t     prot;
    uint8_t     pte_c_status; // status of the C bit of the PTE
} PATResult;

/** DMA memory mapping result. */
typedef struct MapDmaResult {
    uint32_t    type;
    bool        is_writable;
    // for memory regions
    uint8_t*    host_va;
    // for MMIO regions
    MMIODevice* dev_obj;
    uint32_t    dev_base;
} MapDmaResult;

#define PAGE_SIZE_BITS      12
#define PAGE_SIZE           (1 << PAGE_SIZE_BITS)
#define PAGE_MASK           ~(PAGE_SIZE - 1)
#define TLB_SIZE            4096
#define TLB2_WAYS           4
#define TLB_INVALID_TAG     0xFFFFFFFF

typedef struct TLBEntry {
    uint32_t    tag;
    uint16_t    flags;
    uint16_t    lru_bits;
    union {
        struct { // for memory pages
            int64_t host_va_offs_r;
            int64_t host_va_offs_w;
        };
        struct { // for MMIO pages
            AddressMapEntry*    rgn_desc;
            int64_t             dev_base_va;
        };
    };
    uint32_t phys_tag;
    uint32_t reserved;
} TLBEntry;

enum TLBFlags : uint16_t {
    PAGE_MEM      = 1 << 0, // memory page backed by host memory
    PAGE_IO       = 1 << 1, // memory mapped I/O page
    PAGE_NOPHYS   = 1 << 2, // no physical storage for this page (unmapped)
    TLBE_FROM_BAT = 1 << 3, // TLB entry has been translated with BAT
    TLBE_FROM_PAT = 1 << 4, // TLB entry has been translated with PAT
    PAGE_WRITABLE = 1 << 5, // page is writable
    PTE_SET_C     = 1 << 6, // tells if C bit of the PTE needs to be updated
};

extern std::function<void(uint32_t bat_reg)> ibat_update;
extern std::function<void(uint32_t bat_reg)> dbat_update;

extern MapDmaResult mmu_map_dma_mem(uint32_t addr, uint32_t size, bool allow_mmio);

extern void mmu_change_mode(void);
extern void mmu_pat_ctx_changed();
extern void tlb_flush_entry(uint32_t ea);

extern uint64_t mem_read_dbg(uint32_t virt_addr, uint32_t size);
uint8_t *mmu_translate_imem(uint32_t vaddr, uint32_t *paddr = nullptr);
bool mmu_translate_dbg(uint32_t guest_va, uint32_t &guest_pa);

template <class T>
extern T mmu_read_vmem(uint32_t guest_va);
template <class T>
extern void mmu_write_vmem(uint32_t guest_va, T value);

//====================== Deprecated calls =========================
#if 0
extern void mem_write_byte(uint32_t addr, uint8_t value);
extern void mem_write_word(uint32_t addr, uint16_t value);
extern void mem_write_dword(uint32_t addr, uint32_t value);
extern void mem_write_qword(uint32_t addr, uint64_t value);
extern uint8_t mem_grab_byte(uint32_t addr);
extern uint16_t mem_grab_word(uint32_t addr);
extern uint32_t mem_grab_dword(uint32_t addr);
extern uint64_t mem_grab_qword(uint32_t addr);
extern uint8_t* quickinstruction_translate(uint32_t address_grab);
#endif

#endif    // PPCMMU_H
