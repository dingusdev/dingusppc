/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-23 divingkatae and maximum
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

/** @file PowerPC Memory Management Unit emulation. */

#include <devices/memctrl/memctrlbase.h>
#include <devices/common/mmiodevice.h>
#include <memaccess.h>
#include "ppcemu.h"
#include "ppcmmu.h"

#include <array>
#include <cinttypes>
#include <loguru.hpp>
#include <stdexcept>

//#define MMU_PROFILING // uncomment this to enable MMU profiling
//#define TLB_PROFILING // uncomment this to enable SoftTLB profiling

/* pointer to exception handler to be called when a MMU exception is occurred. */
void (*mmu_exception_handler)(Except_Type exception_type, uint32_t srr1_bits);

/* pointers to BAT update functions. */
std::function<void(uint32_t bat_reg)> ibat_update;
std::function<void(uint32_t bat_reg)> dbat_update;

/** PowerPC-style MMU BAT arrays (NULL initialization isn't prescribed). */
PPC_BAT_entry ibat_array[4] = {{0}};
PPC_BAT_entry dbat_array[4] = {{0}};

#ifdef MMU_PROFILING

/* global variables for lightweight MMU profiling */
uint64_t    dmem_reads_total   = 0; // counts reads from data memory
uint64_t    iomem_reads_total  = 0; // counts I/O memory reads
uint64_t    dmem_writes_total  = 0; // counts writes to data memory
uint64_t    iomem_writes_total = 0; // counts I/O memory writes
uint64_t    exec_reads_total   = 0; // counts reads from executable memory
uint64_t    bat_transl_total   = 0; // counts BAT translations
uint64_t    ptab_transl_total  = 0; // counts page table translations
uint64_t    unaligned_reads    = 0; // counts unaligned reads
uint64_t    unaligned_writes   = 0; // counts unaligned writes
uint64_t    unaligned_crossp_r = 0; // counts unaligned crosspage reads
uint64_t    unaligned_crossp_w = 0; // counts unaligned crosspage writes

#endif // MMU_PROFILING

#ifdef TLB_PROFILING

/* global variables for lightweight SoftTLB profiling */
uint64_t    num_primary_itlb_hits   = 0; // number of hits in the primary ITLB
uint64_t    num_secondary_itlb_hits = 0; // number of hits in the secondary ITLB
uint64_t    num_itlb_refills        = 0; // number of ITLB refills
uint64_t    num_primary_dtlb_hits   = 0; // number of hits in the primary DTLB
uint64_t    num_secondary_dtlb_hits = 0; // number of hits in the secondary DTLB
uint64_t    num_dtlb_refills        = 0; // number of DTLB refills
uint64_t    num_entry_replacements  = 0; // number of entry replacements

#endif // TLB_PROFILING

/** remember recently used physical memory regions for quicker translation. */
AddressMapEntry last_read_area;
AddressMapEntry last_write_area;
AddressMapEntry last_exec_area;
AddressMapEntry last_ptab_area;

/** Dummy pages for catching writes to physical read-only pages */
static std::array<uint64_t, 8192 / sizeof(uint64_t)> dummy_page;

/** 601-style block address translation. */
static BATResult mpc601_block_address_translation(uint32_t la)
{
    uint32_t pa;    // translated physical address
    uint8_t  prot;  // protection bits for the translated address
    unsigned key;

    bool bat_hit    = false;
    unsigned msr_pr = !!(ppc_state.msr & MSR::PR);

    // I/O controller interface takes precedence over BAT in 601
    // Report BAT miss if T bit is set in the corresponding SR
    if (ppc_state.sr[(la >> 28) & 0x0F] & 0x80000000) {
        return BATResult{false, 0, 0};
    }

    for (int bat_index = 0; bat_index < 4; bat_index++) {
        PPC_BAT_entry* bat_entry = &ibat_array[bat_index];

        if (bat_entry->valid && ((la & bat_entry->hi_mask) == bat_entry->bepi)) {
            bat_hit = true;

            key = (((bat_entry->access & 1) & msr_pr) |
                  (((bat_entry->access >> 1) & 1) & (msr_pr ^ 1)));

            // remapping BAT access from 601-style to PowerPC-style
            static uint8_t access_conv[8] = {2, 2, 2, 1, 0, 1, 2, 1};

            prot = access_conv[(key << 2) | bat_entry->prot];

#ifdef MMU_PROFILING
            bat_transl_total++;
#endif

            // logical to physical translation
            pa = bat_entry->phys_hi | (la & ~bat_entry->hi_mask);
            return BATResult{bat_hit, prot, pa};
        }
    }

    return BATResult{bat_hit, 0, 0};
}

/** PowerPC-style block address translation. */
template <const BATType type>
static BATResult ppc_block_address_translation(uint32_t la)
{
    uint32_t pa = 0;    // translated physical address
    uint8_t  prot = 0;  // protection bits for the translated address
    PPC_BAT_entry *bat_array;

    bool bat_hit    = false;
    unsigned msr_pr = !!(ppc_state.msr & MSR::PR);

    bat_array = (type == BATType::IBAT) ? ibat_array : dbat_array;

    // Format: %XY
    // X - supervisor access bit, Y - problem/user access bit
    // Those bits are mutually exclusive
    unsigned access_bits = ((msr_pr ^ 1) << 1) | msr_pr;

    for (int bat_index = 0; bat_index < 4; bat_index++) {
        PPC_BAT_entry* bat_entry = &bat_array[bat_index];

        if ((bat_entry->access & access_bits) && ((la & bat_entry->hi_mask) == bat_entry->bepi)) {
            bat_hit = true;

#ifdef MMU_PROFILING
            bat_transl_total++;
#endif
            // logical to physical translation
            pa = bat_entry->phys_hi | (la & ~bat_entry->hi_mask);
            prot = bat_entry->prot;
            break;
        }
    }

    return BATResult{bat_hit, prot, pa};
}

static inline uint8_t* calc_pteg_addr(uint32_t hash)
{
    uint32_t sdr1_val, pteg_addr;

    sdr1_val = ppc_state.spr[SPR::SDR1];

    pteg_addr = sdr1_val & 0xFE000000;
    pteg_addr |= (sdr1_val & 0x01FF0000) | (((sdr1_val & 0x1FF) << 16) & ((hash & 0x7FC00) << 6));
    pteg_addr |= (hash & 0x3FF) << 6;

    if (pteg_addr >= last_ptab_area.start && pteg_addr <= last_ptab_area.end) {
        return last_ptab_area.mem_ptr + (pteg_addr - last_ptab_area.start);
    } else {
        AddressMapEntry* entry = mem_ctrl_instance->find_range(pteg_addr);
        if (entry && entry->type & (RT_ROM | RT_RAM)) {
            last_ptab_area.start   = entry->start;
            last_ptab_area.end     = entry->end;
            last_ptab_area.mem_ptr = entry->mem_ptr;
            return last_ptab_area.mem_ptr + (pteg_addr - last_ptab_area.start);
        } else {
            ABORT_F("SOS: no page table region was found at %08X!\n", pteg_addr);
        }
    }
}

static bool search_pteg(uint8_t* pteg_addr, uint8_t** ret_pte_addr, uint32_t vsid,
                        uint16_t page_index, uint8_t pteg_num)
{
    /* construct PTE matching word */
    uint32_t pte_check = 0x80000000 | (vsid << 7) | (pteg_num << 6) | (page_index >> 10);

#ifdef MMU_INTEGRITY_CHECKS
    /* PTEG integrity check that ensures that all matching PTEs have
     identical RPN, WIMG and PP bits (PPC PEM 32-bit 7.6.2, rule 5). */
    uint32_t pte_word2_check;
    bool match_found = false;

    for (int i = 0; i < 8; i++, pteg_addr += 8) {
        if (pte_check == READ_DWORD_BE_A(pteg_addr)) {
            if (match_found) {
                if ((READ_DWORD_BE_A(pteg_addr) & 0xFFFFF07B) != pte_word2_check) {
                    ABORT_F("Multiple PTEs with different RPN/WIMG/PP found!\n");
                }
            } else {
                /* isolate RPN, WIMG and PP fields */
                pte_word2_check = READ_DWORD_BE_A(pteg_addr) & 0xFFFFF07B;
                *ret_pte_addr   = pteg_addr;
            }
        }
    }
#else
    for (int i = 0; i < 8; i++, pteg_addr += 8) {
        if (pte_check == READ_DWORD_BE_A(pteg_addr)) {
            *ret_pte_addr = pteg_addr;
            return true;
        }
    }
#endif

    return false;
}

