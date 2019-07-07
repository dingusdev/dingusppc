//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

//Functionality for the DAVBus (Sound Bus + Screamer?)

#include <iostream>
#include <cstring>
#include <cinttypes>
#include "ppcemumain.h"

uint32_t davbus_address;
uint32_t davbus_write_word;
uint32_t davbus_read_word;

void davbus_init(){

}

void davbus_read(){
    davbus_read_word = (uint32_t)(machine_upperiocontrol_mem[davbus_address++]);
    davbus_read_word = (uint32_t)((machine_upperiocontrol_mem[davbus_address++]) << 8);
    davbus_read_word = (uint32_t)((machine_upperiocontrol_mem[davbus_address++]) << 16);
    davbus_read_word = (uint32_t)((machine_upperiocontrol_mem[davbus_address]) << 24);
}

void davbus_write(){
    machine_upperiocontrol_mem[davbus_address++] = (uint8_t)(davbus_write_word);
    machine_upperiocontrol_mem[davbus_address++] = (uint8_t)((davbus_write_word) >> 8);
    machine_upperiocontrol_mem[davbus_address++] = (uint8_t)((davbus_write_word) >> 16);
    machine_upperiocontrol_mem[davbus_address] = (uint8_t)((davbus_write_word) >> 24);
}
