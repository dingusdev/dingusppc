//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

//Functionality for the OPENPIC

#include <iostream>
#include <cstring>
#include <cinttypes>
#include "ppcemumain.h"

uint32_t openpic_address;
uint32_t openpic_write_word;
uint32_t openpic_read_word;

void openpic_init(){
    for (int i = 0x40004; i < 0x7FFFF; i++){
        if (i % 16 == 0){
            i += 4;
        }
        machine_upperiocontrol_mem[i] = 0xFF;
    }
}

void openpic_read(){
    machine_upperiocontrol_mem[openpic_address] = openpic_read_word;
}

void openpic_write(){
    openpic_write_word = machine_upperiocontrol_mem[openpic_address];
}