static PATResult page_address_translation(uint32_t la, bool is_instr_fetch,
                                          unsigned msr_pr, int is_write)
{
    uint32_t sr_val, page_index, pteg_hash1, vsid, pte_word2;
    unsigned key, pp;
    uint8_t* pte_addr;

    sr_val = ppc_state.sr[(la >> 28) & 0x0F];
    if (sr_val & 0x80000000) {
        // check for 601-specific memory-forced I/O segments
        if (((sr_val >> 20) & 0x1FF) == 0x7F) {
            return PATResult{
                (la & 0x0FFFFFFF) | (sr_val << 28),
                0, // prot = read/write
                1  // no C bit updates
            };
        } else {
            ABORT_F("Direct-store segments not supported, LA=0x%X\n", la);
        }
    }

    /* instruction fetch from a no-execute segment will cause ISI exception */
    if ((sr_val & 0x10000000) && is_instr_fetch) {
        mmu_exception_handler(Except_Type::EXC_ISI, 0x10000000);
    }

    page_index = (la >> 12) & 0xFFFF;
    pteg_hash1 = (sr_val & 0x7FFFF) ^ page_index;
    vsid       = sr_val & 0x0FFFFFF;

    if (!search_pteg(calc_pteg_addr(pteg_hash1), &pte_addr, vsid, page_index, 0)) {
        if (!search_pteg(calc_pteg_addr(~pteg_hash1), &pte_addr, vsid, page_index, 1)) {
            if (is_instr_fetch) {
                mmu_exception_handler(Except_Type::EXC_ISI, 0x40000000);
            } else {
                ppc_state.spr[SPR::DSISR] = 0x40000000 | (is_write << 25);
                ppc_state.spr[SPR::DAR]   = la;
                mmu_exception_handler(Except_Type::EXC_DSI, 0);
            }
        }
    }

    pte_word2 = READ_DWORD_BE_A(pte_addr + 4);

    key = (((sr_val >> 29) & 1) & msr_pr) | (((sr_val >> 30) & 1) & (msr_pr ^ 1));

    /* check page access */
    pp = pte_word2 & 3;

    // the following scenarios cause DSI/ISI exception:
    // any access with key = 1 and PP = %00
    // write access with key = 1 and PP = %01
    // write access with PP = %11
    if ((key && (!pp || (pp == 1 && is_write))) || (pp == 3 && is_write)) {
        if (is_instr_fetch) {
            mmu_exception_handler(Except_Type::EXC_ISI, 0x08000000);
        } else {
            ppc_state.spr[SPR::DSISR] = 0x08000000 | (is_write << 25);
            ppc_state.spr[SPR::DAR]   = la;
            mmu_exception_handler(Except_Type::EXC_DSI, 0);
        }
    }

    /* update R and C bits */
    /* For simplicity, R is set on each access, C is set only for writes */
    pte_addr[6] |= 0x01;
    if (is_write) {
        pte_addr[7] |= 0x80;
    }

    /* return physical address, access protection and C status */
    return PATResult{
        ((pte_word2 & 0xFFFFF000) | (la & 0x00000FFF)),
        static_cast<uint8_t>((key << 2) | pp),
        static_cast<uint8_t>(pte_word2 & 0x80)
    };
}

MapDmaResult mmu_map_dma_mem(uint32_t addr, uint32_t size, bool allow_mmio) {
    MMIODevice      *devobj  = nullptr;
    uint8_t         *host_va = nullptr;
    uint32_t        dev_base = 0;
    bool            is_writable;
    AddressMapEntry *cur_dma_rgn;

    cur_dma_rgn = mem_ctrl_instance->find_range(addr);
    if (!cur_dma_rgn) {
        ABORT_F("SOS: DMA access to unmapped physical memory 0x%08X..0x%08X!",
            addr, addr + size - 1
        );
    }

    if (addr + size - 1 > cur_dma_rgn->end) {
        ABORT_F("SOS: DMA access to unmapped physical memory 0x%08X..0x%08X because size extends outside region 0x%08X..0x%08X!",
            addr, addr + size - 1, cur_dma_rgn->start, cur_dma_rgn->end
        );
    }

    if ((cur_dma_rgn->type & RT_MMIO) && !allow_mmio) {
        ABORT_F("SOS: DMA access to a MMIO region 0x%08X..0x%08X (%s) for physical memory 0x%08X..0x%08X is not allowed.",
            cur_dma_rgn->start, cur_dma_rgn->end, cur_dma_rgn->devobj->get_name().c_str(), addr, addr + size - 1
        );
    }

    if (cur_dma_rgn->type & (RT_ROM | RT_RAM)) {
        host_va  = cur_dma_rgn->mem_ptr + (addr - cur_dma_rgn->start);
        is_writable = cur_dma_rgn->type & RT_RAM;
    } else { // RT_MMIO
        devobj = cur_dma_rgn->devobj;
        dev_base = cur_dma_rgn->start;
        is_writable = true; // all MMIO devices must provide a write method
    }

    return MapDmaResult{cur_dma_rgn->type, is_writable, host_va, devobj, dev_base};
}

// primary ITLB for all MMU modes
static std::array<TLBEntry, TLB_SIZE> itlb1_mode1;
static std::array<TLBEntry, TLB_SIZE> itlb1_mode2;
static std::array<TLBEntry, TLB_SIZE> itlb1_mode3;

// secondary ITLB for all MMU modes
static std::array<TLBEntry, TLB_SIZE*TLB2_WAYS> itlb2_mode1;
static std::array<TLBEntry, TLB_SIZE*TLB2_WAYS> itlb2_mode2;
static std::array<TLBEntry, TLB_SIZE*TLB2_WAYS> itlb2_mode3;

// primary DTLB for all MMU modes
static std::array<TLBEntry, TLB_SIZE> dtlb1_mode1;
static std::array<TLBEntry, TLB_SIZE> dtlb1_mode2;
static std::array<TLBEntry, TLB_SIZE> dtlb1_mode3;

// secondary DTLB for all MMU modes
static std::array<TLBEntry, TLB_SIZE*TLB2_WAYS> dtlb2_mode1;
static std::array<TLBEntry, TLB_SIZE*TLB2_WAYS> dtlb2_mode2;
static std::array<TLBEntry, TLB_SIZE*TLB2_WAYS> dtlb2_mode3;

TLBEntry *pCurITLB1; // current primary ITLB
TLBEntry *pCurITLB2; // current secondary ITLB
TLBEntry *pCurDTLB1; // current primary DTLB
TLBEntry *pCurDTLB2; // current secondary DTLB

uint32_t tlb_size_mask = TLB_SIZE - 1;

// fake TLB entry for handling of unmapped memory accesses
uint64_t    UnmappedVal = -1ULL;
TLBEntry    UnmappedMem = {TLB_INVALID_TAG, TLBFlags::PAGE_NOPHYS, 0, 0};

uint8_t     CurITLBMode = {0xFF}; // current ITLB mode
uint8_t     CurDTLBMode = {0xFF}; // current DTLB mode

