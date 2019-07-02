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

uint32_t mpc106_address;
uint32_t mpc_config_addr;
uint32_t mpc_config_dat;

uint32_t mpc106_write_word;
uint32_t mpc106_read_word;
uint16_t mpc106_write_half;
uint16_t mpc106_read_half;
uint8_t mpc106_write_byte;
uint8_t mpc106_read_byte;
unsigned char* mpc106_regs;

void mpc106_init(){
    mpc106_regs = (unsigned char*) calloc (256, 1);
    memset(mpc106_regs, 0x0, 256);

    //Initialize Vendor & Device IDs
    mpc106_regs[0] = 0x57;
    mpc106_regs[1] = 0x10;

    mpc106_regs[2] = 0x02;

    //PCI command + status
    mpc106_regs[4] = 0x06;
    mpc106_regs[6] = 0x80;

    mpc106_regs[115] = 0xCD;

    mpc106_regs[168] = 0x10;
    mpc106_regs[169] = 0x00;
    mpc106_regs[170] = 0x00;
    mpc106_regs[171] = 0xFF;

    mpc106_regs[172] = 0x0C;
    mpc106_regs[173] = 0x06;
    mpc106_regs[174] = 0x0C;
    mpc106_regs[175] = 0x00;
}

void mpc106_write_device(uint32_t value){
}

void mpc106_read_device(uint32_t value, uint8_t bit_length){
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
