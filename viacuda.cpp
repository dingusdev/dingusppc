//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

//Functionality for the VIA CUDA

#include <iostream>
#include <cinttypes>
#include "viacuda.h"
#include "ppcemumain.h"

uint32_t via_cuda_address;
uint32_t via_set_mode;

uint8_t via_opcode_store_bit;
uint8_t via_write_byte;
uint8_t via_read_byte;

unsigned char porta_ca1, porta_ca2 = 0;
unsigned char porta_cb1, porta_cb2 = 0;

bool via_cuda_confirm;
bool via_cuda_signal_read;
bool via_cuda_signal_write;

void via_ifr_update(){
    if ((machine_iocontrolmem_mem[VIACUDA_IFR] & 127) && (machine_iocontrolmem_mem[VIACUDA_IER] & 127)){
        machine_iocontrolmem_mem[VIACUDA_IFR] |= 128;
    }
    else{
        machine_iocontrolmem_mem[VIACUDA_IFR] &= 127;
    }
}

void via_t1_update(){
    if (machine_iocontrolmem_mem[VIACUDA_T1CH] > 0){
        machine_iocontrolmem_mem[VIACUDA_T1CH]--;
    }
    else{
        machine_iocontrolmem_mem[VIACUDA_T1CH] = machine_iocontrolmem_mem[VIACUDA_T1LH];
    }


    if (machine_iocontrolmem_mem[VIACUDA_T1CL] > 0){
        machine_iocontrolmem_mem[VIACUDA_T1CL]--;
    }
    else{
        machine_iocontrolmem_mem[VIACUDA_T1CL] = machine_iocontrolmem_mem[VIACUDA_T1LL];
    }
}

void via_cuda_init(){
    machine_iocontrolmem_mem[VIACUDA_A] = 0x80;
    machine_iocontrolmem_mem[VIACUDA_DIRB] = 0xFF;
    machine_iocontrolmem_mem[VIACUDA_DIRA] = 0xFF;
    machine_iocontrolmem_mem[VIACUDA_T1LL] = 0xFF;
    machine_iocontrolmem_mem[VIACUDA_T1LH] = 0xFF;
    machine_iocontrolmem_mem[VIACUDA_IER] = 0x7F;
}

void via_cuda_read(){
    switch(via_cuda_address){
        case VIACUDA_B:
        if (machine_iocontrolmem_mem[VIACUDA_DIRB] != 0){
            via_read_byte = (via_write_byte & ~machine_iocontrolmem_mem[VIACUDA_DIRB]) | (machine_iocontrolmem_mem[VIACUDA_B] & machine_iocontrolmem_mem[VIACUDA_DIRB]);
            break;
        }
        case VIACUDA_A:
            machine_iocontrolmem_mem[VIACUDA_IFR] = ~porta_ca1;

            via_read_byte = (machine_iocontrolmem_mem[VIACUDA_A] & ~machine_iocontrolmem_mem[VIACUDA_DIRA]) | (machine_iocontrolmem_mem[VIACUDA_A] & machine_iocontrolmem_mem[VIACUDA_DIRA]);
            break;
        case VIACUDA_IER:
            via_read_byte = machine_iocontrolmem_mem[VIACUDA_IER] | 0x80;
            break;
        case VIACUDA_ANH:
            via_read_byte = machine_iocontrolmem_mem[VIACUDA_A] & ~machine_iocontrolmem_mem[VIACUDA_DIRA];
            break;
        default:
            via_read_byte = machine_iocontrolmem_mem[via_cuda_address];
        }
    via_read_byte = machine_iocontrolmem_mem[via_cuda_address];
}

void via_cuda_write(){
    switch(via_cuda_address){
        case VIACUDA_B:
            machine_iocontrolmem_mem[VIACUDA_B] = via_write_byte & machine_iocontrolmem_mem[VIACUDA_DIRB];
            break;
        case VIACUDA_A:
            machine_iocontrolmem_mem[VIACUDA_IFR] = ~porta_ca1;
            if ((machine_iocontrolmem_mem[VIACUDA_PCR] & 0x0A) != 0x02){
                machine_iocontrolmem_mem[VIACUDA_IFR] = ~porta_ca2;
            }
            via_ifr_update();
            machine_iocontrolmem_mem[VIACUDA_A] = (via_write_byte & machine_iocontrolmem_mem[VIACUDA_DIRA]) | (~machine_iocontrolmem_mem[VIACUDA_DIRA]);
            break;
        case VIACUDA_DIRB:
            machine_iocontrolmem_mem[VIACUDA_DIRB] = via_write_byte;
            break;
        case VIACUDA_DIRA:
            machine_iocontrolmem_mem[VIACUDA_DIRA] = via_write_byte;
            break;
        case VIACUDA_T1LL:
        case VIACUDA_T1LH:
            via_t1_update();
            machine_iocontrolmem_mem[via_cuda_address] = via_write_byte;
            break;
        case VIACUDA_T2CH:
            via_t1_update();
            machine_iocontrolmem_mem[VIACUDA_T2CH] = via_write_byte;
            break;
        case VIACUDA_PCR:
            machine_iocontrolmem_mem[VIACUDA_PCR] = via_write_byte;
            if ((via_write_byte & 0x0E) == 0x0C){
                porta_ca2 = 0;
            }
            else if ((via_write_byte & 0x08)){
                porta_ca2 = 1;
            }

            if ((via_write_byte & 0xE0) == 0xC0){
                porta_cb2 = 0;
            }
            if ((via_write_byte & 0x80)){
                porta_cb2 = 1;
            }
            break;
        case VIACUDA_IFR:
            if (via_write_byte & 0x80){
                machine_iocontrolmem_mem[VIACUDA_IFR] &= (via_write_byte & 0x7F);
            }
            else{
                machine_iocontrolmem_mem[VIACUDA_IFR] &=  ~(via_write_byte & 0x7F);
            }
            via_ifr_update();
            break;
        case VIACUDA_IER:
            machine_iocontrolmem_mem[VIACUDA_IER] &= ~(via_write_byte & 0x7F);
            via_ifr_update();
            break;
        case VIACUDA_ANH:
            machine_iocontrolmem_mem[VIACUDA_A] = (via_write_byte & machine_iocontrolmem_mem[VIACUDA_DIRA]) | (~machine_iocontrolmem_mem[VIACUDA_DIRA]);
            break;
        default:
            machine_iocontrolmem_mem[via_cuda_address] = via_write_byte;

    }
    machine_iocontrolmem_mem[via_cuda_address] = via_write_byte;
}