void mmu_change_mode()
{
    uint8_t mmu_mode;

    // switch ITLB tables first
    mmu_mode = ((!!(ppc_state.msr & MSR::IR)) << 1) | !!(ppc_state.msr & MSR::PR);

    if (CurITLBMode != mmu_mode) {
        switch(mmu_mode) {
            case 0: // real address mode
                pCurITLB1 = &itlb1_mode1[0];
                pCurITLB2 = &itlb2_mode1[0];
                break;
            case 2: // supervisor mode with instruction translation enabled
                pCurITLB1 = &itlb1_mode2[0];
                pCurITLB2 = &itlb2_mode2[0];
                break;
            case 1:
                // user mode can't disable translations
                //LOG_F(ERROR, "instruction mmu mode 1 is invalid!"); // this happens alot. Maybe it's not invalid?
                mmu_mode = 3;
            case 3: // user mode with instruction translation enabled
                pCurITLB1 = &itlb1_mode3[0];
                pCurITLB2 = &itlb2_mode3[0];
                break;
        }
        CurITLBMode = mmu_mode;
    }

    // then switch DTLB tables
    mmu_mode = ((!!(ppc_state.msr & MSR::DR)) << 1) | !!(ppc_state.msr & MSR::PR);

    if (CurDTLBMode != mmu_mode) {
        switch(mmu_mode) {
            case 0: // real address mode
                pCurDTLB1 = &dtlb1_mode1[0];
                pCurDTLB2 = &dtlb2_mode1[0];
                break;
            case 2: // supervisor mode with data translation enabled
                pCurDTLB1 = &dtlb1_mode2[0];
                pCurDTLB2 = &dtlb2_mode2[0];
                break;
            case 1:
                // user mode can't disable translations
                LOG_F(ERROR, "data mmu mode 1 is invalid!");
                mmu_mode = 3;
            case 3: // user mode with data translation enabled
                pCurDTLB1 = &dtlb1_mode3[0];
                pCurDTLB2 = &dtlb2_mode3[0];
                break;
        }
        CurDTLBMode = mmu_mode;
    }
}

template <const TLBType tlb_type>
static TLBEntry* tlb2_target_entry(uint32_t gp_va)
{
    TLBEntry *tlb_entry;

    if (tlb_type == TLBType::ITLB) {
        tlb_entry = &pCurITLB2[((gp_va >> PPC_PAGE_SIZE_BITS) & tlb_size_mask) * TLB2_WAYS];
    } else {
        tlb_entry = &pCurDTLB2[((gp_va >> PPC_PAGE_SIZE_BITS) & tlb_size_mask) * TLB2_WAYS];
    }

    // select the target from invalid blocks first
    if (tlb_entry[0].tag == TLB_INVALID_TAG) {
        // update LRU bits
        tlb_entry[0].lru_bits  = 0x3;
        tlb_entry[1].lru_bits  = 0x2;
        tlb_entry[2].lru_bits &= 0x1;
        tlb_entry[3].lru_bits &= 0x1;
        return tlb_entry;
    } else if (tlb_entry[1].tag == TLB_INVALID_TAG) {
        // update LRU bits
        tlb_entry[0].lru_bits  = 0x2;
        tlb_entry[1].lru_bits  = 0x3;
        tlb_entry[2].lru_bits &= 0x1;
        tlb_entry[3].lru_bits &= 0x1;
        return &tlb_entry[1];
    } else if (tlb_entry[2].tag == TLB_INVALID_TAG) {
        // update LRU bits
        tlb_entry[0].lru_bits &= 0x1;
        tlb_entry[1].lru_bits &= 0x1;
        tlb_entry[2].lru_bits  = 0x3;
        tlb_entry[3].lru_bits  = 0x2;
        return &tlb_entry[2];
    } else if (tlb_entry[3].tag == TLB_INVALID_TAG) {
        // update LRU bits
        tlb_entry[0].lru_bits &= 0x1;
        tlb_entry[1].lru_bits &= 0x1;
        tlb_entry[2].lru_bits  = 0x2;
        tlb_entry[3].lru_bits  = 0x3;
        return &tlb_entry[3];
    } else { // no free entries, replace an existing one according with the hLRU policy
#ifdef TLB_PROFILING
        num_entry_replacements++;
#endif
        if (tlb_entry[0].lru_bits == 0) {
            // update LRU bits
            tlb_entry[0].lru_bits  = 0x3;
            tlb_entry[1].lru_bits  = 0x2;
            tlb_entry[2].lru_bits &= 0x1;
            tlb_entry[3].lru_bits &= 0x1;
            return tlb_entry;
        } else if (tlb_entry[1].lru_bits == 0) {
            // update LRU bits
            tlb_entry[0].lru_bits  = 0x2;
            tlb_entry[1].lru_bits  = 0x3;
            tlb_entry[2].lru_bits &= 0x1;
            tlb_entry[3].lru_bits &= 0x1;
            return &tlb_entry[1];
        } else if (tlb_entry[2].lru_bits == 0) {
            // update LRU bits
            tlb_entry[0].lru_bits &= 0x1;
            tlb_entry[1].lru_bits &= 0x1;
            tlb_entry[2].lru_bits  = 0x3;
            tlb_entry[3].lru_bits  = 0x2;
            return &tlb_entry[2];
        } else {
            // update LRU bits
            tlb_entry[0].lru_bits &= 0x1;
            tlb_entry[1].lru_bits &= 0x1;
            tlb_entry[2].lru_bits  = 0x2;
            tlb_entry[3].lru_bits  = 0x3;
            return &tlb_entry[3];
        }
    }
}

static TLBEntry* itlb2_refill(uint32_t guest_va)
{
    BATResult bat_res;
    uint32_t phys_addr;
    TLBEntry *tlb_entry;
    uint16_t flags = 0;

    /* instruction address translation if enabled */
    if (ppc_state.msr & MSR::IR) {
        // attempt block address translation first
        if (is_601) {
            bat_res = mpc601_block_address_translation(guest_va);
        } else {
            bat_res = ppc_block_address_translation<BATType::IBAT>(guest_va);
        }
        if (bat_res.hit) {
            // check block protection
            // only PP = 0 (no access) causes ISI exception
            if (!bat_res.prot) {
                mmu_exception_handler(Except_Type::EXC_ISI, 0x08000000);
            }
            phys_addr = bat_res.phys;
            flags |= TLBFlags::TLBE_FROM_BAT; // tell the world we come from
        } else {
            // page address translation
            PATResult pat_res = page_address_translation(guest_va, true, !!(ppc_state.msr & MSR::PR), 0);
            phys_addr = pat_res.phys;
            flags = TLBFlags::TLBE_FROM_PAT; // tell the world we come from
        }
    } else { // instruction translation disabled
        phys_addr = guest_va;
    }

    // look up host virtual address
    AddressMapEntry* rgn_desc = mem_ctrl_instance->find_range(phys_addr);
    if (rgn_desc) {
        if (rgn_desc->type & RT_MMIO) {
            ABORT_F("Instruction fetch from MMIO region at 0x%08X!\n", phys_addr);
        }
        // refill the secondary TLB
        const uint32_t tag = guest_va & ~0xFFFUL;
        tlb_entry = tlb2_target_entry<TLBType::ITLB>(tag);
        tlb_entry->tag = tag;
        tlb_entry->flags = flags | TLBFlags::PAGE_MEM;
        tlb_entry->host_va_offs_r = (int64_t)rgn_desc->mem_ptr - guest_va +
                                    (phys_addr - rgn_desc->start);
        tlb_entry->phys_tag = phys_addr & ~0xFFFUL;
    } else {
        ABORT_F("Instruction fetch from unmapped memory at 0x%08X!\n", phys_addr);
    }

    return tlb_entry;
}

