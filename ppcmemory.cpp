//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

// The memory operations - ppcmemory.cpp

#include <iostream>
#include <cstdint>
#include <cinttypes>
#include <string>
#include <array>
#include <thread>
#include <atomic>
#include "ppcemumain.h"
#include "ppcmemory.h"
#include "devices/memctrlbase.h"
#include "devices/mmiodevice.h"
#include "devices/mpc106.h"

std::vector<uint32_t> pte_storage;

uint32_t pte_word1;
uint32_t pte_word2;

uint32_t msr_ir_test;
uint32_t msr_dr_test;
uint32_t msr_ip_test;

uint32_t choose_sr;
uint32_t pteg_hash1;
uint32_t pteg_hash2;
uint32_t pteg_answer;

uint32_t pteg_address1;
uint32_t pteg_temp1;
uint32_t pteg_address2;
uint32_t pteg_temp2;

uint32_t pteg_check1;
uint32_t rev_pteg_check1;
uint32_t pteg_check2;
uint32_t rev_pteg_check2;

unsigned char * grab_tempmem_ptr1;
unsigned char * grab_tempmem_ptr2;
unsigned char * grab_tempmem_ptr3;
unsigned char * grab_tempmem_ptr4;

unsigned char * grab_macmem_ptr;
unsigned char * grab_pteg1_ptr;
unsigned char * grab_pteg2_ptr;

std::atomic<bool> hash_found (false);

/** PowerPC-style MMU BAT arrays (NULL initialization isn't prescribed). */
PPC_BAT_entry ibat_array[4] = {{0}};
PPC_BAT_entry dbat_array[4] = {{0}};


/**
Quickly map to memory - sort of.

0x00000000 - 0x7FFFFFFF - Macintosh system memory
(Because this emulator is trying to emulate a Mac with a Grackle motherboard,
 the most memory that can ever be allocated to the system is 2 Gigs.)

0x80000000 - 0xFF7FFFFF - PCI memory
This memory is allocated to things like the memory controller, video, and audio.

0xF3000000 - Mac OS I/O Device area
0xF3013000 - Serial Printer Port (0x20 bytes)
0xF3013020 - Serial Modem Port (0x20 bytes)
0xF3011000 - BMAC Ethernet (0x1000 bytes)
0xF3014000 - DAVAudio Sound Bus (0x1000 bytes)
0xF3015000 - Swim 3 Floppy Disk Drive (0x1000 bytes)
0xF3016000 - Cuda (0x2000 bytes)
0xF3020000 - Heathrow ATA (Hard Drive Interface)

0xFF800000 - 0xFFFFFFFF - ROM memory
This memory is for storing the ROM needed to boot up the computer.

This could definitely be refactored better - TODO
**/

void msr_status_update(){
    msr_ip_test = (ppc_state.ppc_msr >> 6) & 1;
    msr_ir_test = (ppc_state.ppc_msr >> 5) & 1;
    msr_dr_test = (ppc_state.ppc_msr >> 4) & 1;
}

static inline void ppc_set_cur_instruction(unsigned char *ptr, uint32_t offset)
{
    ppc_cur_instruction  = (ptr[offset]  << 24) | (ptr[offset+1] << 16) |
                           (ptr[offset+2] << 8) |  ptr[offset+3];
}

static inline void ppc_set_return_val(unsigned char *ptr, uint32_t offset,
                                      int num_size)
{
    //Put the final result in return_value here
    //This is what gets put back into the register

    if (ppc_state.ppc_msr & 1) { /* little-endian byte ordering */
        if (num_size == 1) { // BYTE
            return_value = ptr[offset];
        }
        else if (num_size == 2) { // WORD
            return_value = ptr[offset] | (ptr[offset+1] << 8);
        }
        else if (num_size == 4) { // DWORD
            return_value = ptr[offset] | (ptr[offset+1] << 8) |
                          (ptr[offset+2] << 16) | (ptr[offset+3] << 24);
        }
    } else { /* big-endian byte ordering */
        if (num_size == 1) { // BYTE
            return_value = ptr[offset];
        }
        else if (num_size == 2) { // WORD
            return_value = (ptr[offset] << 8) | ptr[offset+1];
        }
        else if (num_size == 4) { // DWORD
            return_value = (ptr[offset]  << 24) | (ptr[offset+1] << 16) |
                           (ptr[offset+2] << 8) | ptr[offset+3];
        }
    }
}

