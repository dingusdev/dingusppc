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
uint32_t openpic_prev_address;

bool openpic_int_go;

//OPENPIC ADDRESS MAP goes like this...

//(Assuming that it's placed at 0x80040000
//Per Processor Registers [private access] 0x40000 - 0x40fff
//Global registers                         0x41000 - 0x4ffff
//Interrupt Source Configuration Registers 0x50000 - 0x5ffff
//Per Processor Registers [global access]  0x60000 - 0x7ffff

//Taken from FreeBSD source code

void openpic_init(){
	machine_upperiocontrol_mem[0x40000] = 0x14;
	machine_upperiocontrol_mem[0x40001] = 0x10;
	machine_upperiocontrol_mem[0x40002] = 0x46;
	machine_upperiocontrol_mem[0x40003] = 0x00;

	machine_upperiocontrol_mem[0x40007] = 0x02;

    for (int i = 0x41004; i < 0x7FFFF; i++){
        if (i % 16 == 0){
            i += 4;
        }
        machine_upperiocontrol_mem[i] = 0xFF;
    }

	machine_upperiocontrol_mem[0x41000] = 0x02;
	machine_upperiocontrol_mem[0x41002] = 0x80;

	machine_upperiocontrol_mem[0x41080] = 0x14;
	machine_upperiocontrol_mem[0x41081] = 0x46;
	machine_upperiocontrol_mem[0x41090] = 0x01;
	machine_upperiocontrol_mem[0x410E0] = 0xFF;
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

    switch(openpic_address){
        //Make sure we don't touch any of the following read-only regs
        case 0x41000:
        case 0x41080:
        case 0x41100:
        case 0x41140:
        case 0x41180:
        case 0x411C0:
        case 0x600A0:
        case 0x600B0:
        case 0x610A0:
        case 0x610B0:
        case 0x620A0:
        case 0x620B0:
        case 0x630A0:
        case 0x630B0:
            return;
        default:
			if (openpic_address == openpic_prev_address){
				return;
			}
			else{
				openpic_prev_address = openpic_address;
			}
            machine_upperiocontrol_mem[openpic_address++] = (uint8_t)(openpic_write_word);
            machine_upperiocontrol_mem[openpic_address++] = (uint8_t)((openpic_write_word) >> 8);
            machine_upperiocontrol_mem[openpic_address++] = (uint8_t)((openpic_write_word) >> 16);
            machine_upperiocontrol_mem[openpic_address] = (uint8_t)((openpic_write_word) >> 24);
            break;
    }

	switch (op_interrupt_check){
		case 0x40:
			printf("IPI 0 stuff goes here! \n");
			break;
		case 0x50:
			printf("IPI 1 stuff goes here! \n");
			break;
		case 0x60:
			printf("IPI 2 stuff goes here! \n");
			break;
		case 0x70:
			printf("IPI 3 stuff goes here! \n");
			break;
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
