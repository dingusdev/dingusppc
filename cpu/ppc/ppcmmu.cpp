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

/** @file PowerPC Memory management unit emulation. */

/* TODO:
    - implement TLB
    - implement 601-style BATs
    - add proper error and exception handling
    - clarify what to do in the case of unaligned memory accesses
 */

#include <thirdparty/loguru/loguru.hpp>
#include <iostream>
#include <cstdint>
#include <cinttypes>
#include <string>
#include <stdexcept>
#include <array>
#include "memreadwrite.h"
#include "ppcemu.h"
#include "ppcmmu.h"
#include "devices/memctrlbase.h"

/* pointer to exception handler to be called when a MMU exception is occured. */
void (*mmu_exception_handler)(Except_Type exception_type, uint32_t srr1_bits);

/** PowerPC-style MMU BAT arrays (NULL initialization isn't prescribed). */
PPC_BAT_entry ibat_array[4] = { {0} };
PPC_BAT_entry dbat_array[4] = { {0} };

/** remember recently used physical memory regions for quicker translation. */
AddressMapEntry last_read_area  = { 0 };
AddressMapEntry last_write_area = { 0 };
AddressMapEntry last_exec_area  = { 0 };
AddressMapEntry last_ptab_area  = { 0 };
AddressMapEntry last_dma_area   = { 0 };


/* macro for generating code reading from physical memory */
#define READ_PHYS_MEM(ENTRY, ADDR, OP, SIZE, UNVAL)                            \
{                                                                              \
    if ((ADDR) >= (ENTRY).start && ((ADDR) + (SIZE)) <= (ENTRY).end) {         \
        ret = OP((ENTRY).mem_ptr + ((ADDR) - (ENTRY).start));                  \
    } else {                                                                   \
        AddressMapEntry* entry = mem_ctrl_instance->find_range((ADDR));        \
        if (entry) {                                                           \
            if (entry->type & (RT_ROM | RT_RAM)) {                             \
                (ENTRY).start   = entry->start;                                \
                (ENTRY).end     = entry->end;                                  \
                (ENTRY).mem_ptr = entry->mem_ptr;                              \
                ret = OP((ENTRY).mem_ptr + ((ADDR) - (ENTRY).start));          \
            }                                                                  \
            else if (entry->type & RT_MMIO) {                                  \
                ret = entry->devobj->read(entry->start, (ADDR) - entry->start, \
                    (SIZE));                                                   \
            }                                                                  \
            else {                                                             \
                LOG_F(ERROR, "Please check your address map! \n");             \
                ret = (UNVAL);                                                 \
            }                                                                  \
        }                                                                      \
        else {                                                                 \
            LOG_F(WARNING, "Read from unmapped memory at 0x%08X!\n", (ADDR));  \
            ret = (UNVAL);                                                     \
        }                                                                      \
    }                                                                          \
}

/* macro for generating code writing to physical memory */
#define WRITE_PHYS_MEM(ENTRY, ADDR, OP, VAL, SIZE)                             \
{                                                                              \
    if ((ADDR) >= (ENTRY).start && ((ADDR) + (SIZE)) <= (ENTRY).end) {         \
        OP((ENTRY).mem_ptr + ((ADDR) - (ENTRY).start), (VAL));                 \
    } else {                                                                   \
        AddressMapEntry* entry = mem_ctrl_instance->find_range((ADDR));        \
        if (entry) {                                                           \
            if (entry->type & RT_RAM) {                                        \
                (ENTRY).start   = entry->start;                                \
                (ENTRY).end     = entry->end;                                  \
                (ENTRY).mem_ptr = entry->mem_ptr;                              \
                OP((ENTRY).mem_ptr + ((ADDR) - (ENTRY).start), (VAL));         \
            }                                                                  \
            else if (entry->type & RT_MMIO) {                                  \
                entry->devobj->write(entry->start, (ADDR) - entry->start,      \
                    (VAL), (SIZE));                                            \
            }                                                                  \
            else {                                                             \
                LOG_F(ERROR, "Please check your address map!\n");              \
            }                                                                  \
        }                                                                      \
        else {                                                                 \
            LOG_F(WARNING, "Write to unmapped memory at 0x%08X!\n", (ADDR));   \
        }                                                                      \
    }                                                                          \
}