static inline void ppc_memstore_value(unsigned char *ptr, uint32_t value,
                                      uint32_t offset, int num_size)
{
    if (ppc_state.ppc_msr & 1) { /* little-endian byte ordering */
        if (num_size >= 1) { // BYTE
            ptr[offset] = value & 0xFF;
        }
        if (num_size >= 2) { // WORD
            ptr[offset+1] = (value >> 8) & 0xFF;
        }
        if (num_size == 4) { // DWORD
            ptr[offset+2] = (value >> 16) & 0xFF;
            ptr[offset+3] = (value >> 24) & 0xFF;
        }
    } else { /* big-endian byte ordering */
        if (num_size == 1) { // BYTE
            ptr[offset] = value & 0xFF;
        }
        else if (num_size == 2) { // WORD
            ptr[offset]   = (value >> 8) & 0xFF;
            ptr[offset+1] = value & 0xFF;
        }
        else if (num_size == 4) { // DWORD
            ptr[offset]   = (value >> 24) & 0xFF;
            ptr[offset+1] = (value >> 16) & 0xFF;
            ptr[offset+2] = (value >> 8)  & 0xFF;
            ptr[offset+3] = value & 0xFF;
        }
    }
}

void ibat_update(uint32_t bat_reg)
{
    int upper_reg_num;
    uint32_t bl, lo_mask;
    PPC_BAT_entry *bat_entry;

    upper_reg_num = bat_reg & 0xFFFFFFFE;

    if (ppc_state.ppc_spr[upper_reg_num] & 3) { // is that BAT pair valid?
        bat_entry = &ibat_array[(bat_reg - 528) >> 1];
        bl = (ppc_state.ppc_spr[upper_reg_num] >> 2) & 0x7FF;
        lo_mask = (bl << 17) | 0x1FFFF;

        bat_entry->access  = ppc_state.ppc_spr[upper_reg_num] & 3;
        bat_entry->prot    = ppc_state.ppc_spr[upper_reg_num + 1] & 3;
        bat_entry->lo_mask = lo_mask;
        bat_entry->phys_hi = ppc_state.ppc_spr[upper_reg_num + 1] & ~lo_mask;
        bat_entry->bepi    = ppc_state.ppc_spr[upper_reg_num] & ~lo_mask;
    }
}

void dbat_update(uint32_t bat_reg)
{
    int upper_reg_num;
    uint32_t bl, lo_mask;
    PPC_BAT_entry *bat_entry;

    upper_reg_num = bat_reg & 0xFFFFFFFE;

    if (ppc_state.ppc_spr[upper_reg_num] & 3) { // is that BAT pair valid?
        bat_entry = &dbat_array[(bat_reg - 536) >> 1];
        bl = (ppc_state.ppc_spr[upper_reg_num] >> 2) & 0x7FF;
        lo_mask = (bl << 17) | 0x1FFFF;

        bat_entry->access  = ppc_state.ppc_spr[upper_reg_num] & 3;
        bat_entry->prot    = ppc_state.ppc_spr[upper_reg_num + 1] & 3;
        bat_entry->lo_mask = lo_mask;
        bat_entry->phys_hi = ppc_state.ppc_spr[upper_reg_num + 1] & ~lo_mask;
        bat_entry->bepi    = ppc_state.ppc_spr[upper_reg_num] & ~lo_mask;
    }
}