static TLBEntry* dtlb2_refill(uint32_t guest_va, int is_write, bool is_dbg = false)
{
    BATResult bat_res;
    uint32_t phys_addr;
    uint16_t flags = 0;
    TLBEntry *tlb_entry;

    const uint32_t tag = guest_va & ~0xFFFUL;

    /* data address translation if enabled */
    if (ppc_state.msr & MSR::DR) {
        // attempt block address translation first
        if (is_601) {
            bat_res = mpc601_block_address_translation(guest_va);
        } else {
            bat_res = ppc_block_address_translation<BATType::DBAT>(guest_va);
        }
        if (bat_res.hit) {
            // check block protection
            if (!bat_res.prot || ((bat_res.prot & 1) && is_write)) {
                if (!is_dbg)
                LOG_F(9, "BAT DSI exception in TLB2 refill!");
                if (!is_dbg)
                LOG_F(9, "Attempt to write to read-only region, LA=0x%08X, PC=0x%08X!", guest_va, ppc_state.pc);
                ppc_state.spr[SPR::DSISR] = 0x08000000 | (is_write << 25);
                ppc_state.spr[SPR::DAR]   = guest_va;
                mmu_exception_handler(Except_Type::EXC_DSI, 0);
            }
            phys_addr = bat_res.phys;
            flags = TLBFlags::PTE_SET_C; // prevent PTE.C updates for BAT
            flags |= TLBFlags::TLBE_FROM_BAT; // tell the world we come from
            if (bat_res.prot == 2) {
                flags |= TLBFlags::PAGE_WRITABLE;
            }
        } else {
            // page address translation
            PATResult pat_res = page_address_translation(guest_va, false, !!(ppc_state.msr & MSR::PR), is_write);
            phys_addr = pat_res.phys;
            flags = TLBFlags::TLBE_FROM_PAT; // tell the world we come from
            if (pat_res.prot <= 2 || pat_res.prot == 6) {
                flags |= TLBFlags::PAGE_WRITABLE;
            }
            if (is_write || pat_res.pte_c_status) {
                // C-bit of the PTE is already set so the TLB logic
                // doesn't need to update it anymore
                flags |= TLBFlags::PTE_SET_C;
            }
        }
    } else { // data translation disabled
        phys_addr = guest_va;
        flags = TLBFlags::PTE_SET_C; // no PTE.C updates in real addressing mode
        flags |= TLBFlags::PAGE_WRITABLE; // assume physical pages are writable
    }

    // look up host virtual address
    AddressMapEntry* rgn_desc = mem_ctrl_instance->find_range(phys_addr);
    if (rgn_desc) {
        // refill the secondary TLB
        tlb_entry = tlb2_target_entry<TLBType::DTLB>(tag);
        tlb_entry->tag = tag;
        if (rgn_desc->type & RT_MMIO) { // MMIO region
            tlb_entry->flags = flags | TLBFlags::PAGE_IO;
            tlb_entry->rgn_desc = rgn_desc;
            tlb_entry->dev_base_va = guest_va - (phys_addr - rgn_desc->start);
        } else { // memory region backed by host memory
            tlb_entry->flags = flags | TLBFlags::PAGE_MEM;
            tlb_entry->host_va_offs_r = (int64_t)rgn_desc->mem_ptr - guest_va +
                                        (phys_addr - rgn_desc->start);
            if (rgn_desc->type == RT_ROM) {
                // redirect writes to the dummy page for ROM regions
                tlb_entry->host_va_offs_w = (int64_t)&dummy_page - tag;
            } else {
                tlb_entry->host_va_offs_w = tlb_entry->host_va_offs_r;
            }
        }
        tlb_entry->phys_tag = phys_addr & ~0xFFFUL;
        return tlb_entry;
    } else {
        if (!is_dbg) {
        static uint32_t last_phys_addr = -1;
        static uint32_t first_phys_addr = -1;
        if (phys_addr != last_phys_addr + 4) {
            if (last_phys_addr != -1 && last_phys_addr != first_phys_addr) {
                LOG_F(WARNING, "                                                         ... phys_addr=0x%08X", last_phys_addr);
            }
            first_phys_addr = phys_addr;
            LOG_F(WARNING, "Access to unmapped physical memory, phys_addr=0x%08X", first_phys_addr);
        }
        last_phys_addr = phys_addr;
        }
        return &UnmappedMem;
    }
}

template <const TLBType tlb_type>
static inline TLBEntry* lookup_secondary_tlb(uint32_t guest_va, uint32_t tag) {
    TLBEntry *tlb_entry;

    if (tlb_type == TLBType::ITLB) {
        tlb_entry = &pCurITLB2[((guest_va >> PPC_PAGE_SIZE_BITS) & tlb_size_mask) * TLB2_WAYS];
    } else {
        tlb_entry = &pCurDTLB2[((guest_va >> PPC_PAGE_SIZE_BITS) & tlb_size_mask) * TLB2_WAYS];
    }

    if (tlb_entry->tag == tag) {
        // update LRU bits
        tlb_entry[0].lru_bits  = 0x3;
        tlb_entry[1].lru_bits  = 0x2;
        tlb_entry[2].lru_bits &= 0x1;
        tlb_entry[3].lru_bits &= 0x1;
    } else if (tlb_entry[1].tag == tag) {
        // update LRU bits
        tlb_entry[0].lru_bits  = 0x2;
        tlb_entry[1].lru_bits  = 0x3;
        tlb_entry[2].lru_bits &= 0x1;
        tlb_entry[3].lru_bits &= 0x1;
        tlb_entry = &tlb_entry[1];
    } else if (tlb_entry[2].tag == tag) {
        // update LRU bits
        tlb_entry[0].lru_bits &= 0x1;
        tlb_entry[1].lru_bits &= 0x1;
        tlb_entry[2].lru_bits  = 0x3;
        tlb_entry[3].lru_bits  = 0x2;
        tlb_entry = &tlb_entry[2];
    } else if (tlb_entry[3].tag == tag) {
        // update LRU bits
        tlb_entry[0].lru_bits &= 0x1;
        tlb_entry[1].lru_bits &= 0x1;
        tlb_entry[2].lru_bits  = 0x2;
        tlb_entry[3].lru_bits  = 0x3;
        tlb_entry = &tlb_entry[3];
    } else {
        return nullptr;
    }
    return tlb_entry;
}

uint8_t *mmu_translate_imem(uint32_t vaddr, uint32_t *paddr)
{
    TLBEntry *tlb1_entry, *tlb2_entry;
    uint8_t *host_va;

#ifdef MMU_PROFILING
    exec_reads_total++;
#endif

    const uint32_t tag = vaddr & ~0xFFFUL;

    // look up guest virtual address in the primary ITLB
    tlb1_entry = &pCurITLB1[(vaddr >> PPC_PAGE_SIZE_BITS) & tlb_size_mask];
    if (tlb1_entry->tag == tag) { // primary ITLB hit -> fast path
#ifdef TLB_PROFILING
        num_primary_itlb_hits++;
#endif
        host_va = (uint8_t *)(tlb1_entry->host_va_offs_r + vaddr);
    } else {
        // primary ITLB miss -> look up address in the secondary ITLB
        tlb2_entry = lookup_secondary_tlb<TLBType::ITLB>(vaddr, tag);
        if (tlb2_entry == nullptr) {
#ifdef TLB_PROFILING
            num_itlb_refills++;
#endif
            // secondary ITLB miss ->
            // perform full address translation and refill the secondary ITLB
            tlb2_entry = itlb2_refill(vaddr);
        }
#ifdef TLB_PROFILING
        else {
            num_secondary_itlb_hits++;
        }
#endif
        // refill the primary ITLB
        tlb1_entry->tag = tag;
        tlb1_entry->flags = tlb2_entry->flags;
        tlb1_entry->host_va_offs_r = tlb2_entry->host_va_offs_r;
        tlb1_entry->phys_tag = tlb2_entry->phys_tag;
        host_va = (uint8_t *)(tlb1_entry->host_va_offs_r + vaddr);
    }

    ppc_set_cur_instruction(host_va);
    if (paddr)
        *paddr = tlb1_entry->phys_tag | (vaddr & 0xFFFUL);

    return host_va;
}

static void tlb_flush_primary_entry(std::array<TLBEntry, TLB_SIZE> &tlb1, uint32_t tag)
{
    TLBEntry *tlb_entry = &tlb1[(tag >> PPC_PAGE_SIZE_BITS) & tlb_size_mask];
    if (tlb_entry->tag == tag) {
        tlb_entry->tag = TLB_INVALID_TAG;
        //LOG_F(INFO, "Invalidated primary TLB entry at 0x%X", ea);
    }
}

