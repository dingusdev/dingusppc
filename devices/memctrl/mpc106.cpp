/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-23 divingkatae and maximum
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
#include <loguru.hpp>
#include <memaccess.h>

#include <cinttypes>
#include <cstring>
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

    // assign PCI device number zero to myself
    this->pci_register_device(DEV_FUN(0,0), this);

    // add PCI/ISA I/O space, 64K for now
    add_mmio_region(0xFE000000, 0x10000, this);

    // add memory mapped I/O region for MPC106 registers
    add_mmio_region(0xFEC00000, 0x300000, this);
}

int MPC106::device_postinit()
{
    std::string pci_dev_name;

    static const std::map<std::string, int> pci_slots = {
        {"pci_A1", DEV_FUN(0xD,0)}, {"pci_B1", DEV_FUN(0xE,0)}, {"pci_C1", DEV_FUN(0xF,0)}
    };

    for (auto& slot : pci_slots) {
        pci_dev_name = GET_STR_PROP(slot.first);
        if (!pci_dev_name.empty()) {
            this->attach_pci_device(pci_dev_name, slot.second, std::string("@") + slot.first);
        }
    }
    return 0;
}

uint32_t MPC106::read(uint32_t rgn_start, uint32_t offset, int size) {
    if (rgn_start == 0xFE000000) {
        return pci_io_read_broadcast(offset, size);
    }
    if (offset < 0x200000) {
        return this->config_addr;
    }
    if (this->config_addr & 0x80) {    // process only if bit E (enable) is set
        int bus_num, dev_num, fun_num;
        uint8_t reg_offs;
        AccessDetails details;
        PCIBase *device;
        cfg_setup(offset, size, bus_num, dev_num, fun_num, reg_offs, details, device);
        details.flags |= PCI_CONFIG_READ;
        if (device) {
            uint32_t value = device->pci_cfg_read(reg_offs, details);
            // bytes 0 to 3 repeat
            return pci_conv_rd_data(value, value, details);
        }
        LOG_READ_NON_EXISTENT_PCI_DEVICE();
        return 0xFFFFFFFFUL; // PCI spec §6.1
    }
    return 0;
}

void MPC106::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) {
    if (rgn_start == 0xFE000000) {
        pci_io_write_broadcast(offset, size, value);
        return;
    }
    if (offset < 0x200000) {
        this->config_addr = value;
        return;
    }
    if (this->config_addr & 0x80) {    // process only if bit E (enable) is set
        int bus_num, dev_num, fun_num;
        uint8_t reg_offs;
        AccessDetails details;
        PCIBase *device;
        cfg_setup(offset, size, bus_num, dev_num, fun_num, reg_offs, details, device);
        details.flags |= PCI_CONFIG_WRITE;
        if (device) {
            if (size == 4 && !details.offset) { // aligned DWORD writes -> fast path
                device->pci_cfg_write(reg_offs, BYTESWAP_32(value), details);
                return;
            }
            // otherwise perform necessary data transformations -> slow path
            uint32_t old_val = details.size == 4 ? 0 : device->pci_cfg_read(reg_offs, details);
            uint32_t new_val = pci_conv_wr_data(old_val, value, details);
            device->pci_cfg_write(reg_offs, new_val, details);
            return;
        }
        LOG_WRITE_NON_EXISTENT_PCI_DEVICE();
    }
}

inline void MPC106::cfg_setup(uint32_t offset, int size, int &bus_num, int &dev_num,
                              int &fun_num, uint8_t &reg_offs, AccessDetails &details,
                              PCIBase *&device)
{
    device = NULL;
    details.size = size;
    details.offset = offset & 3;

    bus_num  = (this->config_addr >>  8) & 0xFF;
    dev_num  = (this->config_addr >> 19) & 0x1F;
    fun_num  = (this->config_addr >> 16) & 0x07;
    reg_offs = (this->config_addr >> 24) & 0xFC;

    if (bus_num) {
        details.flags = PCI_CONFIG_TYPE_1;
        device = pci_find_device(bus_num, dev_num, fun_num);
    }
    else {
        details.flags = PCI_CONFIG_TYPE_0;
        if (this->dev_map.count(DEV_FUN(dev_num, fun_num))) {
            device = this->dev_map[DEV_FUN(dev_num, fun_num)];
        }
    }
}