void get_pointer_pteg1(uint32_t address_grab){
        //Grab the array pointer for the PTEG
    if (address_grab < 0x80000000){
        pte_word1 = address_grab % ram_size_set;
        if (address_grab < 0x040000000){ //for debug purposes
            grab_pteg1_ptr = machine_sysram_mem;
        }
        else if ((address_grab >= 0x5fffe000) && (address_grab <= 0x5fffffff)){
            pte_word1 = address_grab % 0x2000;
            grab_pteg1_ptr = machine_sysconfig_mem;
        }
        else{
            printf("Uncharted territory: %x \n", address_grab);
        }
    }
    else if (address_grab < 0x80800000){
        pte_word1 = address_grab % 0x800000;
        grab_pteg1_ptr = machine_upperiocontrol_mem;

    }
    else if (address_grab < 0x81000000){
        pte_word1 = address_grab % 0x800000;
        grab_pteg1_ptr = machine_iocontrolcdma_mem;

    }
    else if (address_grab < 0xBF80000){
        pte_word1 = address_grab % 33554432;
        grab_pteg1_ptr = machine_loweriocontrol_mem;

    }
    else if (address_grab < 0xC0000000){
        pte_word1 = address_grab % 16;
        grab_pteg1_ptr = machine_interruptack_mem;

    }
    else if (address_grab < 0xF0000000){
        printf("Invalid Memory Attempt: %x \n", address_grab);
        return;
    }
    else if (address_grab < 0xF8000000){
        pte_word1 = address_grab % 67108864;
        grab_pteg1_ptr = machine_iocontrolmem_mem;

    }
    else if (address_grab < rom_file_begin){
        //Get back to this! (weeny1)
        if (address_grab < 0xFE000000){
            pte_word1 = address_grab % 4096;
            grab_pteg1_ptr = machine_f8xxxx_mem;
        }
        else if (address_grab < 0xFEC00000){
            pte_word1 = address_grab % 65536;
            grab_pteg1_ptr = machine_fexxxx_mem;
        }
        else if (address_grab < 0xFEE00000){
            pte_word1 = 0x0CF8;  //CONFIG_ADDR
            grab_pteg1_ptr = machine_fecxxx_mem;
        }
        else if (address_grab < 0xFF000000){
            pte_word1 = 0x0CFC;  //CONFIG_DATA
            grab_pteg1_ptr = machine_feexxx_mem;
        }
        else if (address_grab < 0xFF800000){
            pte_word1 = address_grab % 4096;
            grab_pteg1_ptr = machine_ff00xx_mem;
        }
        else{
            pte_word1 = (address_grab % 1048576) + 0x400000;
            grab_pteg1_ptr = machine_sysram_mem;
        }
    }
    else{
        pte_word1 = address_grab % rom_filesize;
        grab_pteg1_ptr = machine_sysrom_mem;
    }
}

void get_pointer_pteg2(uint32_t address_grab){
        //Grab the array pointer for the PTEG
    if (address_grab < 0x80000000){
        pte_word2 = address_grab % ram_size_set;
        if (address_grab < 0x040000000){ //for debug purposes
            grab_pteg2_ptr = machine_sysram_mem;
        }
        else if ((address_grab >= 0x5fffe000) && (address_grab <= 0x5fffffff)){
            pte_word2 = address_grab % 0x2000;
            grab_pteg2_ptr = machine_sysconfig_mem;
        }
        else{
            printf("Uncharted territory: %x \n", address_grab);
        }
    }
    else if (address_grab < 0x80800000){
        pte_word2 = address_grab % 0x800000;
        grab_pteg2_ptr = machine_upperiocontrol_mem;

    }
    else if (address_grab < 0x81000000){
        pte_word2 = address_grab % 0x800000;
        grab_pteg2_ptr = machine_iocontrolcdma_mem;

    }
    else if (address_grab < 0xBF80000){
        pte_word2 = address_grab % 33554432;
        grab_pteg2_ptr = machine_loweriocontrol_mem;

    }
    else if (address_grab < 0xC0000000){
        pte_word2 = address_grab % 16;
        grab_pteg2_ptr = machine_interruptack_mem;

    }
    else if (address_grab < 0xF0000000){
        printf("Invalid Memory Attempt: %x \n", address_grab);
        return;
    }
    else if (address_grab < 0xF8000000){
        pte_word2 = address_grab % 67108864;
        grab_pteg2_ptr = machine_iocontrolmem_mem;

    }
    else if (address_grab < rom_file_begin){
        //Get back to this! (weeny1)
        if (address_grab < 0xFE000000){
            pte_word2 = address_grab % 4096;
            grab_pteg2_ptr = machine_f8xxxx_mem;
        }
        else if (address_grab < 0xFEC00000){
            pte_word2 = address_grab % 65536;
            grab_pteg2_ptr = machine_fexxxx_mem;
        }
        else if (address_grab < 0xFEE00000){
            pte_word2 = 0x0CF8;  //CONFIG_ADDR
            grab_pteg2_ptr = machine_fecxxx_mem;
        }
        else if (address_grab < 0xFF000000){
            pte_word2 = 0x0CFC;  //CONFIG_DATA
            grab_pteg2_ptr = machine_feexxx_mem;
        }
        else if (address_grab < 0xFF800000){
            pte_word2 = address_grab % 4096;
            grab_pteg2_ptr = machine_ff00xx_mem;
        }
        else{
            pte_word2 = (address_grab % 1048576) + 0x400000;
            grab_pteg2_ptr = machine_sysram_mem;
        }
    }
    else{
        pte_word2 = address_grab % rom_filesize;
        grab_pteg2_ptr = machine_sysrom_mem;
    }
}