uint8_t *mmu_get_dma_mem(uint32_t addr, uint32_t size)
{
    if (addr >= last_dma_area.start && (addr + size) <= last_dma_area.end) {
        return last_dma_area.mem_ptr + (addr - last_dma_area.start);
    }
    else {
        AddressMapEntry* entry = mem_ctrl_instance->find_range(addr);
        if (entry && entry->type & (RT_ROM | RT_RAM)) {
            last_dma_area.start   = entry->start;
            last_dma_area.end     = entry->end;
            last_dma_area.mem_ptr = entry->mem_ptr;
            return last_dma_area.mem_ptr + (addr - last_dma_area.start);
        }
        else {
            LOG_F(ERROR, "SOS: DMA access to unmapped memory %08X!\n", addr);
            exit(-1); // FIXME: ugly error handling, must be the proper exception!
        }
    }
}

void ppc_set_cur_instruction(const uint8_t* ptr)
{
    ppc_cur_instruction = READ_DWORD_BE_A(ptr);
}

void ibat_update(uint32_t bat_reg)
{
    int upper_reg_num;
    uint32_t bl, lo_mask;
    PPC_BAT_entry* bat_entry;

    upper_reg_num = bat_reg & 0xFFFFFFFE;

    if (ppc_state.spr[upper_reg_num] & 3) { // is that BAT pair valid?
        bat_entry = &ibat_array[(bat_reg - 528) >> 1];
        bl = (ppc_state.spr[upper_reg_num] >> 2) & 0x7FF;
        lo_mask = (bl << 17) | 0x1FFFF;

        bat_entry->access = ppc_state.spr[upper_reg_num] & 3;
        bat_entry->prot = ppc_state.spr[upper_reg_num + 1] & 3;
        bat_entry->lo_mask = lo_mask;
        bat_entry->phys_hi = ppc_state.spr[upper_reg_num + 1] & ~lo_mask;
        bat_entry->bepi = ppc_state.spr[upper_reg_num] & ~lo_mask;
    }
}

void dbat_update(uint32_t bat_reg)
{
    int upper_reg_num;
    uint32_t bl, lo_mask;
    PPC_BAT_entry* bat_entry;

    upper_reg_num = bat_reg & 0xFFFFFFFE;

    if (ppc_state.spr[upper_reg_num] & 3) { // is that BAT pair valid?
        bat_entry = &dbat_array[(bat_reg - 536) >> 1];
        bl = (ppc_state.spr[upper_reg_num] >> 2) & 0x7FF;
        lo_mask = (bl << 17) | 0x1FFFF;

        bat_entry->access = ppc_state.spr[upper_reg_num] & 3;
        bat_entry->prot = ppc_state.spr[upper_reg_num + 1] & 3;
        bat_entry->lo_mask = lo_mask;
        bat_entry->phys_hi = ppc_state.spr[upper_reg_num + 1] & ~lo_mask;
        bat_entry->bepi = ppc_state.spr[upper_reg_num] & ~lo_mask;
    }
}