static void tlb_flush_secondary_entry(std::array<TLBEntry, TLB_SIZE*TLB2_WAYS> &tlb2, uint32_t tag)
{
    TLBEntry *tlb_entry = &tlb2[((tag >> PPC_PAGE_SIZE_BITS) & tlb_size_mask) * TLB2_WAYS];
    for (int i = 0; i < TLB2_WAYS; i++) {
        if (tlb_entry[i].tag == tag) {
            tlb_entry[i].tag = TLB_INVALID_TAG;
            //LOG_F(INFO, "Invalidated secondary TLB entry at 0x%X", ea);
        }
    }
}

void tlb_flush_entry(uint32_t ea)
{
    const uint32_t tag = ea & ~0xFFFUL;
    tlb_flush_primary_entry(itlb1_mode1, tag);
    tlb_flush_secondary_entry(itlb2_mode1, tag);
    tlb_flush_primary_entry(itlb1_mode2, tag);
    tlb_flush_secondary_entry(itlb2_mode2, tag);
    tlb_flush_primary_entry(itlb1_mode3, tag);
    tlb_flush_secondary_entry(itlb2_mode3, tag);
    tlb_flush_primary_entry(dtlb1_mode1, tag);
    tlb_flush_secondary_entry(dtlb2_mode1, tag);
    tlb_flush_primary_entry(dtlb1_mode2, tag);
    tlb_flush_secondary_entry(dtlb2_mode2, tag);
    tlb_flush_primary_entry(dtlb1_mode3, tag);
    tlb_flush_secondary_entry(dtlb2_mode3, tag);
}

template <std::size_t N>
static void tlb_flush_entries(std::array<TLBEntry, N> &tlb, TLBFlags type) {
    for (auto &tlb_el : tlb) {
        if (tlb_el.tag != TLB_INVALID_TAG && tlb_el.flags & type) {
            tlb_el.tag = TLB_INVALID_TAG;
        }
    }
}

template <const TLBType tlb_type>
void tlb_flush_entries(TLBFlags type)
{
    // Mode 1 is real addressing and thus can't contain any PAT entries by definition.
    bool flush_mode1 = type != TLBE_FROM_PAT;
    if (tlb_type == TLBType::ITLB) {
        if (flush_mode1) {
            tlb_flush_entries(itlb1_mode1, type);
        }
        tlb_flush_entries(itlb1_mode2, type);
        tlb_flush_entries(itlb1_mode3, type);
        if (flush_mode1) {
            tlb_flush_entries(itlb2_mode1, type);
        }
        tlb_flush_entries(itlb2_mode2, type);
        tlb_flush_entries(itlb2_mode3, type);
    } else {
        if (flush_mode1) {
            tlb_flush_entries(dtlb1_mode1, type);
        }
        tlb_flush_entries(dtlb1_mode2, type);
        tlb_flush_entries(dtlb1_mode3, type);
        if (flush_mode1) {
            tlb_flush_entries(dtlb2_mode1, type);
        }
        tlb_flush_entries(dtlb2_mode2, type);
        tlb_flush_entries(dtlb2_mode3, type);
    }
}

bool gTLBFlushIBatEntries = false;
bool gTLBFlushDBatEntries = false;
bool gTLBFlushIPatEntries = false;
bool gTLBFlushDPatEntries = false;

template <const TLBType tlb_type>
void tlb_flush_bat_entries()
{
    if (tlb_type == TLBType::ITLB) {
        if (!gTLBFlushIBatEntries)
            return;
        tlb_flush_entries<TLBType::ITLB>(TLBE_FROM_BAT);
        gTLBFlushIBatEntries = false;
    } else {
        if (!gTLBFlushDBatEntries)
            return;
        tlb_flush_entries<TLBType::DTLB>(TLBE_FROM_BAT);
        gTLBFlushDBatEntries = false;
    }
}

template <const TLBType tlb_type>
void tlb_flush_pat_entries()
{
    if (tlb_type == TLBType::ITLB) {
        if (!gTLBFlushIPatEntries)
            return;
        tlb_flush_entries<TLBType::ITLB>(TLBE_FROM_PAT);
        gTLBFlushIPatEntries = false;
    } else {
        if (!gTLBFlushDPatEntries)
            return;
        tlb_flush_entries<TLBType::DTLB>(TLBE_FROM_PAT);
        gTLBFlushDPatEntries = false;
    }
}

template <const TLBType tlb_type>
void tlb_flush_all_entries()
{
    if (tlb_type == TLBType::ITLB) {
        if (!gTLBFlushIBatEntries && !gTLBFlushIPatEntries)
            return;
        tlb_flush_entries<TLBType::ITLB>((TLBFlags)(TLBE_FROM_BAT | TLBE_FROM_PAT));
        gTLBFlushIBatEntries = false;
        gTLBFlushIPatEntries = false;
    } else {
        if (!gTLBFlushDBatEntries && !gTLBFlushDPatEntries)
            return;
        tlb_flush_entries<TLBType::DTLB>((TLBFlags)(TLBE_FROM_BAT | TLBE_FROM_PAT));
        gTLBFlushDBatEntries = false;
        gTLBFlushDPatEntries = false;
    }
}

static void mpc601_bat_update(uint32_t bat_reg)
{
    PPC_BAT_entry *ibat_entry, *dbat_entry;
    uint32_t bsm, hi_mask;
    int upper_reg_num;

    upper_reg_num = bat_reg & 0xFFFFFFFE;

    ibat_entry = &ibat_array[(bat_reg - 528) >> 1];
    dbat_entry = &dbat_array[(bat_reg - 528) >> 1];

    if (ppc_state.spr[bat_reg | 1] & 0x40) {
        bsm     = ppc_state.spr[upper_reg_num + 1] & 0x3F;
        hi_mask = ~((bsm << 17) | 0x1FFFF);

        ibat_entry->valid   = true;
        ibat_entry->access  = (ppc_state.spr[upper_reg_num] >> 2) & 3;
        ibat_entry->prot    = ppc_state.spr[upper_reg_num] & 3;
        ibat_entry->hi_mask = hi_mask;
        ibat_entry->phys_hi = ppc_state.spr[upper_reg_num + 1] & hi_mask;
        ibat_entry->bepi    = ppc_state.spr[upper_reg_num] & hi_mask;

        // copy IBAT entry to DBAT entry
        *dbat_entry = *ibat_entry;
    } else {
        // disable the corresponding BAT paars
        ibat_entry->valid = false;
        dbat_entry->valid = false;
    }

    // MPC601 has unified BATs so we're going to flush both ITLB and DTLB
    if (!gTLBFlushIBatEntries || !gTLBFlushIPatEntries || !gTLBFlushDBatEntries || !gTLBFlushDPatEntries) {
        gTLBFlushIBatEntries = true;
        gTLBFlushIPatEntries = true;
        gTLBFlushDBatEntries = true;
        gTLBFlushDPatEntries = true;
        add_ctx_sync_action(&tlb_flush_all_entries<TLBType::ITLB>);
        add_ctx_sync_action(&tlb_flush_all_entries<TLBType::DTLB>);
    }
}

static void ppc_ibat_update(uint32_t bat_reg)
{
    int upper_reg_num;
    uint32_t bl, hi_mask;
    PPC_BAT_entry* bat_entry;

    upper_reg_num = bat_reg & 0xFFFFFFFE;

    bat_entry = &ibat_array[(bat_reg - 528) >> 1];
    bl        = (ppc_state.spr[upper_reg_num] >> 2) & 0x7FF;
    hi_mask   = ~((bl << 17) | 0x1FFFF);

    bat_entry->access  = ppc_state.spr[upper_reg_num] & 3;
    bat_entry->prot    = ppc_state.spr[upper_reg_num + 1] & 3;
    bat_entry->hi_mask = hi_mask;
    bat_entry->phys_hi = ppc_state.spr[upper_reg_num + 1] & hi_mask;
    bat_entry->bepi    = ppc_state.spr[upper_reg_num] & hi_mask;

    if (!gTLBFlushIBatEntries || !gTLBFlushIPatEntries) {
        gTLBFlushIBatEntries = true;
        gTLBFlushIPatEntries = true;
        add_ctx_sync_action(&tlb_flush_all_entries<TLBType::ITLB>);
    }
}

