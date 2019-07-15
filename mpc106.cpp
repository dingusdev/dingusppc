//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

//Functionality for the MPC106

#include <iostream>
#include <cstring>
#include <cinttypes>
#include "viacuda.h"
#include "mpc106.h"
#include "ppcemumain.h"
#include "ppcmemory.h"

bool mpc106_membound_change;
bool mpc106_mem_approve; //Confirm memory transaction

uint32_t mpc106_address;  // For fe000000 - fe00ffff
uint32_t mpc_config_addr; // Device No. and Reg No. included
uint32_t mpc_config_dat;  // Value to write to the device
uint32_t cust_grab_size;  // Custom bit size

uint32_t mpc106_write_word;
uint32_t mpc106_read_word;
uint16_t mpc106_write_half;
uint16_t mpc106_read_half;
uint8_t mpc106_write_byte;
uint8_t mpc106_read_byte;

uint8_t mpc106_word_custom_size;

//The below array is used for ROM banks
//Top eight entries are for the starting addresses
//Bottom eight entries are for the ending addresses
uint32_t mpc106_memory_bounds[16] =
{
0x00000, 0x00000, 0x00000, 0x00000,
0x00000, 0x00000, 0x00000, 0x00000,
0xFFFFF, 0xFFFFF, 0xFFFFF, 0xFFFFF,
0xFFFFF, 0xFFFFF, 0xFFFFF, 0xFFFFF,
};

//Entries are organized like so...

//ROM Banks 0-3  (ext. starting address) (starting address) (0x00000)
//ROM Banks 4-7  (ext. starting address) (starting address) (0x00000)
//ROM Banks 0-3  (ext.  ending  address) ( ending  address) (0x00000)
//ROM Banks 4-7  (ext.  ending  address) ( ending  address) (0x00000)

void mpc106_init(){
    mpc_config_addr = 0;
    mpc_config_dat = 0;

    //Initialize Vendor & Device IDs
    machine_fexxxx_mem[0x00] = 0x57;
    machine_fexxxx_mem[0x01] = 0x10;

    machine_fexxxx_mem[0x02] = 0x02;

    //PCI command + status
    machine_fexxxx_mem[0x04] = 0x06;
    machine_fexxxx_mem[0x06] = 0x80;

    machine_fexxxx_mem[0x0B] = 0x06;
    machine_fexxxx_mem[0x0C] = 0x08;

    machine_fexxxx_mem[0x73] = 0xCD;

    machine_fexxxx_mem[0xA8] = 0x10;
    machine_fexxxx_mem[0xA9] = 0x00;
    machine_fexxxx_mem[0xAA] = 0x00;
    machine_fexxxx_mem[0xAB] = 0xFF;

    machine_fexxxx_mem[0xAC] = 0x0C;
    machine_fexxxx_mem[0xAD] = 0x06;
    machine_fexxxx_mem[0xAE] = 0x0C;
    machine_fexxxx_mem[0xAF] = 0x00;

    machine_fexxxx_mem[0xBA] = 0x04;
    machine_fexxxx_mem[0xC0] = 0x01;

    machine_fexxxx_mem[0xE0] = 0x42;
    machine_fexxxx_mem[0xE1] = 0x00;
    machine_fexxxx_mem[0xE2] = 0xFF;
    machine_fexxxx_mem[0xE3] = 0x0F;

    machine_fexxxx_mem[0xF0] = 0x00;
    machine_fexxxx_mem[0xF1] = 0x00;
    machine_fexxxx_mem[0xF2] = 0x02;
    machine_fexxxx_mem[0xF3] = 0xFF;

    machine_fexxxx_mem[0xF4] = 0x03;
    machine_fexxxx_mem[0xF5] = 0x00;
    machine_fexxxx_mem[0xF6] = 0x00;
    machine_fexxxx_mem[0xF7] = 0x00;

    machine_fexxxx_mem[0xFC] = 0x00;
    machine_fexxxx_mem[0xFD] = 0x00;
    machine_fexxxx_mem[0xFE] = 0x10;
    machine_fexxxx_mem[0xFF] = 0x00;

	mpc106_mem_approve = false;
	mpc106_word_custom_size = 0;
}

