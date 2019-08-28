//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)


#include <iostream>
#include <cinttypes>
#include <vector>
#include "viacuda.h"

using namespace std;


ViaCuda::ViaCuda()
{
    this->via_regs[VIA_A]    = 0x80;
    this->via_regs[VIA_DIRB] = 0xFF;
    this->via_regs[VIA_DIRA] = 0xFF;
    this->via_regs[VIA_T1LL] = 0xFF;
    this->via_regs[VIA_T1LH] = 0xFF;
    this->via_regs[VIA_IER]  = 0x7F;
}

uint8_t ViaCuda::read(int reg)
{
    uint8_t res;

    cout << "Read VIA reg " << hex << (uint32_t)reg << endl;

    res = this->via_regs[reg & 0xF];

    /* reading from some VIA registers triggers special actions */
    switch(reg & 0xF) {
    case VIA_IER:
        res |= 0x80; /* bit 7 always reads as "1" */
    }

    return res;
}

void ViaCuda::write(int reg, uint8_t value)
{
    switch(reg & 0xF) {
    case VIA_B:
        cout << "VIA_B = " << hex << (uint32_t)value << endl;
        break;
    case VIA_A:
        cout << "VIA_A = " << hex << (uint32_t)value << endl;
        break;
    case VIA_DIRB:
        cout << "VIA_DIRB = " << hex << (uint32_t)value << endl;
        break;
    case VIA_DIRA:
        cout << "VIA_DIRA = " << hex << (uint32_t)value << endl;
        break;
    case VIA_PCR:
        cout << "VIA_PCR = " << hex << (uint32_t)value << endl;
        break;
    case VIA_ACR:
        cout << "VIA_ACR = " << hex << (uint32_t)value << endl;
        break;
    case VIA_IER:
        this->via_regs[VIA_IER] = (value & 0x80) ? value & 0x7F
                                  : this->via_regs[VIA_IER] & ~value;
        cout << "VIA_IER updated to " << hex << (uint32_t)this->via_regs[VIA_IER]
             << endl;
        print_enabled_ints();
        break;
    case VIA_ANH:
        cout << "VIA_ANH = " << hex << (uint32_t)value << endl;
        break;
    default:
        this->via_regs[reg & 0xF] = value;
    }
}

void ViaCuda::print_enabled_ints()
{
    vector<string> via_int_src = {"CA2", "CA1", "SR", "CB2", "CB1", "T2", "T1"};

    for (int i = 0; i < 7; i++) {
        if (this->via_regs[VIA_IER] & (1 << i))
            cout << "VIA " << via_int_src[i] << " interrupt enabled." << endl;
    }
}