static inline uint8_t* calc_pteg_addr(uint32_t hash)
{
    uint32_t sdr1_val, pteg_addr;

    sdr1_val = ppc_state.spr[SPR::SDR1];

    pteg_addr = sdr1_val & 0xFE000000;
    pteg_addr |= (sdr1_val & 0x01FF0000) |
        (((sdr1_val & 0x1FF) << 16) & ((hash & 0x7FC00) << 6));
    pteg_addr |= (hash & 0x3FF) << 6;

    if (pteg_addr >= last_ptab_area.start && pteg_addr <= last_ptab_area.end) {
        return last_ptab_area.mem_ptr + (pteg_addr - last_ptab_area.start);
    }
    else {
        AddressMapEntry* entry = mem_ctrl_instance->find_range(pteg_addr);
        if (entry && entry->type & (RT_ROM | RT_RAM)) {
            last_ptab_area.start   = entry->start;
            last_ptab_area.end     = entry->end;
            last_ptab_area.mem_ptr = entry->mem_ptr;
            return last_ptab_area.mem_ptr + (pteg_addr - last_ptab_area.start);
        }
        else {
            LOG_F(ERROR, "SOS: no page table region was found at %08X!\n", pteg_addr);
            exit(-1); // FIXME: ugly error handling, must be the proper exception!
        }
    }
}

