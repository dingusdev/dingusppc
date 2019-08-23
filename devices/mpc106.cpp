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
        return READ_WORD_BE(&this->my_pci_cfg_hdr[reg_offs]);
        break;
    case 4:
        return READ_DWORD_BE(&this->my_pci_cfg_hdr[reg_offs]);
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
