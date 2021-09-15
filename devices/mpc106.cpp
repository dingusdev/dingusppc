/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-20 divingkatae and maximum
                      (theweirdo)     spatium

(Contact divingkatae#1017 or powermax#2286 on Discord for more info)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/** MPC106 (Grackle) emulation

    Author: Max Poliakovski
*/

#include <cinttypes>
#include <cstring>
#include <iostream>
#include <loguru.hpp>

#include "hwcomponent.h"
#include "memctrlbase.h"
#include "memaccess.h"
#include "mmiodevice.h"
#include "mpc106.h"


MPC106::MPC106() : MemCtrlBase(), PCIDevice("Grackle PCI host bridge") {
    this->name = "Grackle";

    /* add PCI/ISA I/O space, 64K for now */
    add_mmio_region(0xFE000000, 0x10000, this);

    /* add memory mapped I/O region for MPC106 registers */
    add_mmio_region(0xFEC00000, 0x300000, this);

    this->pci_0_bus.clear();
    this->io_space_devs.clear();
}

MPC106::~MPC106() {
    this->pci_0_bus.clear();
}

bool MPC106::supports_type(HWCompType type) {
    if (type == HWCompType::MEM_CTRL || type == HWCompType::MMIO_DEV ||
        type == HWCompType::PCI_HOST || type == HWCompType::PCI_DEV) {
        return true;
    } else {
        return false;
    }
}

uint32_t MPC106::read(uint32_t reg_start, uint32_t offset, int size) {
    uint32_t result;

    if (reg_start == 0xFE000000) {
        /* broadcast I/O request to devices that support I/O space
           until a device returns true that means "request accepted" */
        for (auto& dev : this->io_space_devs) {
            if (dev->pci_io_read(offset, size, &result)) {
                return result;
            }
        }
        LOG_F(ERROR, "Attempt to read from unmapped PCI I/O space, offset=0x%X", offset);
    } else {
        if (offset >= 0x200000) {
            if (this->config_addr & 0x80)    // process only if bit E (enable) is set
                return pci_read(size);
        }
    }

    /* FIXME: reading from CONFIG_ADDR is ignored for now */

    return 0;
}

void MPC106::write(uint32_t reg_start, uint32_t offset, uint32_t value, int size) {
    if (reg_start == 0xFE000000) {
        /* broadcast I/O request to devices that support I/O space
           until a device returns true that means "request accepted" */
        for (auto& dev : this->io_space_devs) {
            if (dev->pci_io_write(offset, value, size)) {
                return;
            }
        }
        LOG_F(ERROR, "Attempt to write to unmapped PCI I/O space, offset=0x%X", offset);
    } else {
        if (offset < 0x200000) {
            this->config_addr = value;
        } else {
            if (this->config_addr & 0x80)    // process only if bit E (enable) is set
                return pci_write(value, size);
        }
    }
}

uint32_t MPC106::pci_read(uint32_t size) {
    int bus_num, dev_num, fun_num, reg_offs;

    bus_num = (this->config_addr >> 8) & 0xFF;
    if (bus_num) {
        LOG_F(
            ERROR,
            "%s err: read attempt from non-local PCI bus, config_addr = %x \n",
            this->name.c_str(),
            this->config_addr);
        return 0;
    }

    dev_num  = (this->config_addr >> 19) & 0x1F;
    fun_num  = (this->config_addr >> 16) & 0x07;
    reg_offs = (this->config_addr >> 24) & 0xFC;

    if (dev_num == 0 && fun_num == 0) {    // dev_num 0 is assigned to myself
        return this->pci_cfg_read(reg_offs, size);
    } else {
        if (this->pci_0_bus.count(dev_num)) {
            return this->pci_0_bus[dev_num]->pci_cfg_read(reg_offs, size);
        } else {
            LOG_F(
                ERROR,
                "%s err: read attempt from non-existing PCI device %d \n",
                this->name.c_str(),
                dev_num);
            return 0;
        }
    }

    return 0;
}

void MPC106::pci_write(uint32_t value, uint32_t size) {
    int bus_num, dev_num, fun_num, reg_offs;

    bus_num = (this->config_addr >> 8) & 0xFF;
    if (bus_num) {
        LOG_F(
            ERROR,
            "%s err: write attempt to non-local PCI bus, config_addr = %x \n",
            this->name.c_str(),
            this->config_addr);
        return;
    }

    dev_num  = (this->config_addr >> 19) & 0x1F;
    fun_num  = (this->config_addr >> 16) & 0x07;
    reg_offs = (this->config_addr >> 24) & 0xFC;

    if (dev_num == 0 && fun_num == 0) {    // dev_num 0 is assigned to myself
        this->pci_cfg_write(reg_offs, value, size);
    } else {
        if (this->pci_0_bus.count(dev_num)) {
            this->pci_0_bus[dev_num]->pci_cfg_write(reg_offs, value, size);
        } else {
            LOG_F(
                ERROR,
                "%s err: write attempt to non-existing PCI device %d \n",
                this->name.c_str(),
                dev_num);
        }
    }
}

uint32_t MPC106::pci_cfg_read(uint32_t reg_offs, uint32_t size) {
#ifdef MPC106_DEBUG
    LOG_F(9, "read from Grackle register %08X\n", reg_offs);
#endif

    return read_mem(&this->my_pci_cfg_hdr[reg_offs], size);
}

void MPC106::pci_cfg_write(uint32_t reg_offs, uint32_t value, uint32_t size) {
#ifdef MPC106_DEBUG
    LOG_F(9, "write %08X to Grackle register %08X\n", value, reg_offs);
#endif

    // FIXME: implement write-protection for read-only registers

    write_mem(&this->my_pci_cfg_hdr[reg_offs], value, size);

    if (this->my_pci_cfg_hdr[0xF2] & 8) {
#ifdef MPC106_DEBUG
        LOG_F(9, "MPC106: MCCR1[MEMGO] was set! \n");
#endif
        setup_ram();
    }
}

bool MPC106::pci_register_device(int dev_num, PCIDevice* dev_instance) {
    if (this->pci_0_bus.count(dev_num))    // is dev_num already registered?
        return false;

    this->pci_0_bus[dev_num] = dev_instance;

    dev_instance->set_host(this);

    if (dev_instance->supports_io_space()) {
        this->io_space_devs.push_back(dev_instance);
    }

    return true;
}

bool MPC106::pci_register_mmio_region(uint32_t start_addr, uint32_t size, PCIDevice* obj) {
    // FIXME: add sanity checks!
    return this->add_mmio_region(start_addr, size, obj);
}

void MPC106::setup_ram() {
    uint32_t mem_start, mem_end, ext_mem_start, ext_mem_end, bank_start, bank_end;
    uint32_t ram_size = 0;

    uint8_t bank_en = this->my_pci_cfg_hdr[0xA0];

    for (int bank = 0; bank < 8; bank++) {
        if (bank_en & (1 << bank)) {
            if (bank < 4) {
                mem_start     = READ_DWORD_LE_A(&this->my_pci_cfg_hdr[0x80]);
                ext_mem_start = READ_DWORD_LE_A(&this->my_pci_cfg_hdr[0x88]);
                mem_end       = READ_DWORD_LE_A(&this->my_pci_cfg_hdr[0x90]);
                ext_mem_end   = READ_DWORD_LE_A(&this->my_pci_cfg_hdr[0x98]);
            } else {
                mem_start     = READ_DWORD_LE_A(&this->my_pci_cfg_hdr[0x84]);
                ext_mem_start = READ_DWORD_LE_A(&this->my_pci_cfg_hdr[0x8C]);
                mem_end       = READ_DWORD_LE_A(&this->my_pci_cfg_hdr[0x94]);
                ext_mem_end   = READ_DWORD_LE_A(&this->my_pci_cfg_hdr[0x9C]);
            }
            bank_start = (((ext_mem_start >> bank * 8) & 3) << 30) |
                (((mem_start >> bank * 8) & 0xFF) << 20);
            bank_end = (((ext_mem_end >> bank * 8) & 3) << 30) |
                (((mem_end >> bank * 8) & 0xFF) << 20) | 0xFFFFFUL;
            if (bank && bank_start != ram_size)
                LOG_F(WARNING, "MPC106: RAM not contiguous! \n");
            ram_size += bank_end - bank_start + 1;
        }
    }

    if (!this->add_ram_region(0, ram_size)) {
        LOG_F(ERROR, "MPC106 RAM allocation failed! \n");
    }
}