uint32_t mpc106_write_device(uint32_t device_addr, uint32_t insert_to_device, uint8_t bit_length){
    //Write to the specified device - Invoked when a write is made to 0xFEExxxxx.
    //device_addr is what's stored in 0xFEC00CF8 (The MPG106/Grackle's CONFIG_ADDR Register).

    uint32_t reg_num  = (device_addr & 0x07FC) >> 2;
    uint32_t dev_num  = (device_addr & 0xF800) >> 11;

    switch(dev_num){
    case 0:
        //Device 0 is reserved to the grackle by default
        mpc106_address = reg_num;
        mpc106_write(insert_to_device);
        break;
    case 16:
    case 17:
        via_cuda_address = (reg_num << 9) + (dev_num << 12) + 0x3000000;
        via_write_byte = (uint8_t)insert_to_device;
        via_cuda_write();
        break;
    }

    return 0;
}

uint32_t mpc106_read_device(uint32_t device_addr, uint8_t bit_length){
    //Read to the specified device - Invoked when a read is made to 0xFEExxxxx.
    //device_addr is what's stored in 0xFEC00CF8 (The MPG106/Grackle's CONFIG_ADDR Register).

    uint32_t reg_num = (device_addr & 0x07FC) >> 2;
    uint32_t dev_num = (device_addr & 0xF800) >> 11;

    uint32_t grab_value = 0;

    switch(dev_num){
    case 0:
        //Device 0 is reserved to the grackle by default
        mpc106_address = reg_num;
        mpc106_read();
        grab_value = mpc106_read_word;
        break;
    case 16:
    case 17:
        via_cuda_address = (reg_num << 8) + (dev_num << 12) + 0x3000000;
        via_cuda_read();
        grab_value = (uint32_t)via_read_byte;
        break;
    }

    return grab_value;
}

bool mpc106_check_membound(uint32_t attempted_address){
	uint32_t mem_address_begin;
	uint32_t mem_address_end;
	for (int get_rom_bank = 0; get_rom_bank < 8; get_rom_bank++){
		mem_address_begin = mpc106_memory_bounds[get_rom_bank];
		mem_address_end = mpc106_memory_bounds[get_rom_bank + 8];
		if ((attempted_address >= mem_address_begin) & (attempted_address <= mem_address_end)){
		uint8_t is_valid = (machine_fexxxx_mem[0xA0] >> get_rom_bank) & 0x01;
			if (is_valid == 0){
				return true;
			}
		}
	}
	return false;
}


void mpc106_set_membound_begin(uint8_t bound_area){
    uint32_t bcheck_area = 0x80 + bound_area;
	uint32_t newbound = machine_fexxxx_mem[bcheck_area];
	uint32_t bound_entry = bound_area % 8;
	uint32_t change_entry = mpc106_memory_bounds[bound_entry];
	change_entry &= 0xfffff;
	change_entry = newbound << 20;
	mpc106_memory_bounds[bound_entry] = change_entry;
}

void mpc106_set_membound_extbegin(uint8_t bound_area){
	uint32_t bcheck_area = 0x88 + bound_area;
	uint32_t newbound = machine_fexxxx_mem[bcheck_area];
	uint32_t bound_entry = bound_area % 8;
	uint32_t change_entry = mpc106_memory_bounds[bound_entry];
	change_entry &= 0x0fffffff;
	change_entry = (newbound & 0x3) << 28;
	mpc106_memory_bounds[bound_entry] = change_entry;
}

void mpc106_set_membound_end(uint8_t bound_area){
    uint32_t bcheck_area = 0x90 + bound_area;
	uint32_t newbound = machine_fexxxx_mem[bcheck_area];
	uint32_t bound_entry = (bound_area % 8) + 8;
	uint32_t change_entry = mpc106_memory_bounds[bound_entry];
	change_entry &= 0xfffff;
	change_entry = newbound << 20;
	mpc106_memory_bounds[bound_entry] = change_entry;
}

void mpc106_set_membound_extend(uint8_t bound_area){
    uint32_t bcheck_area = 0x98 + bound_area;
	uint32_t newbound = machine_fexxxx_mem[bcheck_area];
	uint32_t bound_entry = (bound_area % 8) + 8;
	uint32_t change_entry = mpc106_memory_bounds[bound_entry];
	change_entry &= 0x0fffffff;
	change_entry = (newbound & 0x3) << 28;
	mpc106_memory_bounds[bound_entry] = change_entry;
}

