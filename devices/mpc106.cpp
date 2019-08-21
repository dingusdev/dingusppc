//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

/** MPC106 (Grackle) emulation

    Author: Max Poliakovski
*/

#include <iostream>
#include <cstring>
#include <cinttypes>

#include "memctrlbase.h"
#include "mmiodevice.h"
#include "../viacuda.h"
#include "mpc106.h"
#include "../ppcemumain.h"
#include "../ppcmemory.h"


MPC106::MPC106() : MemCtrlBase("Grackle")
{
    /* add memory mapped I/O region for MPC106 registers */
    add_mmio_region(0xFEC00000, 0x300000, this);
}

MPC106::~MPC106()
{
}

uint32_t MPC106::read(uint32_t offset, int size)
{
    if (offset >= 0x200000) {
        if (this->config_addr & 0x80) // process only if bit E (enable) is set
            return pci_read(size);
    }

    /* FIXME: reading from CONFIG_ADDR is ignored for now */

    return 0;
}

void MPC106::write(uint32_t offset, uint32_t value, int size)
{
    if (offset < 0x200000) {
        this->config_addr = value;
    } else {
        if (this->config_addr & 0x80) // process only if bit E (enable) is set
            return pci_write(value, size);
    }
}

uint32_t MPC106::pci_read(uint32_t size)
{
    int bus_num, dev_num, fun_num, reg_num;

    bus_num = (this->config_addr >> 8) & 0xFF;
    if (bus_num) {
        std::cout << this->name << " err: read attempt from non-local PCI bus, "
            << "config_addr = " << std::hex << this->config_addr << std::endl;
        return 0;
    }

    dev_num = (this->config_addr >> 19) & 0x1F;
    fun_num = (this->config_addr >> 16) & 0x07;
    reg_num = (this->config_addr >> 24) & 0xFC;

    if (dev_num == 0 && fun_num == 0) { // dev_num 0 is assigned to myself
        return myself_read(reg_num, size);
    } else {
        std::cout << this->name << " err: reading from device " << dev_num
            << " not supported yet" << std::endl;
        return 0;
    }

    return 0;
}

void MPC106::pci_write(uint32_t value, uint32_t size)
{
    int bus_num, dev_num, fun_num, reg_num;

    bus_num = (this->config_addr >> 8) & 0xFF;
    if (bus_num) {
        std::cout << this->name << " err: write attempt to non-local PCI bus, "
            << "config_addr = " << std::hex << this->config_addr << std::endl;
        return;
    }

    dev_num = (this->config_addr >> 19) & 0x1F;
    fun_num = (this->config_addr >> 16) & 0x07;
    reg_num = (this->config_addr >> 24) & 0xFC;

    if (dev_num == 0 && fun_num == 0) { // dev_num 0 is assigned to myself
        myself_write(reg_num, value, size);
    } else {
        std::cout << this->name << " err: writing to device " << dev_num
            << " not supported yet" << std::endl;
    }
}

uint32_t MPC106::myself_read(int reg_num, uint32_t size)
{
#ifdef MPC106_DEBUG
    printf("read from Grackle register %08X\n", reg_num);
#endif

    switch(size) {
    case 1:
        return this->my_pci_cfg_hdr[reg_num];
        break;
    case 2:
        return READ_WORD_BE(&this->my_pci_cfg_hdr[reg_num]);
        break;
    case 4:
        return READ_DWORD_BE(&this->my_pci_cfg_hdr[reg_num]);
        break;
    default:
        std::cout << "MPC106 read error: invalid size parameter " << size
            << std::endl;
    }

    return 0;
}

void MPC106::myself_write(int reg_num, uint32_t value, uint32_t size)
{
#ifdef MPC106_DEBUG
    printf("write %08X to Grackle register %08X\n", value, reg_num);
#endif

    // FIXME: implement write-protection for read-only registers
    switch(size) {
    case 1:
        this->my_pci_cfg_hdr[reg_num] = value & 0xFF;
        break;
    case 2:
        this->my_pci_cfg_hdr[reg_num]   = (value >> 8) & 0xFF;
        this->my_pci_cfg_hdr[reg_num+1] = value & 0xFF;
        break;
    case 4:
        this->my_pci_cfg_hdr[reg_num]   = (value >> 24) & 0xFF;
        this->my_pci_cfg_hdr[reg_num+1] = (value >> 16) & 0xFF;
        this->my_pci_cfg_hdr[reg_num+2] = (value >> 8)  & 0xFF;
        this->my_pci_cfg_hdr[reg_num+3] = value & 0xFF;
        break;
    default:
        std::cout << "MPC106 read error: invalid size parameter " << size
            << std::endl;
    }
}
