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

/** Bandit ARBus-to-PCI Bridge emulation. */

#include "bandit.h"
#include <devices/memctrl/memctrlbase.h>
#include <endianswap.h>
#include <loguru.hpp>
#include <machines/machinebase.h>
#include <memaccess.h>

#include <cinttypes>

Bandit::Bandit(int bridge_num, std::string name) : MMIODevice(), PCIHost()
{
    this->base_addr = 0xF0000000 + ((bridge_num & 3) << 25);
    this->name      = name;

    MemCtrlBase *mem_ctrl = dynamic_cast<MemCtrlBase *>
                           (gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));

    // add memory mapped I/O region for Bandit control registers
    // This region has the following format:
    // base_addr +  0x000000 --> I/O space
    // base_addr +  0x800000 --> CONFIG_ADDR
    // base_addr +  0xC00000 --> CONFIG_DATA
    // base_addr + 0x1000000 - pass-through memory space (not included below)
    mem_ctrl->add_mmio_region(base_addr, 0x01000000, this);

    // prepare the PCI config header
    std::memset(this->my_pci_cfg_hdr, 0, sizeof(this->my_pci_cfg_hdr));
    WRITE_DWORD_BE_A(&this->my_pci_cfg_hdr[0x00], 0x0001106B);
    WRITE_DWORD_BE_A(&this->my_pci_cfg_hdr[0x08], 0x06000003);

    // mask array for protecting accesses to the PCI config header
    std::memset(this->my_cfg_mask, 0xFF, sizeof(this->my_pci_cfg_hdr));
    WRITE_DWORD_BE_A(&this->my_cfg_mask[0x00], 0); // make DeviceID/VendorID read-only
    WRITE_DWORD_BE_A(&this->my_cfg_mask[0x08], 0); // make ClassCode/Revision read-only
    WRITE_DWORD_BE_A(&this->my_cfg_mask[0x0C], 0xFF00FFFFUL); // make HeaderType read-only
}

uint32_t Bandit::read(uint32_t reg_start, uint32_t offset, int size)
{
    int dev_num, dev_fn;
    uint8_t reg_offs;
    uint32_t result;

    if (offset & BANDIT_CONFIG_SPACE) {
        if (offset & 0x00400000) {
            // access to the CONFIG_DATA pseudo-register causes a Config Cycle
            if (this->config_addr & BANDIT_DEV_NUM) { // access to myself
                result = this->pci_cfg_read(this->config_addr & 0xFFU, size);
            } else {
                dev_num  = this->config_addr >> 12;
                dev_fn   = this->config_addr >>  8;
                reg_offs = this->config_addr & 0xFFU;

                if (this->dev_map.count(dev_num)) {
                    result = this->dev_map[dev_num]->pci_cfg_read(reg_offs, size);
                } else {
                    LOG_F(
                        ERROR,
                        "%s err: read attempt from non-existing PCI device %d \n",
                        this->name.c_str(),
                        dev_num);
                    result = 0;
                }
            }
        } else {
            result = this->config_addr;
        }
    } else { // I/O space access
        LOG_F(WARNING, "%s: I/O space write not implemented yet", this->name.c_str());
    }
    return BYTESWAP_32(result);
}

void Bandit::write(uint32_t reg_start, uint32_t offset, uint32_t value, int size)
{
    int dev_num, dev_fn;
    uint8_t reg_offs;

    if (offset & BANDIT_CONFIG_SPACE) {
        if (offset & 0x00400000) {
            // access to the CONFIG_DATA pseudo-register causes a Config Cycle
            if (this->config_addr & BANDIT_DEV_NUM) { // access to myself
                this->pci_cfg_write(this->config_addr & 0xFFU, value, size);
                return;
            }

            dev_num  = this->config_addr >> 12;
            dev_fn   = this->config_addr >>  8;
            reg_offs = this->config_addr & 0xFFU;

            if (this->dev_map.count(dev_num)) {
                this->dev_map[dev_num]->pci_cfg_write(reg_offs, value, size);
            } else {
                LOG_F(
                    ERROR,
                    "%s err: write attempt to non-existing PCI device %d \n",
                    this->name.c_str(),
                    dev_num);
            }
        } else {
            this->config_addr = BYTESWAP_32(value) & 0x3FFFFF;
        }
    } else { // I/O space access
        LOG_F(WARNING, "%s: I/O space write not implemented yet", this->name.c_str());
    }
}

uint32_t Bandit::pci_cfg_read(uint32_t reg_offs, uint32_t size)
{
    if ((reg_offs & 3) || size != 4) {
        LOG_F(WARNING, "%s: non-DWORD access to the config space", this->name.c_str());
    }

    return read_mem_rev(&this->my_pci_cfg_hdr[reg_offs], size);
}

void Bandit::pci_cfg_write(uint32_t reg_offs, uint32_t value, uint32_t size)
{
    if ((reg_offs & 3) || size != 4) {
        LOG_F(WARNING, "%s: non-DWORD access to the config space", this->name.c_str());
    }

    uint32_t mask = read_mem_rev(&my_cfg_mask[reg_offs], size);
    uint32_t cur_val = read_mem_rev(&this->my_pci_cfg_hdr[reg_offs], size);
    write_mem_rev(&this->my_pci_cfg_hdr[reg_offs], (cur_val & ~mask) | (value & mask), size);
}
