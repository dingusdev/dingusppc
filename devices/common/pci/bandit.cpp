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

Bandit::Bandit(int bridge_num, std::string name) : PCIHost(), PCIDevice(name)
{
    this->base_addr = 0xF0000000 + ((bridge_num & 3) << 25);

    MemCtrlBase *mem_ctrl = dynamic_cast<MemCtrlBase *>
                           (gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));

    // add memory mapped I/O region for Bandit control registers
    // This region has the following layout:
    // base_addr +  0x000000 --> I/O space
    // base_addr +  0x800000 --> CONFIG_ADDR
    // base_addr +  0xC00000 --> CONFIG_DATA
    // base_addr + 0x1000000 --> pass-through memory space (not included below)
    mem_ctrl->add_mmio_region(base_addr, 0x01000000, this);

    // prepare the PCI config header
    this->vendor_id   = PCI_VENDOR_APPLE;
    this->device_id   = 0x0001;
    this->class_rev   = 0x06000003;
    this->cache_ln_sz = 8;
    this->command     = 0x16;

    // make several PCI config space registers read-only
    this->pci_wr_cmd = [](uint16_t cmd) {}; // command register
    this->pci_wr_cache_lnsz = [](uint8_t val) {}; // cache line size register
}

uint32_t Bandit::read(uint32_t reg_start, uint32_t offset, int size)
{
    int      fun_num;
    uint8_t  reg_offs;
    uint32_t result, idsel;

    if (offset & BANDIT_CONFIG_SPACE) {
        if (offset & 0x00400000) {
            // access to the CONFIG_DATA pseudo-register causes a Config Cycle
            if (this->config_addr & BANDIT_CAR_TYPE) {
                LOG_F(WARNING, "%s: config cycle type 1 not supported yet", this->name.c_str());
                return 0;
            }

            idsel    = (this->config_addr >> 11) & 0x1FFFFFU;
            fun_num  = (this->config_addr >> 8) & 7;
            reg_offs = this->config_addr & 0xFCU;

            if (!SINGLE_BIT_SET(idsel)) {
                LOG_F(ERROR, "%s: invalid IDSEL=0x%X passed", this->name.c_str(), idsel);
                return 0;
            }

            if (idsel == BANDIT_ID_SEL) { // access to myself
                result = this->pci_cfg_read(reg_offs, size);
            } else {
                if (this->dev_map.count(idsel)) {
                    result = this->dev_map[idsel]->pci_cfg_read(reg_offs, size);
                } else {
                    LOG_F(
                        ERROR,
                        "%s err: read attempt from non-existing PCI device %d \n",
                        this->name.c_str(),
                        idsel);
                    result = 0;
                }
            }
        } else {
            result = this->config_addr;
        }
    } else { // I/O space access
        LOG_F(WARNING, "%s: I/O space write not implemented yet", this->name.c_str());
    }
    return result;
}

void Bandit::write(uint32_t reg_start, uint32_t offset, uint32_t value, int size)
{
    int      fun_num;
    uint8_t  reg_offs;
    uint32_t idsel;

    if (offset & BANDIT_CONFIG_SPACE) {
        if (offset & 0x00400000) {
            // access to the CONFIG_DATA pseudo-register causes a Config Cycle
            if (this->config_addr & BANDIT_CAR_TYPE) {
                LOG_F(WARNING, "%s: config cycle type 1 not supported yet", this->name.c_str());
                return;
            }

            idsel    = (this->config_addr >> 11) & 0x1FFFFFU;
            fun_num  = (this->config_addr >> 8) & 7;
            reg_offs = this->config_addr & 0xFCU;

            if (!SINGLE_BIT_SET(idsel)) {
                LOG_F(ERROR, "%s: invalid IDSEL=0x%X passed", this->name.c_str(), idsel);
                return;
            }

            if (idsel == BANDIT_ID_SEL) { // access to myself
                this->pci_cfg_write(reg_offs, value, size);
                return;
            }

            if (this->dev_map.count(idsel)) {
                this->dev_map[idsel]->pci_cfg_write(reg_offs, value, size);
            } else {
                LOG_F(
                    ERROR,
                    "%s err: write attempt to non-existing PCI device %d \n",
                    this->name.c_str(),
                    idsel);
            }
        } else {
            this->config_addr = BYTESWAP_32(value);
        }
    } else { // I/O space access
        LOG_F(WARNING, "%s: I/O space write not implemented yet", this->name.c_str());
    }
}
