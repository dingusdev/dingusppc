//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

//Functionality for the Swim 3 Floppy Drive

#include <iostream>
#include <cinttypes>
#include "macswim3.h"
#include "ppcemumain.h"

uint32_t mac_swim3_address;
uint8_t swim3_write_byte;
uint8_t swim3_read_byte;

void mac_swim3_init(){
    machine_iocontrolmem_mem[0x3015C00] = 0xF0;
}

void mac_swim3_read(){
    swim3_read_byte = machine_iocontrolmem_mem[mac_swim3_address];
}

void mac_swim3_write(){
     machine_iocontrolmem_mem[mac_swim3_address] = swim3_write_byte;
}
