//DingusPPC
//Written by divingkatae and maximum
//(c)2018-20 (theweirdo)     spatium
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 or powermax#2286 on Discord)

/** MPC106 (Grackle) emulation

    Author: Max Poliakovski
*/

#include <thirdparty/loguru.hpp>
#include <iostream>
#include <cstring>
#include <cinttypes>

#include "memreadwrite.h"
#include "memctrlbase.h"
#include "mmiodevice.h"
#include "mpc106.h"


MPC106::MPC106() : MemCtrlBase("Grackle"), PCIDevice("Grackle PCI host bridge")
{
    /* add memory mapped I/O region for MPC106 registers */
    add_mmio_region(0xFEC00000, 0x300000, this);

    this->pci_0_bus.clear();
}

MPC106::~MPC106()
{
    this->pci_0_bus.clear();
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
    int bus_num, dev_num, fun_num, reg_offs;

    bus_num = (this->config_addr >> 8) & 0xFF;
    if (bus_num) {
        std::cout << this->name << " err: read attempt from non-local PCI bus, "
            << "config_addr = " << std::hex << this->config_addr << std::endl;
        return 0;
    }

    dev_num  = (this->config_addr >> 19) & 0x1F;
    fun_num  = (this->config_addr >> 16) & 0x07;
    reg_offs = (this->config_addr >> 24) & 0xFC;

    if (dev_num == 0 && fun_num == 0) { // dev_num 0 is assigned to myself
        return this->pci_cfg_read(reg_offs, size);
    } else {
        if (this->pci_0_bus.count(dev_num)) {
            return this->pci_0_bus[dev_num]->pci_cfg_read(reg_offs, size);
        } else {
            std::cout << this->name << " err: read attempt from non-existing PCI device "
                << dev_num << std::endl;
            return 0;
        }
    }

    return 0;
}

void MPC106::pci_write(uint32_t value, uint32_t size)
{
    int bus_num, dev_num, fun_num, reg_offs;

    bus_num = (this->config_addr >> 8) & 0xFF;
    if (bus_num) {
        std::cout << this->name << " err: write attempt to non-local PCI bus, "
            << "config_addr = " << std::hex << this->config_addr << std::endl;
        return;
    }

    dev_num  = (this->config_addr >> 19) & 0x1F;
    fun_num  = (this->config_addr >> 16) & 0x07;
    reg_offs = (this->config_addr >> 24) & 0xFC;

    if (dev_num == 0 && fun_num == 0) { // dev_num 0 is assigned to myself
        this->pci_cfg_write(reg_offs, value, size);
    } else {
        if (this->pci_0_bus.count(dev_num)) {
            this->pci_0_bus[dev_num]->pci_cfg_write(reg_offs, value, size);
        } else {
            std::cout << this->name << " err: write attempt to non-existing PCI device "
                << dev_num << std::endl;
        }
    }
}

uint32_t MPC106::pci_cfg_read(uint32_t reg_offs, uint32_t size)
{
#ifdef MPC106_DEBUG
    printf("read from Grackle register %08X\n", reg_offs);
#endif

    switch(size) {
    case 1:
        return this->my_pci_cfg_hdr[reg_offs];
        break;
    case 2:
        return READ_WORD_BE_A(&this->my_pci_cfg_hdr[reg_offs]);
        break;
    case 4:
        return READ_DWORD_BE_A(&this->my_pci_cfg_hdr[reg_offs]);
        break;
    default:
        std::cout << "MPC106 read error: invalid size parameter " << size
            << std::endl;
    }

    return 0;
}

void MPC106::pci_cfg_write(uint32_t reg_offs, uint32_t value, uint32_t size)
{
#ifdef MPC106_DEBUG
    printf("write %08X to Grackle register %08X\n", value, reg_offs);
#endif

    // FIXME: implement write-protection for read-only registers
    switch(size) {
    case 1:
        this->my_pci_cfg_hdr[reg_offs] = value & 0xFF;
        break;
    case 2:
        this->my_pci_cfg_hdr[reg_offs]   = (value >> 8) & 0xFF;
        this->my_pci_cfg_hdr[reg_offs+1] = value & 0xFF;
        break;
    case 4:
        this->my_pci_cfg_hdr[reg_offs]   = (value >> 24) & 0xFF;
        this->my_pci_cfg_hdr[reg_offs+1] = (value >> 16) & 0xFF;
        this->my_pci_cfg_hdr[reg_offs+2] = (value >> 8)  & 0xFF;
        this->my_pci_cfg_hdr[reg_offs+3] = value & 0xFF;
        break;
    default:
        std::cout << "MPC106 read error: invalid size parameter " << size
            << std::endl;
    }

    if (this->my_pci_cfg_hdr[0xF2] & 8) {
#ifdef MPC106_DEBUG
        std::cout << "MPC106: MCCR1[MEMGO] was set! " << std::endl;
#endif
        setup_ram();
    }
}

bool MPC106::pci_register_device(int dev_num, PCIDevice *dev_instance)
{
    if (this->pci_0_bus.count(dev_num)) // is dev_num already registered?
        return false;

    this->pci_0_bus[dev_num] = dev_instance;

    dev_instance->set_host(this);

    return true;
}

bool MPC106::pci_register_mmio_region(uint32_t start_addr, uint32_t size, PCIDevice *obj)
{
    // FIXME: add sanity checks!
    return this->add_mmio_region(start_addr, size, obj);
}

void MPC106::setup_ram()
{
    uint32_t mem_start, mem_end, ext_mem_start, ext_mem_end, bank_start, bank_end;
    uint32_t ram_size = 0;

    uint8_t  bank_en = this->my_pci_cfg_hdr[0xA0];

    for (int bank = 0; bank < 8; bank++) {
        if (bank_en & (1 << bank)) {
            if (bank < 4) {
                mem_start = READ_DWORD_LE_A(&this->my_pci_cfg_hdr[0x80]);
                ext_mem_start = READ_DWORD_LE_A(&this->my_pci_cfg_hdr[0x88]);
                mem_end = READ_DWORD_LE_A(&this->my_pci_cfg_hdr[0x90]);
                ext_mem_end = READ_DWORD_LE_A(&this->my_pci_cfg_hdr[0x98]);
            } else {
                mem_start = READ_DWORD_LE_A(&this->my_pci_cfg_hdr[0x84]);
                ext_mem_start = READ_DWORD_LE_A(&this->my_pci_cfg_hdr[0x8C]);
                mem_end = READ_DWORD_LE_A(&this->my_pci_cfg_hdr[0x94]);
                ext_mem_end = READ_DWORD_LE_A(&this->my_pci_cfg_hdr[0x9C]);
            }
            bank_start = (((ext_mem_start >> bank * 8) & 3) << 30) |
                (((mem_start >> bank * 8) & 0xFF) << 20);
            bank_end = (((ext_mem_end >> bank * 8) & 3) << 30) |
                (((mem_end >> bank * 8) & 0xFF) << 20) | 0xFFFFFUL;
            if (bank && bank_start != ram_size)
                std::cout << "MPC106 error: RAM not contiguous!" << std::endl;
            ram_size += bank_end + 1;
        }
    }

    if (!this->add_ram_region(0, ram_size)) {
        std::cout << "MPC106 RAM allocation failed!" << std::endl;
    }
}