void mpc106_read(){

    uint8_t read_length = 4;


    if ((mpc106_address >= 0x80) | (mpc106_address < 0xA0)){
		mpc106_membound_change = true;
        read_length = mpc106_word_custom_size;
    }
    else{
    switch (mpc106_address){
    case 0x8:
    case 0x9:
    case 0xA:
    case 0xB:
    case 0xC:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x3C:
    case 0x3D:
    case 0x3E:
    case 0x3F:
    case 0x72:
    case 0x73:
    case 0xA0:
    case 0xA3:
        read_length = 1;
        break;
    case 0x0:
    case 0x2:
    case 0x4:
    case 0x6:
    case 0x70:
        read_length = 2;
        break;
    case 0xF0:
    case 0xF4:
    case 0xF8:
    case 0xFC:
        read_length = 4;
    default: //Avoid writing into reserved areas
        read_length = 0;
    }
    }

    switch(read_length){
        case 1:
            mpc106_read_word |= (grab_macmem_ptr[mpc106_address]);
            break;
        case 2:
            mpc106_read_word |= (grab_macmem_ptr[mpc106_address++]);
            mpc106_read_word |= (grab_macmem_ptr[mpc106_address]) << 8;
            break;
        case 4:
            mpc106_read_word |= (grab_macmem_ptr[mpc106_address++]);
            mpc106_read_word |= (grab_macmem_ptr[mpc106_address++]) << 8;
            mpc106_read_word |= (grab_macmem_ptr[mpc106_address++]) << 16;
            mpc106_read_word |= (grab_macmem_ptr[mpc106_address]) << 24;
            break;
    }

}

void mpc106_write(uint32_t write_word){

    uint8_t write_length = 4;

    if ((mpc106_address >= 0x80) | (mpc106_address < 0xA0)){
		mpc106_membound_change = true;
        write_length = mpc106_word_custom_size;
    }
    else{
        switch (mpc106_address){
        case 0x70:
        case 0x72:
        case 0x73:
        case 0xA0:
        case 0xA3:
            write_length = 1;
            break;
        case 0x4:
        case 0x6:
            write_length = 2;
            break;
        case 0xF0:
        case 0xF4:
        case 0xF8:
        case 0xFC:
            write_length = 4;
        default: //Avoid writing into reserved areas
            write_length = 0;
        }
    }

    if (mpc106_membound_change){
        switch(write_length){
            case 1:
                change_membound_time();
                grab_macmem_ptr[mpc106_address] |= (uint8_t)((mpc106_read_word) & 0xFF);
                break;
            case 2:
                change_membound_time();
                grab_macmem_ptr[mpc106_address++] |= (uint8_t)((mpc106_read_word) & 0xFF);
                change_membound_time();
                grab_macmem_ptr[mpc106_address] |= (uint8_t)((mpc106_read_word >> 8) & 0xFF);
                break;
            case 4:
                change_membound_time();
                grab_macmem_ptr[mpc106_address++] |= (uint8_t)((mpc106_read_word) & 0xFF);
                change_membound_time();
                grab_macmem_ptr[mpc106_address++] |= (uint8_t)((mpc106_read_word >> 8) & 0xFF);
                change_membound_time();
                grab_macmem_ptr[mpc106_address++] |= (uint8_t)((mpc106_read_word >> 16) & 0xFF);
                change_membound_time();
                grab_macmem_ptr[mpc106_address] |= (uint8_t)((mpc106_read_word >> 24) & 0xFF);
                break;
        }
    }
    else{
        switch(write_length){
            case 1:
                grab_macmem_ptr[mpc106_address] |= (uint8_t)((mpc106_read_word) & 0xFF);
                break;
            case 2:
                grab_macmem_ptr[mpc106_address++] |= (uint8_t)((mpc106_read_word) & 0xFF);
                grab_macmem_ptr[mpc106_address] |= (uint8_t)((mpc106_read_word >> 8) & 0xFF);
                break;
            case 4:
                grab_macmem_ptr[mpc106_address++] |= (uint8_t)((mpc106_read_word) & 0xFF);
                grab_macmem_ptr[mpc106_address++] |= (uint8_t)((mpc106_read_word >> 8) & 0xFF);
                grab_macmem_ptr[mpc106_address++] |= (uint8_t)((mpc106_read_word >> 16) & 0xFF);
                grab_macmem_ptr[mpc106_address] |= (uint8_t)((mpc106_read_word >> 24) & 0xFF);
                break;
            }
        }
}

void change_membound_time(){
    if (mpc106_address < 0x88){
        mpc106_set_membound_begin(mpc106_address);
        mpc106_membound_change = false;
    }
    else if (mpc106_address < 0x90){
        mpc106_set_membound_extbegin(mpc106_address);
        mpc106_membound_change = false;
    }
    else if (mpc106_address < 0x98){
        mpc106_set_membound_end(mpc106_address);
        mpc106_membound_change = false;
    }
    else if (mpc106_address < 0xA0){
        mpc106_set_membound_extend(mpc106_address);
        mpc106_membound_change = false;
    }
}