static bool search_pteg(uint8_t* pteg_addr, uint8_t** ret_pte_addr,
    uint32_t vsid, uint16_t page_index, uint8_t pteg_num)
{
    /* construct PTE matching word */
    uint32_t pte_check = 0x80000000 | (vsid << 7) | (pteg_num << 6) |
        (page_index >> 10);

#ifdef MMU_INTEGRITY_CHECKS
    /* PTEG integrity check that ensures that all matching PTEs have
       identical RPN, WIMG and PP bits (PPC PEM 32-bit 7.6.2, rule 5). */
    uint32_t pte_word2_check;
    bool match_found = false;

    for (int i = 0; i < 8; i++, pteg_addr += 8) {
        if (pte_check == READ_DWORD_BE_A(pteg_addr)) {
            if (match_found) {
                if ((READ_DWORD_BE_A(pteg_addr) & 0xFFFFF07B) != pte_word2_check) {
                    LOG_F(ERROR, "Multiple PTEs with different RPN/WIMG/PP found!\n");
                    exit(-1);
                }
            }
            else {
                /* isolate RPN, WIMG and PP fields */
                pte_word2_check = READ_DWORD_BE_A(pteg_addr) & 0xFFFFF07B;
                *ret_pte_addr = pteg_addr;
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

static uint32_t page_address_translate(uint32_t la, bool is_instr_fetch,
    unsigned msr_pr, int is_write)
{
    uint32_t sr_val, page_index, pteg_hash1, vsid, pte_word2;
    unsigned key, pp;
    uint8_t* pte_addr;

    sr_val = ppc_state.sr[(la >> 28) & 0x0F];
    if (sr_val & 0x80000000) {
        LOG_F(ERROR, "Direct-store segments not supported, LA=%0xX\n", la);
        exit(-1); // FIXME: ugly error handling, must be the proper exception!
    }

    /* instruction fetch from a no-execute segment will cause ISI exception */
    if ((sr_val & 0x10000000) && is_instr_fetch) {
        mmu_exception_handler(Except_Type::EXC_ISI, 0x10000000);
    }

    page_index = (la >> 12) & 0xFFFF;
    pteg_hash1 = (sr_val & 0x7FFFF) ^ page_index;
    vsid = sr_val & 0x0FFFFFF;

    if (!search_pteg(calc_pteg_addr(pteg_hash1), &pte_addr, vsid, page_index, 0)) {
        if (!search_pteg(calc_pteg_addr(~pteg_hash1), &pte_addr, vsid, page_index, 1)) {
            if (is_instr_fetch) {
                mmu_exception_handler(Except_Type::EXC_ISI, 0x40000000);
            }
            else {
                ppc_state.spr[SPR::DSISR] = 0x40000000 | (is_write << 25);
                ppc_state.spr[SPR::DAR] = la;
                mmu_exception_handler(Except_Type::EXC_DSI, 0);
            }
        }
    }

    pte_word2 = READ_DWORD_BE_A(pte_addr + 4);

    key = (((sr_val >> 29) & 1)& msr_pr) | (((sr_val >> 30) & 1)& (msr_pr ^ 1));

    /* check page access */
    pp = pte_word2 & 3;

    // the following scenarios cause DSI/ISI exception:
    // any access with key = 1 and PP = %00
    // write access with key = 1 and PP = %01
    // write access with PP = %11
    if ((key && (!pp || (pp == 1 && is_write))) || (pp == 3 && is_write)) {
        if (is_instr_fetch) {
            mmu_exception_handler(Except_Type::EXC_ISI, 0x08000000);
        }
        else {
            ppc_state.spr[SPR::DSISR] = 0x08000000 | (is_write << 25);
            ppc_state.spr[SPR::DAR] = la;
            mmu_exception_handler(Except_Type::EXC_DSI, 0);
        }
    }

    /* update R and C bits */
    /* For simplicity, R is set on each access, C is set only for writes */
    pte_addr[6] |= 0x01;
    if (is_write) {
        pte_addr[7] |= 0x80;
    }

    /* return physical address */
    return ((pte_word2 & 0xFFFFF000) | (la & 0x00000FFF));
}

/** PowerPC-style MMU instruction address translation. */
static uint32_t ppc_mmu_instr_translate(uint32_t la)
{
    uint32_t pa; /* translated physical address */

    bool bat_hit = false;
    unsigned msr_pr = !!(ppc_state.msr & 0x4000);

    // Format: %XY
    // X - supervisor access bit, Y - problem/user access bit
    // Those bits are mutually exclusive
    unsigned access_bits = (~msr_pr << 1) | msr_pr;

    for (int bat_index = 0; bat_index < 4; bat_index++) {
        PPC_BAT_entry* bat_entry = &ibat_array[bat_index];

        if ((bat_entry->access & access_bits) &&
            ((la & ~bat_entry->lo_mask) == bat_entry->bepi)) {
            bat_hit = true;

            if (!bat_entry->prot) {
                mmu_exception_handler(Except_Type::EXC_ISI, 0x08000000);
            }

            // logical to physical translation
            pa = bat_entry->phys_hi | (la & bat_entry->lo_mask);
            break;
        }
    }

    /* page address translation */
    if (!bat_hit) {
        pa = page_address_translate(la, true, msr_pr, 0);
    }

    return pa;
}

/** PowerPC-style MMU data address translation. */
static uint32_t ppc_mmu_addr_translate(uint32_t la, int is_write)
{
#ifdef PROFILER
    mmu_translations_num++;
#endif

    uint32_t pa; /* translated physical address */

    bool bat_hit = false;
    unsigned msr_pr = !!(ppc_state.msr & 0x4000);

    // Format: %XY
    // X - supervisor access bit, Y - problem/user access bit
    // Those bits are mutually exclusive
    unsigned access_bits = (~msr_pr << 1) | msr_pr;

    for (int bat_index = 0; bat_index < 4; bat_index++) {
        PPC_BAT_entry* bat_entry = &dbat_array[bat_index];

        if ((bat_entry->access & access_bits) &&
            ((la & ~bat_entry->lo_mask) == bat_entry->bepi)) {
            bat_hit = true;

            if (!bat_entry->prot || ((bat_entry->prot & 1) && is_write)) {
                ppc_state.spr[SPR::DSISR] = 0x08000000 | (is_write << 25);
                ppc_state.spr[SPR::DAR] = la;
                mmu_exception_handler(Except_Type::EXC_DSI, 0);
            }

            // logical to physical translation
            pa = bat_entry->phys_hi | (la & bat_entry->lo_mask);
            break;
        }
    }

    /* page address translation */
    if (!bat_hit) {
        pa = page_address_translate(la, false, msr_pr, is_write);
    }

    return pa;
}

static void mem_write_unaligned(uint32_t addr, uint32_t value, uint32_t size)
{
    LOG_F(WARNING, "Attempt to write unaligned %d bytes to 0x%08X\n", size, addr);

    if (((addr & 0xFFF) + size) > 0x1000) {
        LOG_F(ERROR, "SOS! Cross-page unaligned write, addr=%08X, size=%d\n", addr, size);
        exit(-1); //FIXME!
    } else {
        /* data address translation if enabled */
        if (ppc_state.msr & 0x10) {
            addr = ppc_mmu_addr_translate(addr, 0);
        }

        if (size == 2) {
            WRITE_PHYS_MEM(last_write_area, addr, WRITE_WORD_BE_U, value, 2);
        } else {
            WRITE_PHYS_MEM(last_write_area, addr, WRITE_DWORD_BE_U, value, 4);
        }
    }
}

void mem_write_byte(uint32_t addr, uint8_t value)
{
    /* data address translation if enabled */
    if (ppc_state.msr & 0x10) {
        addr = ppc_mmu_addr_translate(addr, 1);
    }

    #define WRITE_BYTE(addr, val) (*(addr) = val)

    WRITE_PHYS_MEM(last_write_area, addr, WRITE_BYTE, value, 1);
}

void mem_write_word(uint32_t addr, uint16_t value)
{
    if (addr & 1) {
        mem_write_unaligned(addr, value, 2);
    }

    /* data address translation if enabled */
    if (ppc_state.msr & 0x10) {
        addr = ppc_mmu_addr_translate(addr, 1);
    }

    WRITE_PHYS_MEM(last_write_area, addr, WRITE_WORD_BE_A, value, 2);
}

void mem_write_dword(uint32_t addr, uint32_t value)
{
    if (addr & 3) {
        mem_write_unaligned(addr, value, 4);
    }

    /* data address translation if enabled */
    if (ppc_state.msr & 0x10) {
        addr = ppc_mmu_addr_translate(addr, 1);
    }

    WRITE_PHYS_MEM(last_write_area, addr, WRITE_DWORD_BE_A, value, 4);
}

void mem_write_qword(uint32_t addr, uint64_t value)
{
    if (addr & 7) {
        LOG_F(ERROR, "SOS! Attempt to write unaligned QWORD to 0x%08X\n", addr);
        exit(-1); //FIXME!
    }

    /* data address translation if enabled */
    if (ppc_state.msr & 0x10) {
        addr = ppc_mmu_addr_translate(addr, 1);
    }

    WRITE_PHYS_MEM(last_write_area, addr, WRITE_QWORD_BE_A, value, 8);
}

static uint32_t mem_grab_unaligned(uint32_t addr, uint32_t size)
{
    uint32_t ret = 0;

    LOG_F(WARNING, "Attempt to read unaligned %d bytes from 0x%08X\n", size, addr);

    if (((addr & 0xFFF) + size) > 0x1000) {
        LOG_F(ERROR, "SOS! Cross-page unaligned read, addr=%08X, size=%d\n", addr, size);
        exit(-1); //FIXME!
    } else {
        /* data address translation if enabled */
        if (ppc_state.msr & 0x10) {
            addr = ppc_mmu_addr_translate(addr, 0);
        }

        if (size == 2) {
            READ_PHYS_MEM(last_read_area, addr, READ_WORD_BE_U, 2, 0xFFFFU);
        } else {
            READ_PHYS_MEM(last_read_area, addr, READ_DWORD_BE_U, 4, 0xFFFFFFFFUL);
        }
    }

    return ret;
}

/** Grab a value from memory into a register */
uint8_t mem_grab_byte(uint32_t addr)
{
    uint8_t ret;

    /* data address translation if enabled */
    if (ppc_state.msr & 0x10) {
        addr = ppc_mmu_addr_translate(addr, 0);
    }

    READ_PHYS_MEM(last_read_area, addr, *, 1, 0xFFU);
    return ret;
}

uint16_t mem_grab_word(uint32_t addr)
{
    uint16_t ret;

    if (addr & 1) {
        return mem_grab_unaligned(addr, 2);
    }

    /* data address translation if enabled */
    if (ppc_state.msr & 0x10) {
        addr = ppc_mmu_addr_translate(addr, 0);
    }

    READ_PHYS_MEM(last_read_area, addr, READ_WORD_BE_A, 2, 0xFFFFU);
    return ret;
}

uint32_t mem_grab_dword(uint32_t addr)
{
    uint32_t ret;

    if (addr & 3) {
        return mem_grab_unaligned(addr, 4);
    }

    /* data address translation if enabled */
    if (ppc_state.msr & 0x10) {
        addr = ppc_mmu_addr_translate(addr, 0);
    }

    READ_PHYS_MEM(last_read_area, addr, READ_DWORD_BE_A, 4, 0xFFFFFFFFUL);
    return ret;
}

uint64_t mem_grab_qword(uint32_t addr)
{
    uint64_t ret;

    if (addr & 7) {
        LOG_F(ERROR, "SOS! Attempt to read unaligned QWORD at 0x%08X\n", addr);
        exit(-1); //FIXME!
    }

    /* data address translation if enabled */
    if (ppc_state.msr & 0x10) {
        addr = ppc_mmu_addr_translate(addr, 0);
    }

    READ_PHYS_MEM(last_read_area, addr, READ_QWORD_BE_A, 8, 0xFFFFFFFFFFFFFFFFULL);
    return ret;
}

uint8_t* quickinstruction_translate(uint32_t addr)
{
    uint8_t* real_addr;

    /* perform instruction address translation if enabled */
    if (ppc_state.msr & 0x20) {
        addr = ppc_mmu_instr_translate(addr);
    }

    if (addr >= last_exec_area.start && addr <= last_exec_area.end) {
        real_addr = last_exec_area.mem_ptr + (addr - last_exec_area.start);
        ppc_set_cur_instruction(real_addr);
    }
    else {
        AddressMapEntry* entry = mem_ctrl_instance->find_range(addr);
        if (entry && entry->type & (RT_ROM | RT_RAM)) {
            last_exec_area.start   = entry->start;
            last_exec_area.end     = entry->end;
            last_exec_area.mem_ptr = entry->mem_ptr;
            real_addr = last_exec_area.mem_ptr + (addr - last_exec_area.start);
            ppc_set_cur_instruction(real_addr);
        }
        else {
            LOG_F(WARNING, "attempt to execute code at %08X!\n", addr);
            exit(-1); // FIXME: ugly error handling, must be the proper exception!
        }
    }

    return real_addr;
}

uint64_t mem_read_dbg(uint32_t virt_addr, uint32_t size)
{
    uint32_t save_dsisr, save_dar;
    uint64_t ret_val;

    /* save MMU-related CPU state */
    save_dsisr = ppc_state.spr[SPR::DSISR];
    save_dar   = ppc_state.spr[SPR::DAR];
    mmu_exception_handler = dbg_exception_handler;

    try {
        switch(size) {
            case 1:
                ret_val = mem_grab_byte(virt_addr);
                break;
            case 2:
                ret_val = mem_grab_word(virt_addr);
                break;
            case 4:
                ret_val = mem_grab_dword(virt_addr);
                break;
            case 8:
                ret_val = mem_grab_qword(virt_addr);
                break;
            default:
                ret_val = mem_grab_byte(virt_addr);
        }
    }
    catch (std::invalid_argument& exc) {
        /* restore MMU-related CPU state */
        mmu_exception_handler = ppc_exception_handler;
        ppc_state.spr[SPR::DSISR] = save_dsisr;
        ppc_state.spr[SPR::DAR] = save_dar;

        /* rethrow MMU exception */
        throw exc;
    }

    /* restore MMU-related CPU state */
    mmu_exception_handler = ppc_exception_handler;
    ppc_state.spr[SPR::DSISR] = save_dsisr;
    ppc_state.spr[SPR::DAR] = save_dar;

    return ret_val;
}

void ppc_mmu_init()
{
    mmu_exception_handler = ppc_exception_handler;
}
