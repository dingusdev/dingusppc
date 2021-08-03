/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-20 divingkatae and maximum
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

// The opcodes for the processor - ppcopcodes.cpp

#ifndef PPCMEMORY_H
#define PPCMEMORY_H

#include <array>
#include <cinttypes>
#include <vector>
#include "devices/memctrlbase.h"

/* Uncomment this to exhaustive MMU integrity checks. */
//#define MMU_INTEGRITY_CHECKS

/** generic PowerPC BAT descriptor (MMU internal state) */
typedef struct PPC_BAT_entry {
    uint8_t access;   /* copy of Vs | Vp bits */
    uint8_t prot;     /* copy of PP bits */
    uint32_t phys_hi; /* high-order bits for physical address generation */
    uint32_t hi_mask; /* mask for high-order logical address bits */
    uint32_t bepi;    /* copy of Block effective page index */
} PPC_BAT_entry;

/** Block address translation types. */
enum BATType : int {
    Instruction,
    Data
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

#define PAGE_SIZE_BITS      12
#define TLB_SIZE            4096
#define TLB2_WAYS           4
#define TLB_INVALID_TAG     0xFFFFFFFF

typedef struct TLBEntry {
    uint32_t    tag;
    uint16_t    flags;
    uint16_t    lru_bits;
    union {
        int64_t             host_va_offset;
        AddressMapEntry*    reg_desc;
    };
} TLBEntry;

enum TLBFlags : uint16_t {
    PAGE_MEM      = 1 << 0, // memory page backed by host memory
    PAGE_IO       = 1 << 1, // memory mapped I/O page
    TLBE_FROM_BAT = 1 << 2, // TLB entry has been translated with BAT
    TLBE_FROM_PAT = 1 << 3, // TLB entry has been translated with PAT
    PAGE_WRITABLE = 1 << 4, // page is writable
    PTE_SET_C     = 1 << 5, // tells if C bit of the PTE needs to be updated
};

extern void ibat_update(uint32_t bat_reg);
extern void dbat_update(uint32_t bat_reg);

extern uint8_t* mmu_get_dma_mem(uint32_t addr, uint32_t size);

extern void mmu_change_mode(void);
extern void mmu_pat_ctx_changed();
extern void tlb_flush_entry(uint32_t ea);
extern void tlb_flush_bat_entries();
extern void tlb_flush_pat_entries();

extern void ppc_set_cur_instruction(const uint8_t* ptr);
extern void mem_write_byte(uint32_t addr, uint8_t value);
extern void mem_write_word(uint32_t addr, uint16_t value);
extern void mem_write_dword(uint32_t addr, uint32_t value);
extern void mem_write_qword(uint32_t addr, uint64_t value);
extern uint8_t mem_grab_byte(uint32_t addr);
extern uint16_t mem_grab_word(uint32_t addr);
extern uint32_t mem_grab_dword(uint32_t addr);
extern uint64_t mem_grab_qword(uint32_t addr);
extern uint64_t mem_read_dbg(uint32_t virt_addr, uint32_t size);
extern uint8_t* quickinstruction_translate(uint32_t address_grab);

template <class T>
extern T mmu_read_vmem(uint32_t guest_va);
template <class T>
extern void mmu_write_vmem(uint32_t guest_va, T value);

#endif    // PPCMEMORY_H