void primary_generate_pa(){
    pteg_address1 |= ppc_state.ppc_spr[25] & 0xFE000000;
    pteg_temp1 = (((ppc_state.ppc_spr[25] & 0x1FF) << 10) & (pteg_hash1 & 0x7FC00));
    pteg_address1 |= ((ppc_state.ppc_spr[25] & 0x1FF0000) | pteg_temp1);
    pteg_address1 |= (pteg_hash1 & 0x3FF) << 6;
}

void secondary_generate_pa(){
    pteg_address2 |= ppc_state.ppc_spr[25] & 0xFE000000;
    pteg_temp2 = (((ppc_state.ppc_spr[25] & 0x1FF) << 10) & (pteg_hash2 & 0x7FC00));
    pteg_address2 |= ((ppc_state.ppc_spr[25] & 0x1FF0000) | pteg_temp2);
    pteg_address2 |= (pteg_hash2 & 0x3FF) << 6;
}

void primary_hash_check(uint32_t vpid_known){

    uint32_t entries_size = ((ppc_state.ppc_spr[25] & 0x1FF) > 0)? ((ppc_state.ppc_spr[25] & 0x1FF) << 9): 65536;
    uint32_t entries_area = pte_word1 + entries_size;

    uint32_t check_vpid = 0;

    do{
        if (!hash_found){
            check_vpid |= grab_pteg1_ptr[pte_word1++] << 24;
            check_vpid |= grab_pteg1_ptr[pte_word1++] << 16;
            check_vpid |= grab_pteg1_ptr[pte_word1++] << 8;
            check_vpid |= grab_pteg1_ptr[pte_word1++];

        check_vpid = (check_vpid >> 7) & 0xFFFFFF;

        if ((check_vpid >> 31) & 0x01){
            if (vpid_known == check_vpid){
                hash_found = true;
                pteg_answer |= grab_pteg1_ptr[pte_word1++] << 24;
                pteg_answer |= grab_pteg1_ptr[pte_word1++] << 16;
                pteg_answer |= grab_pteg1_ptr[pte_word1++] << 8;
                pteg_answer |= grab_pteg1_ptr[pte_word1++];
                break;
            }
            else{
                pte_word1 += 4;
                check_vpid = 0;
            }
        }
        else{
            pte_word1 += 4;
            check_vpid = 0;
        }
        }
        else{
            pte_word1 = entries_area;
        }

    }while (pte_word1 < entries_area);
}

