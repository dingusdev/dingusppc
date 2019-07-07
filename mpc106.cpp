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
#include "mpc106.h"
#include "ppcemumain.h"
#include "ppcmemory.h"

bool mpc106_address_map;

uint32_t mpc106_address;  // For fe000000 - fe00ffff
uint32_t mpc_config_addr; // Device No. and Reg No. included
uint32_t mpc_config_dat;  // Value to write to the device

uint32_t mpc106_write_word;
uint32_t mpc106_read_word;
uint16_t mpc106_write_half;
uint16_t mpc106_read_half;
uint8_t mpc106_write_byte;
uint8_t mpc106_read_byte;

void mpc106_init(){
    mpc_config_addr = 0;
    mpc_config_dat = 0;

    //Initialize Vendor & Device IDs
    machine_fexxxx_mem[0] = 0x57;
    machine_fexxxx_mem[1] = 0x10;

    machine_fexxxx_mem[2] = 0x02;

    //PCI command + status
    machine_fexxxx_mem[4] = 0x06;
    machine_fexxxx_mem[6] = 0x80;

    machine_fexxxx_mem[115] = 0xCD;

    machine_fexxxx_mem[168] = 0x10;
    machine_fexxxx_mem[169] = 0x00;
    machine_fexxxx_mem[170] = 0x00;
    machine_fexxxx_mem[171] = 0xFF;

    machine_fexxxx_mem[172] = 0x0C;
    machine_fexxxx_mem[173] = 0x06;
    machine_fexxxx_mem[174] = 0x0C;
    machine_fexxxx_mem[175] = 0x00;
}

uint32_t mpc106_write_device(uint32_t device_addr, uint32_t insert_to_device, uint8_t bit_length){
    //Write to the specified device - Invoked when a write is made to 0xFEExxxxx.
    //device_addr is what's stored in 0xFEC00CF8.

    uint32_t reg_num  = (device_addr & 0x07FC) >> 2;
    uint32_t dev_num  = (device_addr & 0xF800) >> 11;
    uint32_t location = reg_num + (dev_num << 11) + 0x3000000;

    switch (bit_length){
    case 1:
        machine_iocontrolmem_mem[location] = (uint8_t)insert_to_device;
    case 2:
        machine_iocontrolmem_mem[location++] = (uint8_t)insert_to_device;
        machine_iocontrolmem_mem[location] = (uint8_t)(insert_to_device >> 8);
    case 4:
        machine_iocontrolmem_mem[location++] = (uint8_t)insert_to_device;
        machine_iocontrolmem_mem[location++] = (uint8_t)(insert_to_device >> 8);
        machine_iocontrolmem_mem[location++] = (uint8_t)(insert_to_device >> 16);
        machine_iocontrolmem_mem[location] = (uint8_t)(insert_to_device >> 24);
    }

    return 0;
}

uint32_t mpc106_read_device(uint32_t device_addr, uint8_t bit_length){
    //Read to the specified device - Invoked when a read is made to 0xFEExxxxx.
    //device_addr is what's stored in 0xFEC00CF8.

    uint32_t reg_num = (device_addr & 0x07FC) >> 2;
    uint32_t dev_num = (device_addr & 0xF800) >> 11;
    uint32_t location = reg_num + (dev_num << 11) + 0x3000000;

    uint32_t grab_value = 0;

    switch (bit_length){
    case 1:
        grab_value |= ((uint32_t)(machine_iocontrolmem_mem[location++]));
    case 2:
        grab_value |= ((uint32_t)(machine_iocontrolmem_mem[location++]));
        grab_value |= ((uint32_t)(machine_iocontrolmem_mem[location++])) << 8;
    case 4:
        grab_value |= ((uint32_t)(machine_iocontrolmem_mem[location++]));
        grab_value |= ((uint32_t)(machine_iocontrolmem_mem[location++])) << 8;
        grab_value |= ((uint32_t)(machine_iocontrolmem_mem[location++])) << 16;
        grab_value |= ((uint32_t)(machine_iocontrolmem_mem[location])) << 24;
    }

    return grab_value;
}

void mpc106_read(){

    uint8_t read_length = 4;

    switch (mpc106_address){
    case 0x8:
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
    case 0x80:
    case 0x84:
    case 0x88:
    case 0x8C:
    case 0x90:
    case 0x94:
    case 0x98:
    case 0x9C:
    case 0xF0:
    case 0xF4:
    case 0xF8:
    case 0xFC:
        read_length = 4;
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

    switch (mpc106_address){
    case 0x8:
    case 0x70:
    case 0x72:
    case 0x73:
    case 0xA0:
        write_length = 1;
        break;
    case 0x0:
    case 0x2:
    case 0x4:
    case 0x6:
        write_length = 2;
        break;
    case 0x80:
    case 0x84:
    case 0x88:
    case 0x8C:
    case 0x90:
    case 0x94:
    case 0x98:
    case 0x9C:
    case 0xF0:
    case 0xF4:
    case 0xF8:
    case 0xFC:
        write_length = 4;
    }

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