uint32_t MPC106::pci_cfg_read(uint32_t reg_offs, AccessDetails &details) {
    if (reg_offs < 64) {
        return PCIDevice::pci_cfg_read(reg_offs, details);
    }

    switch (reg_offs) {
    case GrackleReg::CFG10:
        return 0;
    case GrackleReg::PMCR1:
        return (this->odcr << 24) | (this->pmcr2 << 16) | this->pmcr1;
    case GrackleReg::MSAR1:
    case GrackleReg::MSAR2:
        return this->mem_start[(reg_offs >> 2) & 1];
    case GrackleReg::EMSAR1:
    case GrackleReg::EMSAR2:
        return this->ext_mem_start[(reg_offs >> 2) & 1];
    case GrackleReg::MEAR1:
    case GrackleReg::MEAR2:
        return this->mem_end[(reg_offs >> 2) & 1];
    case GrackleReg::EMEAR1:
    case GrackleReg::EMEAR2:
        return this->ext_mem_end[(reg_offs >> 2) & 1];
    case GrackleReg::MBER:
        return this->mem_bank_en;
    case GrackleReg::PICR1:
        return this->picr1;
    case GrackleReg::PICR2:
        return this->picr2;
    case GrackleReg::MCCR1:
        return this->mccr1;
    case GrackleReg::MCCR2:
        return this->mccr2;
    case GrackleReg::MCCR3:
        return this->mccr3;
    case GrackleReg::MCCR4:
        return this->mccr4;
    default:
        LOG_READ_UNIMPLEMENTED_CONFIG_REGISTER();
    }

    return 0; // PCI Spec §6.1
}

void MPC106::pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details) {
    if (reg_offs < 64) {
        PCIDevice::pci_cfg_write(reg_offs, value, details);
        return;
    }

    switch (reg_offs) {
    case GrackleReg::CFG10:
        // Open Firmware writes 0 to subordinate bus # - we don't care
        break;
    case GrackleReg::PMCR1:
        this->pmcr1 = value & 0xFFFFU;
        this->pmcr2 = (value >> 16) & 0xFF;
        this->odcr  = value >> 24;
        break;
    case GrackleReg::MSAR1:
    case GrackleReg::MSAR2:
        this->mem_start[(reg_offs >> 2) & 1] = value;
        break;
    case GrackleReg::EMSAR1:
    case GrackleReg::EMSAR2:
        this->ext_mem_start[(reg_offs >> 2) & 1] = value;
        break;
    case GrackleReg::MEAR1:
    case GrackleReg::MEAR2:
        this->mem_end[(reg_offs >> 2) & 1] = value;
        break;
    case GrackleReg::EMEAR1:
    case GrackleReg::EMEAR2:
        this->ext_mem_end[(reg_offs >> 2) & 1] = value;
        break;
    case GrackleReg::MBER:
        this->mem_bank_en = value & 0xFFU;
        break;
    case GrackleReg::PICR1:
        this->picr1 = value;
        break;
    case GrackleReg::PICR2:
        this->picr2 = value;
        break;
    case GrackleReg::MCCR1:
        if ((value ^ this->mccr1) & MEMGO) {
            if (value & MEMGO)
                setup_ram();
        }
        this->mccr1 = value;
        break;
    case GrackleReg::MCCR2:
        this->mccr2 = value;
        break;
    case GrackleReg::MCCR3:
        this->mccr3 = value;
        break;
    case GrackleReg::MCCR4:
        this->mccr4 = value;
        break;
    default:
        LOG_WRITE_UNIMPLEMENTED_CONFIG_REGISTER();
    }
}

void MPC106::setup_ram() {
    uint32_t mem_start, mem_end, ext_mem_start, ext_mem_end, bank_start, bank_end;
    uint32_t ram_size = 0;

    for (int bank = 0; bank < 8; bank++) {
        if (this->mem_bank_en & (1 << bank)) {
            if (bank < 4) {
                mem_start     = this->mem_start[0];
                ext_mem_start = this->ext_mem_start[0];
                mem_end       = this->mem_end[0];
                ext_mem_end   = this->ext_mem_end[0];
            } else {
                mem_start     = this->mem_start[1];
                ext_mem_start = this->ext_mem_start[1];
                mem_end       = this->mem_end[1];
                ext_mem_end   = this->ext_mem_end[1];
            }
            bank_start = (((ext_mem_start >> bank * 8) & 3) << 30) |
                (((mem_start >> bank * 8) & 0xFF) << 20);
            bank_end = (((ext_mem_end >> bank * 8) & 3) << 30) |
                (((mem_end >> bank * 8) & 0xFF) << 20) | 0xFFFFFUL;
            if (bank && bank_start != ram_size)
                LOG_F(WARNING, "Grackle: RAM not contiguous!");
            ram_size += bank_end - bank_start + 1;
        }
    }

    if (!this->add_ram_region(0, ram_size)) {
        LOG_F(WARNING, "Grackle: RAM allocation 0x%X..0x%X failed (maybe already exists?)",
              0, ram_size - 1);
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