static void ppc_dbat_update(uint32_t bat_reg)
{
    int upper_reg_num;
    uint32_t bl, hi_mask;
    PPC_BAT_entry* bat_entry;

    upper_reg_num = bat_reg & 0xFFFFFFFE;

    bat_entry = &dbat_array[(bat_reg - 536) >> 1];
    bl        = (ppc_state.spr[upper_reg_num] >> 2) & 0x7FF;
    hi_mask   = ~((bl << 17) | 0x1FFFF);

    bat_entry->access  = ppc_state.spr[upper_reg_num] & 3;
    bat_entry->prot    = ppc_state.spr[upper_reg_num + 1] & 3;
    bat_entry->hi_mask = hi_mask;
    bat_entry->phys_hi = ppc_state.spr[upper_reg_num + 1] & hi_mask;
    bat_entry->bepi    = ppc_state.spr[upper_reg_num] & hi_mask;

    if (!gTLBFlushDBatEntries || !gTLBFlushDPatEntries) {
        gTLBFlushDBatEntries = true;
        gTLBFlushDPatEntries = true;
        add_ctx_sync_action(&tlb_flush_all_entries<TLBType::DTLB>);
    }

}

void mmu_pat_ctx_changed()
{
    // Page address translation context changed so we need to flush
    // all PAT entries from both ITLB and DTLB
    if (!gTLBFlushIPatEntries || !gTLBFlushDPatEntries) {
        gTLBFlushIPatEntries = true;
        gTLBFlushDPatEntries = true;
        add_ctx_sync_action(&tlb_flush_pat_entries<TLBType::ITLB>);
        add_ctx_sync_action(&tlb_flush_pat_entries<TLBType::DTLB>);
    }
}

// Forward declarations.
template <class T>
static T read_unaligned(uint32_t guest_va, uint8_t *host_va);
template <class T>
static void write_unaligned(uint32_t guest_va, uint8_t *host_va, T value);

template <class T>
inline T mmu_read_vmem(uint32_t guest_va)
{
    TLBEntry *tlb1_entry, *tlb2_entry;
    uint8_t *host_va;

    const uint32_t tag = guest_va & ~0xFFFUL;

    // look up guest virtual address in the primary TLB
    tlb1_entry = &pCurDTLB1[(guest_va >> PPC_PAGE_SIZE_BITS) & tlb_size_mask];
    if (tlb1_entry->tag == tag) { // primary TLB hit -> fast path
#ifdef TLB_PROFILING
        num_primary_dtlb_hits++;
#endif
        host_va = (uint8_t *)(tlb1_entry->host_va_offs_r + guest_va);
    } else {
        // primary TLB miss -> look up address in the secondary TLB
        tlb2_entry = lookup_secondary_tlb<TLBType::DTLB>(guest_va, tag);
        if (tlb2_entry == nullptr) {
#ifdef TLB_PROFILING
            num_dtlb_refills++;
#endif
            // secondary TLB miss ->
            // perform full address translation and refill the secondary TLB
            tlb2_entry = dtlb2_refill(guest_va, 0);
            if (tlb2_entry->flags & PAGE_NOPHYS) {
                return (T)UnmappedVal;
            }
        }
#ifdef TLB_PROFILING
        else {
            num_secondary_dtlb_hits++;
        }
#endif

        if (tlb2_entry->flags & TLBFlags::PAGE_MEM) { // is it a real memory region?
            // refill the primary TLB
            *tlb1_entry = *tlb2_entry;
            host_va = (uint8_t *)(tlb1_entry->host_va_offs_r + guest_va);
        } else { // otherwise, it's an access to a memory-mapped device
#ifdef MMU_PROFILING
            iomem_reads_total++;
#endif
            if (sizeof(T) == 8) {
                if (guest_va & 3)
                    ppc_alignment_exception(guest_va);

                return (
                    ((T)tlb2_entry->rgn_desc->devobj->read(tlb2_entry->rgn_desc->start,
                                                           static_cast<uint32_t>(guest_va - tlb2_entry->dev_base_va),
                                                           4) << 32) |
                    tlb2_entry->rgn_desc->devobj->read(tlb2_entry->rgn_desc->start,
                                                       static_cast<uint32_t>(guest_va + 4 - tlb2_entry->dev_base_va),
                                                       4)
                );
            }
            else {
                return (
                    tlb2_entry->rgn_desc->devobj->read(tlb2_entry->rgn_desc->start,
                                                       static_cast<uint32_t>(guest_va - tlb2_entry->dev_base_va),
                                                       sizeof(T))
                );
            }
        }
    }

#ifdef MMU_PROFILING
    dmem_reads_total++;
#endif

    // handle unaligned memory accesses
    if (sizeof(T) > 1 && (guest_va & (sizeof(T) - 1))) {
        return read_unaligned<T>(guest_va, host_va);
    }

    // handle aligned memory accesses
    switch(sizeof(T)) {
        case 1:
            return *host_va;
        case 2:
            return READ_WORD_BE_A(host_va);
        case 4:
            return READ_DWORD_BE_A(host_va);
        case 8:
            return READ_QWORD_BE_A(host_va);
    }
}

// explicitely instantiate all required mmu_read_vmem variants
template uint8_t  mmu_read_vmem<uint8_t>(uint32_t guest_va);
template uint16_t mmu_read_vmem<uint16_t>(uint32_t guest_va);
template uint32_t mmu_read_vmem<uint32_t>(uint32_t guest_va);
template uint64_t mmu_read_vmem<uint64_t>(uint32_t guest_va);

template <class T>
inline void mmu_write_vmem(uint32_t guest_va, T value)
{
    TLBEntry *tlb1_entry, *tlb2_entry;
    uint8_t *host_va;

    const uint32_t tag = guest_va & ~0xFFFUL;

    // look up guest virtual address in the primary TLB
    tlb1_entry = &pCurDTLB1[(guest_va >> PPC_PAGE_SIZE_BITS) & tlb_size_mask];
    if (tlb1_entry->tag == tag) { // primary TLB hit -> fast path
#ifdef TLB_PROFILING
        num_primary_dtlb_hits++;
#endif
        if (!(tlb1_entry->flags & TLBFlags::PAGE_WRITABLE)) {
            ppc_state.spr[SPR::DSISR] = 0x08000000 | (1 << 25);
            ppc_state.spr[SPR::DAR]   = guest_va;
            mmu_exception_handler(Except_Type::EXC_DSI, 0);
        }
        if (!(tlb1_entry->flags & TLBFlags::PTE_SET_C)) {
            // perform full page address translation to update PTE.C bit
            page_address_translation(guest_va, false, !!(ppc_state.msr & MSR::PR), true);
            tlb1_entry->flags |= TLBFlags::PTE_SET_C;

            // don't forget to update the secondary TLB as well
            tlb2_entry = lookup_secondary_tlb<TLBType::DTLB>(guest_va, tag);
            if (tlb2_entry != nullptr) {
                tlb2_entry->flags |= TLBFlags::PTE_SET_C;
            }
        }
        host_va = (uint8_t *)(tlb1_entry->host_va_offs_w + guest_va);
    } else {
        // primary TLB miss -> look up address in the secondary TLB
        tlb2_entry = lookup_secondary_tlb<TLBType::DTLB>(guest_va, tag);
        if (tlb2_entry == nullptr) {
#ifdef TLB_PROFILING
            num_dtlb_refills++;
#endif
            // secondary TLB miss ->
            // perform full address translation and refill the secondary TLB
            tlb2_entry = dtlb2_refill(guest_va, 1);
            if (tlb2_entry->flags & PAGE_NOPHYS) {
                return;
            }
        }
#ifdef TLB_PROFILING
        else {
            num_secondary_dtlb_hits++;
        }
#endif

        if (!(tlb2_entry->flags & TLBFlags::PAGE_WRITABLE)) {
            ppc_state.spr[SPR::DSISR] = 0x08000000 | (1 << 25);
            ppc_state.spr[SPR::DAR]   = guest_va;
            mmu_exception_handler(Except_Type::EXC_DSI, 0);
        }

        if (!(tlb2_entry->flags & TLBFlags::PTE_SET_C)) {
            // perform full page address translation to update PTE.C bit
            page_address_translation(guest_va, false, !!(ppc_state.msr & MSR::PR), true);
            tlb2_entry->flags |= TLBFlags::PTE_SET_C;
        }

        if (tlb2_entry->flags & TLBFlags::PAGE_MEM) { // is it a real memory region?
            // refill the primary TLB
            *tlb1_entry = *tlb2_entry;
            host_va = (uint8_t *)(tlb1_entry->host_va_offs_w + guest_va);
        } else { // otherwise, it's an access to a memory-mapped device
#ifdef MMU_PROFILING
            iomem_writes_total++;
#endif
            if (sizeof(T) == 8) {
                if (guest_va & 3)
                    ppc_alignment_exception(guest_va);

                tlb2_entry->rgn_desc->devobj->write(tlb2_entry->rgn_desc->start,
                                                    static_cast<uint32_t>(guest_va - tlb2_entry->dev_base_va),
                                                    value >> 32, 4);
                tlb2_entry->rgn_desc->devobj->write(tlb2_entry->rgn_desc->start,
                                                    static_cast<uint32_t>(guest_va + 4 - tlb2_entry->dev_base_va),
                                                    (uint32_t)value, 4);
            } else {
                tlb2_entry->rgn_desc->devobj->write(tlb2_entry->rgn_desc->start,
                                                    static_cast<uint32_t>(guest_va - tlb2_entry->dev_base_va),
                                                    value, sizeof(T));
            }
            return;
        }
    }

#ifdef MMU_PROFILING
    dmem_writes_total++;
#endif

    // handle unaligned memory accesses
    if (sizeof(T) > 1 && (guest_va & (sizeof(T) - 1))) {
        write_unaligned<T>(guest_va, host_va, value);
        return;
    }

    // handle aligned memory accesses
    switch(sizeof(T)) {
        case 1:
            *host_va = value;
            break;
        case 2:
            WRITE_WORD_BE_A(host_va, value);
            break;
        case 4:
            WRITE_DWORD_BE_A(host_va, value);
            break;
        case 8:
            WRITE_QWORD_BE_A(host_va, value);
            break;
    }
}

