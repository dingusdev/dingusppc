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

bool openpic_int_go;

//OPENPIC ADDRESS MAP goes like this...

//(Assuming that it's placed at 0x80040000
//Per Processor Registers [private access] 0x40000 - 0x40fff
//Global registers                         0x41000 - 0x4ffff
//Interrupt Source Configuration Registers 0x50000 - 0x5ffff
//Per Processor Registers [global access]  0x60000 - 0x7ffff

//Taken from FreeBSD source code

void openpic_init(){
    for (int i = 0x40004; i < 0x7FFFF; i++){
        if (i % 16 == 0){
            i += 4;
        }
        machine_upperiocontrol_mem[i] = 0xFF;
    }

	machine_upperiocontrol_mem[0x40080] = 0x6B;
	machine_upperiocontrol_mem[0x40081] = 0x10;
	machine_upperiocontrol_mem[0x40082] = 0x10;
	machine_upperiocontrol_mem[0x40083] = 0x00;
}

void openpic_read(){
    if (openpic_address > 0x60000){
        openpic_address = (openpic_address % 0x8000) + 0x60000;
    }

    openpic_read_word = (uint32_t)(machine_upperiocontrol_mem[openpic_address++]);
    openpic_read_word = (uint32_t)((machine_upperiocontrol_mem[openpic_address++]) << 8);
    openpic_read_word = (uint32_t)((machine_upperiocontrol_mem[openpic_address++]) << 16);
    openpic_read_word = (uint32_t)((machine_upperiocontrol_mem[openpic_address]) << 24);

}

void openpic_write(){
    //Max of 128 interrupts per processor
    //Each interrupt line takes up 0x8000 bytes

	uint32_t op_interrupt_check = (openpic_address & 0xFF);

	//We have only one processor
    if (openpic_address > 0x60000){
        openpic_address = (openpic_address % 0x8000) + 0x60000;
    }

    machine_upperiocontrol_mem[openpic_address++] = (uint8_t)(openpic_write_word);
    machine_upperiocontrol_mem[openpic_address++] = (uint8_t)((openpic_write_word) >> 8);
    machine_upperiocontrol_mem[openpic_address++] = (uint8_t)((openpic_write_word) >> 16);
    machine_upperiocontrol_mem[openpic_address] = (uint8_t)((openpic_write_word) >> 24);

	switch (op_interrupt_check){
		case 0x80:
			printf("Task Priority Reg stuff goes here! \n");
			break;
		case 0x90:
			printf("WHOAMI stuff goes here! \n");
			break;
		case 0xA0:
			openpic_int_go = true;
			break;
		case 0xB0:
			openpic_int_go = false;
			break;
	}
}