void secondary_hash_check(uint32_t vpid_known){

    uint32_t entries_size = ((ppc_state.ppc_spr[25] & 0x1FF) > 0)? ((ppc_state.ppc_spr[25] & 0x1FF) << 9): 65536;
    uint32_t entries_area = pte_word1 + entries_size;

    uint32_t check_vpid = 0;

    do{
        if (!hash_found){
            check_vpid |= grab_pteg2_ptr[pte_word2++] << 24;
            check_vpid |= grab_pteg2_ptr[pte_word2++] << 16;
            check_vpid |= grab_pteg2_ptr[pte_word2++] << 8;
            check_vpid |= grab_pteg2_ptr[pte_word2++];

        check_vpid = (check_vpid >> 7) & 0xFFFFFF;

        if ((check_vpid >> 31) & 0x01){
            if (vpid_known == check_vpid){
                hash_found = true;
                pteg_answer |= grab_pteg2_ptr[pte_word2++] << 24;
                pteg_answer |= grab_pteg2_ptr[pte_word2++] << 16;
                pteg_answer |= grab_pteg2_ptr[pte_word2++] << 8;
                pteg_answer |= grab_pteg2_ptr[pte_word2++];
                break;
            }
            else{
                pte_word2 += 4;
                check_vpid = 0;
            }
        }
        else{
            pte_word2 += 4;
            check_vpid = 0;
        }
        }
        else{
            pte_word2 = entries_area;
        }

    }while (pte_word2 < entries_area);
}

void pteg_translate(uint32_t address_grab){
    uint32_t choose_sr = (ppc_effective_address >> 28) & 0x0F;
    pteg_hash1 = ppc_state.ppc_sr[choose_sr] & 0x7FFFF;
    uint32_t page_index = (ppc_effective_address & 0xFFFF000) >> 12;
    pteg_hash1 = (pteg_hash1 ^ page_index);
    pteg_hash2 = ~pteg_hash1;

    std::thread primary_pa_check(&primary_generate_pa);
    std::thread secondary_pa_check(&secondary_generate_pa);

    primary_pa_check.join();
    secondary_pa_check.join();

    uint32_t grab_val = ppc_state.ppc_sr[choose_sr] & 0xFFFFFF;

    std::thread primary_pteg_check(&primary_hash_check, std::ref(grab_val));
    std::thread secondary_pteg_check(&secondary_hash_check, std::ref(grab_val));

    primary_pteg_check.join();
    secondary_pteg_check.join();
}

/** PowerPC-style MMU instruction address translation. */
uint32_t ppc_mmu_instr_translate(uint32_t la)
{
    uint32_t pa; /* translated physical address */

    bool bat_hit = false;
    unsigned msr_pr = !!(ppc_state.ppc_msr & 0x4000);

    // Format: %XY
    // X - supervisor access bit, Y - problem/user access bit
    // Those bits are mutually exclusive
    unsigned access_bits = (~msr_pr << 1) | msr_pr;

    for (int bat_index = 0; bat_index < 4; bat_index++){
        PPC_BAT_entry *bat_entry = &ibat_array[bat_index];

        if ((bat_entry->access & access_bits) &&
            ((la & ~bat_entry->lo_mask) == bat_entry->bepi)) {
            bat_hit = true;
            // TODO: check access

            // logical to physical translation
            pa = bat_entry->phys_hi | (la & bat_entry->lo_mask);
            break;
        }
    }

    // Segment registers & page table translation
    if (!bat_hit){
        pteg_translate(la);
        if (hash_found == true){
            pa = (la & 0xFFF) | (pteg_answer & 0xFFFFF000);
        }
    }

    return pa;
}

/** PowerPC-style MMU data address translation. */
uint32_t ppc_mmu_addr_translate(uint32_t la, uint32_t access_type)
{
    uint32_t pa; /* translated physical address */

    bool bat_hit = false;
    unsigned msr_pr = !!(ppc_state.ppc_msr & 0x4000);

    // Format: %XY
    // X - supervisor access bit, Y - problem/user access bit
    // Those bits are mutually exclusive
    unsigned access_bits = (~msr_pr << 1) | msr_pr;

    for (int bat_index = 0; bat_index < 4; bat_index++){
        PPC_BAT_entry *bat_entry = &dbat_array[bat_index];

        if ((bat_entry->access & access_bits) &&
            ((la & ~bat_entry->lo_mask) == bat_entry->bepi)) {
            bat_hit = true;
            // TODO: check access

            // logical to physical translation
            pa = bat_entry->phys_hi | (la & bat_entry->lo_mask);
            break;
        }
    }

    // Segment registers & page table translation
    if (!bat_hit){
        pteg_translate(la);
        if (hash_found == true){
            pa = (la & 0xFFF) | (pteg_answer & 0xFFFFF000);
        }
    }

    return pa;
}