// explicitely instantiate all required mmu_write_vmem variants
template void mmu_write_vmem<uint8_t>(uint32_t guest_va,   uint8_t value);
template void mmu_write_vmem<uint16_t>(uint32_t guest_va, uint16_t value);
template void mmu_write_vmem<uint32_t>(uint32_t guest_va, uint32_t value);
template void mmu_write_vmem<uint64_t>(uint32_t guest_va, uint64_t value);

template <class T>
static T read_unaligned(uint32_t guest_va, uint8_t *host_va)
{
    if ((sizeof(T) == 8) && (guest_va & 3)) {
#ifndef PPC_TESTS
        ppc_alignment_exception(guest_va);
#endif
    }

    T result = 0;

    // is it a misaligned cross-page read?
    if ((sizeof(T) > 1) && ((guest_va & 0xFFF) + sizeof(T)) > 0x1000) {
#ifdef MMU_PROFILING
        unaligned_crossp_r++;
#endif
        // Break such a memory access into multiple, bytewise accesses.
        // Because such accesses suffer a performance penalty, they will be
        // presumably very rare so don't waste time optimizing the code below.
        for (int i = 0; i < sizeof(T); guest_va++, i++) {
            result = (result << 8) | mmu_read_vmem<uint8_t>(guest_va);
        }
    } else {
#ifdef MMU_PROFILING
        unaligned_reads++;
#endif
        switch(sizeof(T)) {
            case 1:
                return *host_va;
            case 2:
                return READ_WORD_BE_U(host_va);
            case 4:
                return READ_DWORD_BE_U(host_va);
            case 8:
                return READ_QWORD_BE_U(host_va);
        }
    }
    return result;
}

// explicitely instantiate all required read_unaligned variants
template uint16_t read_unaligned<uint16_t>(uint32_t guest_va, uint8_t *host_va);
template uint32_t read_unaligned<uint32_t>(uint32_t guest_va, uint8_t *host_va);
template uint64_t read_unaligned<uint64_t>(uint32_t guest_va, uint8_t *host_va);

template <class T>
static void write_unaligned(uint32_t guest_va, uint8_t *host_va, T value)
{
    if ((sizeof(T) == 8) && (guest_va & 3)) {
#ifndef PPC_TESTS
        ppc_alignment_exception(guest_va);
#endif
    }

    // is it a misaligned cross-page write?
    if ((sizeof(T) > 1) && ((guest_va & 0xFFF) + sizeof(T)) > 0x1000) {
#ifdef MMU_PROFILING
        unaligned_crossp_w++;
#endif
        // Break such a memory access into multiple, bytewise accesses.
        // Because such accesses suffer a performance penalty, they will be
        // presumably very rare so don't waste time optimizing the code below.

        uint32_t shift = (sizeof(T) - 1) * 8;

        for (int i = 0; i < sizeof(T); shift -= 8, guest_va++, i++) {
            mmu_write_vmem<uint8_t>(guest_va, (value >> shift) & 0xFF);
        }
    } else {
#ifdef MMU_PROFILING
        unaligned_writes++;
#endif
        switch(sizeof(T)) {
            case 1:
                *host_va = value;
                break;
            case 2:
                WRITE_WORD_BE_U(host_va, value);
                break;
            case 4:
                WRITE_DWORD_BE_U(host_va, value);
                break;
            case 8:
                WRITE_QWORD_BE_U(host_va, value);
                break;
        }
    }
}

// explicitely instantiate all required write_unaligned variants
template void write_unaligned<uint16_t>(uint32_t guest_va, uint8_t *host_va, uint16_t value);
template void write_unaligned<uint32_t>(uint32_t guest_va, uint8_t *host_va, uint32_t value);
template void write_unaligned<uint64_t>(uint32_t guest_va, uint8_t *host_va, uint64_t value);


/* MMU profiling. */
#ifdef MMU_PROFILING

#include "utils/profiler.h"
#include <memory>

class MMUProfile : public BaseProfile {
public:
    MMUProfile() : BaseProfile("PPC_MMU") {};

    void populate_variables(std::vector<ProfileVar>& vars) {
        vars.clear();

        vars.push_back({.name = "Data Memory Reads Total",
                        .format = ProfileVarFmt::DEC,
                        .value = dmem_reads_total});

        vars.push_back({.name = "I/O Memory Reads Total",
                        .format = ProfileVarFmt::DEC,
                        .value = iomem_reads_total});

        vars.push_back({.name = "Data Memory Writes Total",
                        .format = ProfileVarFmt::DEC,
                        .value = dmem_writes_total});

        vars.push_back({.name = "I/O Memory Writes Total",
                        .format = ProfileVarFmt::DEC,
                        .value = iomem_writes_total});

        vars.push_back({.name = "Reads from Executable Memory",
                        .format = ProfileVarFmt::DEC,
                        .value = exec_reads_total});

        vars.push_back({.name = "BAT Translations Total",
                        .format = ProfileVarFmt::DEC,
                        .value = bat_transl_total});

        vars.push_back({.name = "Page Table Translations Total",
                        .format = ProfileVarFmt::DEC,
                        .value = ptab_transl_total});

        vars.push_back({.name = "Unaligned Reads Total",
                        .format = ProfileVarFmt::DEC,
                        .value = unaligned_reads});

        vars.push_back({.name = "Unaligned Writes Total",
                        .format = ProfileVarFmt::DEC,
                        .value = unaligned_writes});

        vars.push_back({.name = "Unaligned Crosspage Reads Total",
                        .format = ProfileVarFmt::DEC,
                        .value = unaligned_crossp_r});

        vars.push_back({.name = "Unaligned Crosspage Writes Total",
                        .format = ProfileVarFmt::DEC,
                        .value = unaligned_crossp_w});
    };

