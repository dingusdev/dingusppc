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
#include "viacuda.h"
#include "macioserial.h"
#include "macswim3.h"
#include "ppcemumain.h"
#include "ppcmemory.h"
#include "openpic.h"
#include "mpc106.h"
#include "davbus.h"

std::vector<uint32_t> pte_storage;
uint64_t ppc_virtual_address; //It's 52 bits, but 64 bits more than covers the range needed.

uint32_t bat_srch;
uint32_t bepi_chk;

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

uint32_t dbat_array_map [4][3]={
      //flg  ea begin     ea end
        {0x00,0x00000000,0x00000000},
        {0x00,0x00000000,0x00000000},
        {0x00,0x00000000,0x00000000},
        {0x00,0x00000000,0x00000000}
    };

uint32_t ibat_array_map [4][3]={
      //flg  ea begin     ea end
        {0x00,0x00000000,0x00000000},
        {0x00,0x00000000,0x00000000},
        {0x00,0x00000000,0x00000000},
        {0x00,0x00000000,0x00000000}
    };

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

void ibat_update(){
    uint8_t tlb_place = 0;
    uint32_t ref_area = 0;
    bool msr_pr = ppc_state.ppc_msr & 0x4000; //This is for problem mode; make sure that supervisor mode does not touch this!
    for (int bat_srch = 528; bat_srch < 535; bat_srch += 2){
        ref_area = ((ppc_state.ppc_spr[bat_srch] & 0x1FFC) > 0) ? ((ppc_state.ppc_spr[bat_srch] & 0x1FFC) << 16): 131072;
        bepi_chk|= (ppc_effective_address & 0xFFFE0000) & ~ref_area;
        bool supervisor_on = (ppc_state.ppc_spr[bat_srch] & 0x00000002);
        bool problem_on = (ppc_state.ppc_spr[bat_srch] & 0x00000001);
        if (((ppc_state.ppc_spr[bat_srch] & 0xFFFE0000) == bepi_chk) &&
            ((problem_on && msr_pr) || (supervisor_on && !msr_pr))){
            //Set write/read flags, beginning of transfer area, and end of transfer area
            ibat_array_map[tlb_place][0] = (ppc_state.ppc_spr[bat_srch] & 0x3);
            ibat_array_map[tlb_place][1] = (ppc_state.ppc_spr[bat_srch] & 0xFFFE0000);
            ibat_array_map[tlb_place][2] = ((ppc_state.ppc_spr[bat_srch] & 0xFFFE0000) + ref_area) - 1;
            break;
        }
        tlb_place++;
    }
}