uint32_t write_last_pa_start  = 0;
uint32_t write_last_pa_end    = 0;
unsigned char *write_last_ptr = 0;

void address_quickinsert_translate(uint32_t value, uint32_t addr, uint8_t num_bytes)
{
    /* data address translation if enabled */
    if (ppc_state.ppc_msr & 0x10) {
        addr = ppc_mmu_addr_translate(addr, 0);
    }

    if (addr >= write_last_pa_start && addr <= write_last_pa_end) {
        ppc_memstore_value(write_last_ptr, value, addr - write_last_pa_start, num_bytes);
    } else {
        AddressMapEntry *entry = mem_ctrl_instance->find_range(addr);
        if (entry) {
            if (entry->type & RT_RAM) {
                write_last_pa_start = entry->start;
                write_last_pa_end   = entry->end;
                write_last_ptr = entry->mem_ptr;
                ppc_memstore_value(write_last_ptr, value, addr - entry->start, num_bytes);
            } else if (entry->type & RT_MMIO) {
                entry->devobj->write(addr - entry->start, value, num_bytes);
            } else {
                printf("Please check your address map!\n");
            }
        } else {
            printf("WARNING: write attempt to unmapped memory at 0x%08X!\n", addr);
        }
    }
}


uint32_t read_last_pa_start  = 0;
uint32_t read_last_pa_end    = 0;
unsigned char *read_last_ptr = 0;

/** Grab a value from memory into a register */
void address_quickgrab_translate(uint32_t addr, uint8_t num_bytes)
{
    /* data address translation if enabled */
    if (ppc_state.ppc_msr & 0x10) {
        //printf("DATA RELOCATION GO! - GRABBING \n");

        addr = ppc_mmu_addr_translate(addr, 0);
    }

    if (addr >= read_last_pa_start && addr <= read_last_pa_end) {
        ppc_set_return_val(read_last_ptr, addr - read_last_pa_start, num_bytes);
    } else {
        AddressMapEntry *entry = mem_ctrl_instance->find_range(addr);
        if (entry) {
            if (entry->type & (RT_ROM | RT_RAM)) {
                read_last_pa_start = entry->start;
                read_last_pa_end   = entry->end;
                read_last_ptr = entry->mem_ptr;
                ppc_set_return_val(read_last_ptr, addr - entry->start, num_bytes);
            } else if (entry->type & RT_MMIO) {
                return_value = entry->devobj->read(addr - entry->start, num_bytes);
            } else {
                printf("Please check your address map!\n");
            }
        } else {
            printf("WARNING: read attempt from unmapped memory at 0x%08X!\n", addr);

            /* reading from unmapped memory will return unmapped value */
            for (return_value = 0xFF; --num_bytes > 0;)
                return_value = (return_value << 8) | 0xFF;
        }
    }
}


uint32_t exec_last_pa_start  = 0;
uint32_t exec_last_pa_end    = 0;
unsigned char *exec_last_ptr = 0;

void quickinstruction_translate(uint32_t addr)
{
    /* instruction address translation if enabled */
    if (ppc_state.ppc_msr & 0x20) {
        addr = ppc_mmu_instr_translate(addr);
    }

    if (addr >= exec_last_pa_start && addr <= exec_last_pa_end) {
        ppc_set_cur_instruction(exec_last_ptr, addr - exec_last_pa_start);
    } else {
        AddressMapEntry *entry = mem_ctrl_instance->find_range(addr);
        if (entry && entry->type & (RT_ROM | RT_RAM)) {
            exec_last_pa_start = entry->start;
            exec_last_pa_end   = entry->end;
            exec_last_ptr = entry->mem_ptr;
            ppc_set_cur_instruction(exec_last_ptr, addr - exec_last_pa_start);
        } else {
            printf("WARNING: attempt to execute code at %08X!\n", addr);
        }
    }
}