    void reset() {
        dmem_reads_total   = 0;
        iomem_reads_total  = 0;
        dmem_writes_total  = 0;
        iomem_writes_total = 0;
        exec_reads_total   = 0;
        bat_transl_total   = 0;
        ptab_transl_total  = 0;
        unaligned_reads    = 0;
        unaligned_writes   = 0;
        unaligned_crossp_r = 0;
        unaligned_crossp_w = 0;
    };
};
#endif

/* SoftTLB profiling. */
#ifdef TLB_PROFILING

#include "utils/profiler.h"
#include <memory>

class TLBProfile : public BaseProfile {
public:
    TLBProfile() : BaseProfile("PPC:MMU:TLB") {};

    void populate_variables(std::vector<ProfileVar>& vars) {
        vars.clear();

        vars.push_back({.name = "Number of hits in the primary ITLB",
            .format = ProfileVarFmt::DEC,
            .value = num_primary_itlb_hits});

        vars.push_back({.name = "Number of hits in the secondary ITLB",
            .format = ProfileVarFmt::DEC,
            .value = num_secondary_itlb_hits});

        vars.push_back({.name = "Number of ITLB refills",
            .format = ProfileVarFmt::DEC,
            .value = num_itlb_refills});

        vars.push_back({.name = "Number of hits in the primary DTLB",
            .format = ProfileVarFmt::DEC,
            .value = num_primary_dtlb_hits});

        vars.push_back({.name = "Number of hits in the secondary DTLB",
            .format = ProfileVarFmt::DEC,
            .value = num_secondary_dtlb_hits});

        vars.push_back({.name = "Number of DTLB refills",
            .format = ProfileVarFmt::DEC,
            .value = num_dtlb_refills});

        vars.push_back({.name = "Number of replaced TLB entries",
            .format = ProfileVarFmt::DEC,
            .value = num_entry_replacements});
    };

    void reset() {
        num_primary_dtlb_hits   = 0;
        num_secondary_dtlb_hits = 0;
        num_dtlb_refills        = 0;
        num_entry_replacements = 0;
    };
};
#endif

uint64_t mem_read_dbg(uint32_t virt_addr, uint32_t size) {
    uint32_t save_dsisr, save_dar;
    uint64_t ret_val;

    /* save MMU-related CPU state */
    save_dsisr            = ppc_state.spr[SPR::DSISR];
    save_dar              = ppc_state.spr[SPR::DAR];
    mmu_exception_handler = dbg_exception_handler;

    try {
        switch (size) {
        case 1:
            ret_val = mmu_read_vmem<uint8_t>(virt_addr);
            break;
        case 2:
            ret_val = mmu_read_vmem<uint16_t>(virt_addr);
            break;
        case 4:
            ret_val = mmu_read_vmem<uint32_t>(virt_addr);
            break;
        case 8:
            ret_val = mmu_read_vmem<uint64_t>(virt_addr);
            break;
        default:
            ret_val = mmu_read_vmem<uint8_t>(virt_addr);
        }
    } catch (std::invalid_argument& exc) {
        /* restore MMU-related CPU state */
        mmu_exception_handler     = ppc_exception_handler;
        ppc_state.spr[SPR::DSISR] = save_dsisr;
        ppc_state.spr[SPR::DAR]   = save_dar;

        /* rethrow MMU exception */
        throw exc;
    }

    /* restore MMU-related CPU state */
    mmu_exception_handler     = ppc_exception_handler;
    ppc_state.spr[SPR::DSISR] = save_dsisr;
    ppc_state.spr[SPR::DAR]   = save_dar;

    return ret_val;
}

bool mmu_translate_dbg(uint32_t guest_va, uint32_t &guest_pa) {
    uint32_t save_dsisr, save_dar;
    bool is_mapped;

    /* save MMU-related CPU state */
    save_dsisr            = ppc_state.spr[SPR::DSISR];
    save_dar              = ppc_state.spr[SPR::DAR];
    mmu_exception_handler = dbg_exception_handler;

    try {
        TLBEntry *tlb1_entry, *tlb2_entry;

        const uint32_t tag = guest_va & ~0xFFFUL;

        // look up guest virtual address in the primary TLB
        tlb1_entry = &pCurDTLB1[(guest_va >> PPC_PAGE_SIZE_BITS) & tlb_size_mask];

        do {
            if (tlb1_entry->tag != tag) {
                // primary TLB miss -> look up address in the secondary TLB
                tlb2_entry = lookup_secondary_tlb<TLBType::DTLB>(guest_va, tag);
                if (tlb2_entry == nullptr) {
                    // secondary TLB miss ->
                    // perform full address translation and refill the secondary TLB
                    tlb2_entry = dtlb2_refill(guest_va, 0, true);
                    if (tlb2_entry->flags & PAGE_NOPHYS) {
                        is_mapped = false;
                        break;
                    }
                }

                if (tlb2_entry->flags & TLBFlags::PAGE_MEM) { // is it a real memory region?
                    // refill the primary TLB
                    *tlb1_entry = *tlb2_entry;
                }
                else {
                    tlb1_entry = tlb2_entry;
                }
            }
            guest_pa = tlb1_entry->phys_tag | (guest_va & 0xFFFUL);
            is_mapped = true;
        } while (0);
    } catch (std::invalid_argument& exc) {
        LOG_F(WARNING, "Unmapped address 0x%08X", guest_va);
        is_mapped = false;
    }

    /* restore MMU-related CPU state */
    mmu_exception_handler     = ppc_exception_handler;
    ppc_state.spr[SPR::DSISR] = save_dsisr;
    ppc_state.spr[SPR::DAR]   = save_dar;

    return is_mapped;
}

template <std::size_t N>
static void invalidate_tlb_entries(std::array<TLBEntry, N> &tlb) {
    for (auto &tlb_el : tlb) {
        tlb_el.tag = TLB_INVALID_TAG;
        tlb_el.flags = 0;
        tlb_el.lru_bits = 0;
        tlb_el.host_va_offs_r = 0;
        tlb_el.host_va_offs_w = 0;
        tlb_el.phys_tag = 0;
        tlb_el.reserved = 0;
    }
}

void ppc_mmu_init()
{
    last_read_area  = {0xFFFFFFFF, 0xFFFFFFFF, 0, 0, nullptr, nullptr};
    last_write_area = {0xFFFFFFFF, 0xFFFFFFFF, 0, 0, nullptr, nullptr};
    last_exec_area  = {0xFFFFFFFF, 0xFFFFFFFF, 0, 0, nullptr, nullptr};
    last_ptab_area  = {0xFFFFFFFF, 0xFFFFFFFF, 0, 0, nullptr, nullptr};

    mmu_exception_handler = ppc_exception_handler;

    if (is_601) {
        // use 601-style unified BATs
        ibat_update = &mpc601_bat_update;
    } else {
        // use PPC-style BATs
        ibat_update = &ppc_ibat_update;
        dbat_update = &ppc_dbat_update;
    }

    // invalidate all IDTLB entries
    invalidate_tlb_entries(itlb1_mode1);
    invalidate_tlb_entries(itlb1_mode2);
    invalidate_tlb_entries(itlb1_mode3);
    invalidate_tlb_entries(itlb2_mode1);
    invalidate_tlb_entries(itlb2_mode2);
    invalidate_tlb_entries(itlb2_mode3);
    // invalidate all DTLB entries
    invalidate_tlb_entries(dtlb1_mode1);
    invalidate_tlb_entries(dtlb1_mode2);
    invalidate_tlb_entries(dtlb1_mode3);
    invalidate_tlb_entries(dtlb2_mode1);
    invalidate_tlb_entries(dtlb2_mode2);
    invalidate_tlb_entries(dtlb2_mode3);

    mmu_change_mode();

#ifdef MMU_PROFILING
    gProfilerObj->register_profile("PPC:MMU",
        std::unique_ptr<BaseProfile>(new MMUProfile()));
#endif

#ifdef TLB_PROFILING
    gProfilerObj->register_profile("PPC:MMU:TLB",
    std::unique_ptr<BaseProfile>(new TLBProfile()));
#endif
}
