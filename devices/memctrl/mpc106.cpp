/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-22 divingkatae and maximum
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

/** MPC106 (Grackle) emulation. */

#include <devices/common/hwcomponent.h>
#include <devices/common/mmiodevice.h>
#include <devices/deviceregistry.h>
#include <devices/memctrl/memctrlbase.h>
#include <devices/memctrl/mpc106.h>
#include <memaccess.h>

#include <cinttypes>
#include <cstring>
#include <iostream>
#include <loguru.hpp>
#include <string>
#include <vector>

MPC106::MPC106() : MemCtrlBase(), PCIDevice("Grackle"), PCIHost()
{
    supports_types(HWCompType::MEM_CTRL | HWCompType::MMIO_DEV |
                   HWCompType::PCI_HOST | HWCompType::PCI_DEV);

    // populate PCI config header
    this->vendor_id   = PCI_VENDOR_MOTOROLA;
    this->device_id   = 0x0002;
    this->class_rev   = 0x06000040;
    this->cache_ln_sz = 8;
    this->command     = 6;
    this->status      = 0x80;

    // add PCI/ISA I/O space, 64K for now
    add_mmio_region(0xFE000000, 0x10000, this);

    // add memory mapped I/O region for MPC106 registers
    add_mmio_region(0xFEC00000, 0x300000, this);
}

int MPC106::device_postinit()
{
    std::string pci_dev_name;

    static const std::map<std::string, int> pci_slots = {
        {"pci_A1", 0xD}, {"pci_B1", 0xE}, {"pci_C1", 0xF}
    };

    for (auto& slot : pci_slots) {
        pci_dev_name = GET_STR_PROP(slot.first);
        if (!pci_dev_name.empty()) {
            this->attach_pci_device(pci_dev_name, slot.second);
        }
    }
    return 0;
}

uint32_t MPC106::read(uint32_t rgn_start, uint32_t offset, int size) {
    uint32_t result;

    if (rgn_start == 0xFE000000) {
        // broadcast I/O request to devices that support I/O space
        // until a device returns true that means "request accepted"
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

    // FIXME: reading from CONFIG_ADDR is ignored for now

    return 0;
}

void MPC106::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) {
    if (rgn_start == 0xFE000000) {
        // broadcast I/O request to devices that support I/O space
        // until a device returns true that means "request accepted"
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
    dev_num  = (this->config_addr >> 19) & 0x1F;
    fun_num  = (this->config_addr >> 16) & 0x07;
    reg_offs = (this->config_addr >> 24) & 0xFC;

    if (bus_num) {
		LOG_F(
			ERROR,
			"%s err: read attempt from non-local PCI bus, config_addr = %x %02x:%02x.%x @%02x.%c",
			this->name.c_str(), this->config_addr, bus_num, dev_num, fun_num, reg_offs,
			size == 4 ? 'l' : size == 2 ? 'w' : size == 1 ? 'b' : '0' + size
		);
        return 0xFFFFFFFFUL; // PCI spec §6.1
    }

    if (dev_num == 0 && fun_num == 0) {    // dev_num 0 is assigned to myself
        return this->pci_cfg_read(reg_offs, size);
    } else {
        if (this->dev_map.count(dev_num)) {
            return this->dev_map[dev_num]->pci_cfg_read(reg_offs, size);
        } else {
            LOG_F(
                ERROR,
                "%s err: read attempt from non-existing PCI device %02x:%02x.%x @%02x.%c",
                this->name.c_str(), bus_num, dev_num, fun_num, reg_offs,
                size == 4 ? 'l' : size == 2 ? 'w' : size == 1 ? 'b' : '0' + size
            );
            return 0xFFFFFFFFUL; // PCI spec §6.1
        }
    }

    return 0;
}

void MPC106::pci_write(uint32_t value, uint32_t size) {
    int bus_num, dev_num, fun_num, reg_offs;

    bus_num = (this->config_addr >> 8) & 0xFF;
    dev_num  = (this->config_addr >> 19) & 0x1F;
    fun_num  = (this->config_addr >> 16) & 0x07;
    reg_offs = (this->config_addr >> 24) & 0xFC;

    if (bus_num) {
        LOG_F(
            ERROR,
            "%s err: write attempt to non-local PCI bus, config_addr = %x %02x:%02x.%x @%02x.%c = %0*x",
			this->name.c_str(), this->config_addr, bus_num, dev_num, fun_num, reg_offs,
			size == 4 ? 'l' : size == 2 ? 'w' : size == 1 ? 'b' : '0' + size,
            size * 2, BYTESWAP_SIZED(value, size)
        );
        return;
    }

    if (dev_num == 0 && fun_num == 0) {    // dev_num 0 is assigned to myself
        this->pci_cfg_write(reg_offs, value, size);
    } else {
        if (this->dev_map.count(dev_num)) {
            this->dev_map[dev_num]->pci_cfg_write(reg_offs, value, size);
        } else {
            LOG_F(
                ERROR,
                "%s err: write attempt to non-existing PCI device %02x:%02x.%x @%02x.%c = %0*x",
                this->name.c_str(), bus_num, dev_num, fun_num, reg_offs,
                size == 4 ? 'l' : size == 2 ? 'w' : size == 1 ? 'b' : '0' + size,
                size * 2, BYTESWAP_SIZED(value, size)
            );
        }
    }
}

uint32_t MPC106::pci_cfg_read(uint32_t reg_offs, uint32_t size) {
#ifdef MPC106_DEBUG
    LOG_F(9, "read from Grackle register %08X", reg_offs);
#endif

    if (reg_offs < 64) {
        return PCIDevice::pci_cfg_read(reg_offs, size);
    }

    return read_mem(&this->my_pci_cfg_hdr[reg_offs], size);
}

void MPC106::pci_cfg_write(uint32_t reg_offs, uint32_t value, uint32_t size) {
#ifdef MPC106_DEBUG
    LOG_F(9, "write %08X to Grackle register %08X", value, reg_offs);
#endif

    if (reg_offs < 64) {
        PCIDevice::pci_cfg_write(reg_offs, value, size);
        return;
    }

    // FIXME: implement write-protection for read-only registers

    write_mem(&this->my_pci_cfg_hdr[reg_offs], value, size);

    if (this->my_pci_cfg_hdr[0xF2] & 8) {
#ifdef MPC106_DEBUG
        LOG_F(9, "MPC106: MCCR1[MEMGO] was set!");
#endif
        setup_ram();
    }
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
                LOG_F(WARNING, "MPC106: RAM not contiguous!");
            ram_size += bank_end - bank_start + 1;
        }
    }

    if (!this->add_ram_region(0, ram_size)) {
        LOG_F(WARNING, "MPC106 RAM allocation 0x%X..0x%X failed (maybe already exists?)", 0, ram_size - 1);
    }
}

static const PropMap Grackle_Properties = {
    {"pci_A1",
        new StrProperty("")},
    {"pci_B1",
        new StrProperty("")},
    {"pci_C1",
        new StrProperty("")},
};

static const DeviceDescription Grackle_Descriptor = {
    MPC106::create, {}, Grackle_Properties
};

REGISTER_DEVICE(Grackle, Grackle_Descriptor);
