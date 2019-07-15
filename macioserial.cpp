//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

//Functionality for the Serial Ports
// Based on the Zilog Z8530 SCC chip

#include <iostream>
#include <cinttypes>
#include "macioserial.h"
#include "ppcemumain.h"

uint32_t mac_serial_address;
uint8_t serial_write_byte;
uint8_t serial_read_byte;

bool w7_prime;

void mac_serial_init(){
    machine_iocontrolmem_mem[0x3013004] = 0x04;
    machine_iocontrolmem_mem[0x3013009] = 0xC0;
    machine_iocontrolmem_mem[0x301300B] = 0x08;
    machine_iocontrolmem_mem[0x301300E] = 0x30;
    machine_iocontrolmem_mem[0x301300F] = 0xF8;
    machine_iocontrolmem_mem[0x3013010] = 0x44;
    machine_iocontrolmem_mem[0x3013011] = 0x06;
}

void mac_serial_read(){
    if (mac_serial_address >=0x3013020){
        mac_serial_address -= 0x20;
    }

    if (w7_prime){
        if (((machine_iocontrolmem_mem[0x301301F] >> 2) & 0x1) & ((machine_iocontrolmem_mem[0x3013007] >> 6) & 0x1)){
            switch(mac_serial_address){
            case 0x14: case 0x15:
                serial_read_byte = machine_iocontrolmem_mem[(mac_serial_address - 0x10)];
                break;
            case 0x19:
                serial_read_byte = machine_iocontrolmem_mem[0x3];
                break;
            case 0x1B:
                serial_read_byte = machine_iocontrolmem_mem[0x10];
                break;
            case 0x1E:
                serial_read_byte = machine_iocontrolmem_mem[0x07];
                break;
            default:
                serial_read_byte = machine_iocontrolmem_mem[mac_serial_address];
                break;
            }
        }
    }
    else{
        if ((machine_iocontrolmem_mem[0x301300F] >> 2) & 0x1){
            switch(mac_serial_address){
            case 0x14: case 0x15:
                serial_read_byte = machine_iocontrolmem_mem[(mac_serial_address - 4)];
                break;
            case 0x19: case 0x1B:
                serial_read_byte = machine_iocontrolmem_mem[(mac_serial_address + 4)];
                break;
            default:
                serial_read_byte = machine_iocontrolmem_mem[mac_serial_address];
                break;
            }
        }
        else {
            switch(mac_serial_address){
            case 0x14: case 0x15: case 0x16: case 0x17:
                serial_read_byte = machine_iocontrolmem_mem[(mac_serial_address - 4)];
            case 0x19: case 0x1B:
                serial_read_byte = machine_iocontrolmem_mem[(mac_serial_address + 4)];
                break;
            default:
                serial_read_byte = machine_iocontrolmem_mem[mac_serial_address];
                break;
            }
        }
    }
}

void mac_serial_write(){
    if (mac_serial_address >=0x3013020){
        mac_serial_address -= 0x20;
    }
    machine_iocontrolmem_mem[mac_serial_address] = serial_write_byte;

    if ((machine_iocontrolmem_mem[0x0F]) & 0x1){
        w7_prime = true;
    }
    else{
        w7_prime = false;
    }

    machine_iocontrolmem_mem[(mac_serial_address + 0x10)] = machine_iocontrolmem_mem[mac_serial_address];

}