void dbat_update(){
    uint8_t tlb_place = 0;
    uint32_t ref_area = 0;
    bool msr_pr = ppc_state.ppc_msr & 0x4000; //This is for problem mode; make sure that supervisor mode does not touch this!
    for (int bat_srch = 536; bat_srch < 543; bat_srch += 2){
        ref_area = (ppc_state.ppc_spr[bat_srch] & 0x1FFC) > 0? ((ppc_state.ppc_spr[bat_srch] & 0x1FFC) << 16): 131072;
        bepi_chk|= (ppc_effective_address & 0xFFFE0000) & ~ref_area;
        bool supervisor_on = (ppc_state.ppc_spr[bat_srch] & 0x00000002);
        bool problem_on = (ppc_state.ppc_spr[bat_srch] & 0x00000001);
        if (((ppc_state.ppc_spr[bat_srch] & 0xFFFE0000) == bepi_chk) &&
            ((problem_on && (msr_pr != 0)) || (supervisor_on && (msr_pr == 0)))){
            //Set write/read flags, beginning of transfer area, and end of transfer area
            dbat_array_map[tlb_place][0] = (ppc_state.ppc_spr[bat_srch] & 0x3);
            dbat_array_map[tlb_place][1] = (ppc_state.ppc_spr[bat_srch] & 0xFFFE0000);
            dbat_array_map[tlb_place][2] = ((ppc_state.ppc_spr[bat_srch] & 0xFFFE0000) + ref_area) - 1;
            break;
        }
    tlb_place++;
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
        pte_word1 = address_grab % rom_file_setsize;
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
        pte_word2 = address_grab % rom_file_setsize;
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

void address_quickinsert_translate(uint32_t address_grab, uint32_t value_insert, uint8_t bit_num){
    //Insert a value into memory from a register

    printf("Inserting into address %x with %x \n", address_grab, value_insert);

    uint32_t storage_area = 0;
    uint32_t grab_batl = 537;
    uint32_t blocklen = 0;
    bool bat_to_go=0;
    bool pteg_to_go=0;

    //data bat
    if ((ppc_state.ppc_msr >> 4) & 0x1){
        printf("DATA RELOCATION GO! - INSERTING \n");
        uint32_t min_val;
        uint32_t max_val;

        pteg_to_go = 1;

        for (uint32_t grab_loop = 0; grab_loop < 4; grab_loop++){
            if ((dbat_array_map[grab_loop][0] >> 1) & 0x1){
                min_val = dbat_array_map[grab_loop][1];
                max_val = dbat_array_map[grab_loop][2];
                if ((address_grab >= min_val) && (address_grab < max_val) && (max_val != 0)){
                    blocklen = max_val - min_val;
                    bat_to_go = 1;
                    pteg_to_go = 0;
                    break;
                }
            }
            grab_batl += 2;
        }
    }

    if (bat_to_go){
        uint32_t final_grab = 0;
        final_grab |= (((address_grab & 0x0FFE0000) & blocklen) | (ppc_state.ppc_spr[grab_batl] & 0xFFFE0000));
        final_grab |= (address_grab & 0x1FFFF);
        //Check the PP Tags in the batl
        //if (!(ppc_state.ppc_spr[grab_batl] == 0x2)){
        //    ppc_expection_handler(0x0300, 0x0);
        // }
        address_grab = final_grab;
    }
    else if (pteg_to_go){
        pteg_translate(address_grab);
        if (hash_found == true){
            address_grab &= 0xFFF;
            address_grab |= (pteg_answer & 0xFFFFF000);
        }
    }

    //regular grabbing
    if (address_grab < 0x80000000){
        if (mpc106_check_membound(address_grab)){
            if (address_grab > 0x03ffffff){ //for debug purposes
                storage_area = address_grab;
                grab_macmem_ptr = machine_sysram_mem;
            }
            else if ((address_grab >= 0x5fffe000) && (address_grab <= 0x5fffffff)){
                storage_area = address_grab % 0x2000;
                grab_macmem_ptr = machine_sysconfig_mem;
            }
            else{
                storage_area = address_grab % 0x04000000;
                grab_macmem_ptr = machine_sysram_mem;
                printf("Uncharted territory: %x \n", address_grab);
            }
        }
        else{
            return;
        }
    }
    else if (address_grab < 0x80800000){
        storage_area = address_grab % 0x800000;
        if (address_grab == 0x80000CF8){
            storage_area = 0x0CF8;  //CONFIG_ADDR
            value_insert = rev_endian32(value_insert);
            grab_macmem_ptr = machine_fecxxx_mem;
            uint32_t reg_num = (value_insert & 0x07FC) >> 2;
            uint32_t dev_num = (value_insert & 0xF800) >> 11;
            printf("ADDRESS SET FOR GRACKLE: ");
            printf("Device Number: %d  ", dev_num);
            printf("Hex Register Number: %x \n", reg_num);
            mpc106_address = value_insert;
        }
        else{
            grab_macmem_ptr = machine_upperiocontrol_mem;
        }


        if ((address_grab >= 0x80040000) && (address_grab < 0x80080000)){
            openpic_address = address_grab - 0x80000000;
            openpic_read_word = value_insert;
            openpic_read();
            return;
        }

        printf("Uncharted territory: %x \n", address_grab);
    }
    else if (address_grab < 0x81000000){
        if (address_grab > 0x83FFFFFF){
            return;
        }
        storage_area = address_grab;
        printf("Uncharted territory: %x \n", address_grab);
        grab_macmem_ptr = machine_iocontrolcdma_mem;
    }
    else if (address_grab < 0xBF800000){
        storage_area = address_grab % 33554432;
        printf("Uncharted territory: %x \n", address_grab);
        grab_macmem_ptr = machine_loweriocontrol_mem;
    }
    else if (address_grab < 0xC0000000){
        storage_area = address_grab % 16;
        printf("Uncharted territory: %x \n", address_grab);
        grab_macmem_ptr = machine_interruptack_mem;
    }
    else if (address_grab < 0xF0000000){
        printf("Invalid Memory Attempt: %x \n", address_grab);
        return;
    }
    else if (address_grab < 0xF8000000){
        storage_area = address_grab % 67108864;
            if ((address_grab >= 0xF3013000) && (address_grab < 0xF3013040)){
                mac_serial_address = storage_area;
                serial_write_byte = (uint8_t)value_insert;
                printf("Writing byte to Serial address %x ... %x \n", address_grab, via_write_byte);
                mac_serial_write();
                return;
            }
            else if ((address_grab >= 0xF3014000) && (address_grab < 0xF3015000)){
                davbus_address = storage_area;
                davbus_write_word = value_insert;
                printf("\nWriting to DAVBus: %x \n", return_value);
                davbus_write();
                return;
            }
            else if ((address_grab >= 0xF3015000) && (address_grab < 0xF3016000)){
                mac_swim3_address = storage_area;
                swim3_write_byte = (uint8_t)value_insert;
                printf("Writing byte to SWIM3 address %x ... %x \n", address_grab, swim3_write_byte);
                mac_swim3_write();
                return;
            }
            else if ((address_grab >= 0xF3016000) && (address_grab < 0xF3018000)){
                via_cuda_address = storage_area;
                via_write_byte = (uint8_t)value_insert;
                printf("Writing byte to CUDA address %x ... %x \n", address_grab, via_write_byte);
                via_cuda_write();
                return;
            }
            else if ((address_grab >= 0xF3040000) && (address_grab < 0xF3080000)){
                openpic_address = storage_area - 0x3000000;
                openpic_write_word = value_insert;
                printf("Writing byte to OpenPIC address %x ... %x \n", address_grab, openpic_write_word);
                openpic_write();
                return;
            }
            else if (address_grab > 0xF3FFFFFF){
                printf("Uncharted territory: %x", address_grab);
                return;
            }
        grab_macmem_ptr = machine_iocontrolmem_mem;
    }
    else if (address_grab < rom_file_begin){
        //Get back to this! (weeny1)

        if (address_grab < 0xFE000000){
            storage_area = address_grab % 4096;
            grab_macmem_ptr = machine_f8xxxx_mem;
        }
        else if (address_grab < 0xFEC00000){
            mpc106_address = address_grab % 65536;
            mpc106_write(value_insert);
            return;
        }
        else if (address_grab < 0xFEE00000){
            storage_area = 0x0CF8;  //CONFIG_ADDR
            grab_macmem_ptr = machine_fecxxx_mem;
            value_insert = rev_endian32(value_insert);
            uint32_t reg_num = (value_insert & 0x07FC) >> 2;
            uint32_t dev_num = (value_insert & 0xF800) >> 11;
            printf("ADDRESS SET FOR GRACKLE \n");
            printf("Device Number: %d ", dev_num);
            printf("Hex Register Number: %x \n", reg_num);
            mpc_config_addr = value_insert;
        }
        else if (address_grab < 0xFF000000){
            storage_area = 0x0CFC;  //CONFIG_DATA
            mpc106_word_custom_size = bit_num;
            mpc106_write_device(mpc_config_addr, value_insert, bit_num);
            grab_macmem_ptr = machine_feexxx_mem;
        }
        else if (address_grab < 0xFF800000){
            storage_area = address_grab % 4096;
            grab_macmem_ptr = machine_ff00xx_mem;
        }
        else{
            storage_area = (address_grab % 1048576) + 0x400000;
            grab_macmem_ptr = machine_sysram_mem;
        }
    }
    else{
        storage_area = address_grab % rom_file_setsize;
        grab_macmem_ptr = machine_sysrom_mem;
    }

    switch (ppc_state.ppc_msr & 0x1){
    case 0:
        if (bit_num == 1){
            grab_macmem_ptr[storage_area] = (uint8_t)value_insert;
        }
        else if (bit_num == 2){
            grab_macmem_ptr[storage_area++] = (uint8_t)(value_insert >> 8);
            grab_macmem_ptr[storage_area] = (uint8_t)value_insert;
        }
        else if (bit_num == 4){
            grab_macmem_ptr[storage_area++] = (uint8_t)(value_insert >> 24);
            grab_macmem_ptr[storage_area++] = (uint8_t)(value_insert >> 16);
            grab_macmem_ptr[storage_area++] = (uint8_t)(value_insert >> 8);
            grab_macmem_ptr[storage_area] = (uint8_t)value_insert;
        }
        break;
    case 1:
        if (bit_num == 1){
            grab_macmem_ptr[storage_area] = (uint8_t)value_insert;
        }
        else if (bit_num == 2){
            grab_macmem_ptr[storage_area++] = (uint8_t)value_insert;
            grab_macmem_ptr[storage_area] = (uint8_t)(value_insert >> 8);
        }
        else if (bit_num == 4){
            grab_macmem_ptr[storage_area++] = (uint8_t)value_insert;
            grab_macmem_ptr[storage_area++] = (uint8_t)(value_insert >> 8);
            grab_macmem_ptr[storage_area++] = (uint8_t)(value_insert >> 16);
            grab_macmem_ptr[storage_area] = (uint8_t)(value_insert >> 24);
        }
        break;
    }
}

void address_quickgrab_translate(uint32_t address_grab, uint32_t value_extract, uint8_t bit_num){
    //Grab a value from memory into a register

    printf("Grabbing from address %x \n", address_grab);

    uint32_t storage_area = 0;
    uint32_t grab_batl = 537;
    uint32_t blocklen = 0;
    bool bat_to_go=0;
    bool pteg_to_go=0;

    return_value = 0; //reset this before going into the real fun.

    //data bat
    if ((ppc_state.ppc_msr >> 4) & 0x1){
        printf("DATA RELOCATION GO! - GRABBING \n");
        uint32_t min_val;
        uint32_t max_val;

        pteg_to_go = 1;

        for (uint32_t grab_loop = 0; grab_loop < 4; grab_loop++){
            if (dbat_array_map[grab_loop][0] & 0x1){
                min_val = dbat_array_map[grab_loop][1];
                max_val = dbat_array_map[grab_loop][2];
                if ((address_grab >= min_val) && (address_grab <= max_val) && (max_val != 0)){
                    blocklen = max_val - min_val;
                    bat_to_go = 1;
                    pteg_to_go = 0;
                    break;
                }
            }
            grab_batl += 2;
        }
    }

    if (bat_to_go){
        uint32_t final_grab = 0;
        final_grab |= (((address_grab & 0x0FFE0000) & blocklen) | (ppc_state.ppc_spr[grab_batl] & 0xFFFE0000));
        final_grab |= (address_grab & 0x1FFFF);
        //Check the PP Tags in the batl
        //if ((ppc_state.ppc_spr[grab_batl] & 0x3) == 0x0){
        //    ppc_expection_handler(0x0300, 0x0);
        //}
        address_grab = final_grab;
    }
    else if (pteg_to_go){
        pteg_translate(address_grab);
        if (hash_found == true){
            address_grab &= 0xFFF;
            address_grab |= (pteg_answer & 0xFFFFF000);
        }
    }

    //regular grabbing
    if (address_grab < 0x80000000){
        if (mpc106_check_membound(address_grab)){
            if (address_grab > 0x03ffffff){ //for debug purposes
                storage_area = address_grab;
                grab_macmem_ptr = machine_sysram_mem;
            }
            else if ((address_grab >= 0x5fffe000) && (address_grab <= 0x5fffffff)){
                storage_area = address_grab % 0x2000;
                grab_macmem_ptr = machine_sysconfig_mem;
            }
            else{
                return_value = (bit_num == 1)?0xFF:(bit_num == 2)?0xFFFF:0xFFFFFFFF;
                return;
            }
        }
        else{
            //The address is not within the ROM banks
            return_value = (bit_num == 1)?0xFF:(bit_num == 2)?0xFFFF:0xFFFFFFFF;
            return;
        }
    }
    else if (address_grab < 0x80800000){
        if ((address_grab >= 0x80040000) && (address_grab < 0x80080000)){
            openpic_address = address_grab - 0x80000000;
            openpic_write();
            return_value = openpic_write_word;
            return;
        }

        storage_area = address_grab % 0x800000;
        printf("Uncharted territory: %x \n", address_grab);
        grab_macmem_ptr = machine_upperiocontrol_mem;
    }
    else if (address_grab < 0x81000000){
        storage_area = address_grab;
        if (address_grab > 0x83FFFFFF){
            return_value = (bit_num == 1)?0xFF:(bit_num == 2)?0xFFFF:0xFFFFFFFF;
            return;
        }
        printf("Uncharted territory: %x \n", address_grab);
        grab_macmem_ptr = machine_iocontrolcdma_mem;
    }
    else if (address_grab < 0xBF800000){
        storage_area = address_grab % 33554432;
        printf("Uncharted territory: %x \n", address_grab);
        grab_macmem_ptr = machine_loweriocontrol_mem;
    }
    else if (address_grab < 0xC0000000){
        storage_area = address_grab % 16;
        printf("Uncharted territory: %x \n", address_grab);
        grab_macmem_ptr = machine_interruptack_mem;
    }
    else if (address_grab < 0xF0000000){
        return_value = (bit_num == 1)?0xFF:(bit_num == 2)?0xFFFF:0xFFFFFFFF;
        return;
    }
    else if (address_grab < 0xF8000000){
        storage_area = address_grab % 67108864;
            if ((address_grab >= 0xF3013000) && (address_grab < 0xF3013040)){
                mac_serial_address = storage_area;
                mac_serial_read();
                return_value = serial_read_byte;
                printf("\n Read from Serial: %x \n", return_value);
                return;
            }
            else if ((address_grab >= 0xF3014000) && (address_grab < 0xF3015000)){
                davbus_address = storage_area;
                davbus_read();
                return_value = davbus_read_word;
                printf("\n Read from DAVBus: %x \n", return_value);
                return;
            }
            else if ((address_grab >= 0xF3015000) && (address_grab < 0xF3016000)){
                mac_swim3_address = storage_area;
                mac_swim3_read();
                return_value = swim3_read_byte;
                printf("\n Read from Swim3: %x \n", return_value);
                return;
            }
            else if ((address_grab >= 0xF3016000) && (address_grab < 0xF3018000)){
                via_cuda_address = storage_area;
                via_cuda_read();
                return_value = via_read_byte;
                printf("\n Read from CUDA: %x \n", return_value);
                return;
            }
            else if ((address_grab >= 0xF3040000) && (address_grab < 0xF3080000)){
                openpic_address = storage_area - 0x3000000;
                openpic_read();
                return_value = openpic_write_word;
                return;
            }
            else if (address_grab > 0xF3FFFFFF){
                return_value = (bit_num == 1)?0xFF:(bit_num == 2)?0xFFFF:0xFFFFFFFF;
                return;
            }
        grab_macmem_ptr = machine_iocontrolmem_mem;
    }
    else if (address_grab < rom_file_begin){
        //Get back to this! (weeny1)
        if (address_grab < 0xFE000000){
            storage_area = address_grab % 4096;
            grab_macmem_ptr = machine_f8xxxx_mem;
        }
        else if (address_grab < 0xFEC00000){
            mpc106_address = address_grab % 65536;
            mpc106_read();
            return_value = mpc106_read_word;
            return;
        }
        else if (address_grab < 0xFEE00000){
            return_value = (bit_num == 1)? (mpc106_address & 0xFF):(bit_num == 2)?(mpc106_address & 0xFFFF):mpc106_address;
            return;
        }
        else if (address_grab < 0xFF000000){
            mpc106_word_custom_size = bit_num;
            return_value = mpc106_read_device(mpc_config_addr, bit_num);
            return_value = rev_endian32(return_value);
            return;
        }
        else if (address_grab < 0xFF800000){
            storage_area = address_grab % 4096;
            grab_macmem_ptr = machine_ff00xx_mem;
        }
        else{
            storage_area = (address_grab % 1048576) + 0x400000;
            grab_macmem_ptr = machine_sysram_mem;
        }
    }
    else{
        printf("Charting ROM Area: %x \n", address_grab);
        storage_area = address_grab % rom_file_setsize;
        grab_macmem_ptr = machine_sysrom_mem;
    }

    //Put the final result in return_value here
    //This is what gets put back into the register
    switch (ppc_state.ppc_msr & 0x1){
    case 0:
        if (bit_num == 1){
            return_value |= ((uint32_t)(grab_macmem_ptr[storage_area]));
        }
        else if (bit_num == 2){
            return_value |= ((uint32_t)(grab_macmem_ptr[storage_area++])) << 8;
            return_value |= ((uint32_t)(grab_macmem_ptr[storage_area]));
        }
        else if (bit_num == 4){
            return_value |= ((uint32_t)(grab_macmem_ptr[storage_area++])) << 24;
            return_value |= ((uint32_t)(grab_macmem_ptr[storage_area++])) << 16;
            return_value |= ((uint32_t)(grab_macmem_ptr[storage_area++])) << 8;
            return_value |= ((uint32_t)(grab_macmem_ptr[storage_area]));
        }
        break;
    case 1:
        if (bit_num == 1){
            return_value |= ((uint32_t)(grab_macmem_ptr[storage_area]));
        }
        else if (bit_num == 2){
            return_value |= ((uint32_t)(grab_macmem_ptr[storage_area++])) ;
            return_value |= ((uint32_t)(grab_macmem_ptr[storage_area])) << 8;
        }
        else if (bit_num == 4){
            return_value |= ((uint32_t)(grab_macmem_ptr[storage_area++])) ;
            return_value |= ((uint32_t)(grab_macmem_ptr[storage_area++])) << 8;
            return_value |= ((uint32_t)(grab_macmem_ptr[storage_area++])) << 16;
            return_value |= ((uint32_t)(grab_macmem_ptr[storage_area])) << 24;
        }
    }
}

void quickinstruction_translate(uint32_t address_grab){
    uint32_t storage_area = 0;
    uint32_t grab_batl = 537;
    uint32_t blocklen = 0;
    bool bat_to_go=0;
    bool pteg_to_go=0;

    return_value = 0; //reset this before going into the real fun.

    //instruction bat
    if ((ppc_state.ppc_msr >> 5) & 0x1) {
        printf("INSTRUCTION RELOCATION GO! \n");

        uint32_t min_val;
        uint32_t max_val;

        pteg_to_go = 1;

        for (uint32_t grab_loop = 0; grab_loop < 4; grab_loop++){
            if (ibat_array_map[grab_loop][0] & 0x1){
                min_val = ibat_array_map[grab_loop][1];
                max_val = ibat_array_map[grab_loop][2];
                if ((address_grab >= min_val) && (address_grab <= max_val) && (max_val != 0)){
                    blocklen = max_val - min_val;
                    bat_to_go = 1;
                    pteg_to_go = 0;
                    break;
                }
            }
            grab_batl += 2;
        }
    }

    if (bat_to_go){
        uint32_t final_grab = 0;
        final_grab |= (((address_grab & 0x0FFE0000) & blocklen) | (ppc_state.ppc_spr[grab_batl] & 0xFFFE0000));
        final_grab |= (address_grab & 0x1FFFF);
        //if ((ppc_state.ppc_spr[grab_batl] & 0x3) == 0x0){
        //    ppc_expection_handler(0x0400, 0x0);
        //}
        address_grab = final_grab;
    }
    else if (pteg_to_go){
        pteg_translate(address_grab);
        if (hash_found == true){
            address_grab &= 0xFFF;
            address_grab |= (pteg_answer & 0xFFFFF000);
        }
    }

    if ((address_grab < 0x100000) && ((ppc_state.ppc_msr >> 6) & 1)){
        address_grab |= 0xFFF00000;
    }

    //grab opcode from memory area
    if (address_grab < 0x80000000){
        if (address_grab < 0x040000000){ //for debug purposes
            storage_area = address_grab;
            grab_macmem_ptr = machine_sysram_mem;
        }
        else if ((address_grab >= 0x5fffe000) && (address_grab <= 0x5fffffff)){
            storage_area = address_grab % 0x2000;
            grab_macmem_ptr = machine_sysconfig_mem;
        }
        else{
            storage_area = address_grab % 0x04000000;
            grab_macmem_ptr = machine_sysram_mem;
            printf("Uncharted territory: %x \n", address_grab);
        }
    }
    else if (address_grab < 0x80800000){
        storage_area = address_grab % 0x800000;
        grab_macmem_ptr = machine_upperiocontrol_mem;

    }
    else if (address_grab < 0x81000000){
        storage_area = address_grab % 0x800000;
        grab_macmem_ptr = machine_iocontrolcdma_mem;

    }
    else if (address_grab < 0xBF80000){
        storage_area = address_grab % 33554432;
        grab_macmem_ptr = machine_loweriocontrol_mem;

    }
    else if (address_grab < 0xC0000000){
        storage_area = address_grab % 16;
        grab_macmem_ptr = machine_interruptack_mem;

    }
    else if (address_grab < 0xF0000000){
        printf("Invalid Memory Attempt: %x \n", address_grab);
        return;
    }
    else if (address_grab < 0xF8000000){
        storage_area = address_grab % 67108864;
        grab_macmem_ptr = machine_iocontrolmem_mem;

    }
    else if (address_grab < rom_file_begin){
        //Get back to this! (weeny1)

        if (address_grab < 0xFE000000){
            storage_area = address_grab % 4096;
            grab_macmem_ptr = machine_f8xxxx_mem;
        }
        else if (address_grab < 0xFEC00000){
            storage_area = address_grab % 65536;
            grab_macmem_ptr = machine_fexxxx_mem;
        }
        else if (address_grab < 0xFEE00000){
            storage_area = 0x0CF8;  //CONFIG_ADDR
            grab_macmem_ptr = machine_fecxxx_mem;
        }
        else if (address_grab < 0xFF000000){
            storage_area = 0x0CFC;  //CONFIG_DATA
            grab_macmem_ptr = machine_feexxx_mem;
        }
        else if (address_grab < 0xFF800000){
            storage_area = address_grab % 4096;
            grab_macmem_ptr = machine_ff00xx_mem;
        }
        else{
            storage_area = (address_grab % 1048576) + 0x400000;
            grab_macmem_ptr = machine_sysram_mem;
        }

    }
    else{
        storage_area = address_grab % rom_file_setsize;
        grab_macmem_ptr = machine_sysrom_mem;
    }

    ppc_cur_instruction += (grab_macmem_ptr[storage_area]) << 24;
    ++storage_area;
    ppc_cur_instruction += (grab_macmem_ptr[storage_area]) << 16;
    ++storage_area;
    ppc_cur_instruction += (grab_macmem_ptr[storage_area]) << 8;
    ++storage_area;
    ppc_cur_instruction += (grab_macmem_ptr[(storage_area)]);
}
