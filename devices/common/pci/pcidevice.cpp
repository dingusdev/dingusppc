/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-25 divingkatae and maximum
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

#include <devices/common/pci/pcidevice.h>
#include <loguru.hpp>

#include <cinttypes>
#include <fstream>
#include <cstring>
#include <string>

PCIDevice::PCIDevice(std::string name) : PCIBase(name, PCI_HEADER_TYPE_0, 6)
{
}

uint32_t PCIDevice::pci_cfg_read(uint32_t reg_offs, AccessDetails &details)
{
    switch (reg_offs) {
    case PCI_CFG_BAR0:
    case PCI_CFG_BAR1:
    case PCI_CFG_BAR2:
    case PCI_CFG_BAR3:
    case PCI_CFG_BAR4:
    case PCI_CFG_BAR5:
        return this->bars[(reg_offs - 0x10) >> 2];
    case PCI_CFG_SUBSYS_ID:
        return (this->subsys_id << 16) | (this->subsys_vndr);
    case PCI_CFG_ROM_BAR:
        return this->exp_rom_bar;
    case PCI_CFG_DWORD_13:
        return cap_ptr;
    case PCI_CFG_DWORD_15:
        return (max_lat << 24) | (min_gnt << 16) | (irq_pin << 8) | irq_line;
    default:
        return PCIBase::pci_cfg_read(reg_offs, details);
    }
}

void PCIDevice::pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details)
{
    switch (reg_offs) {
    case PCI_CFG_BAR0:
    case PCI_CFG_BAR1:
    case PCI_CFG_BAR2:
    case PCI_CFG_BAR3:
    case PCI_CFG_BAR4:
    case PCI_CFG_BAR5:
        this->set_bar_value((reg_offs - 0x10) >> 2, value);
        break;
    case PCI_CFG_ROM_BAR:
        this->pci_wr_exp_rom_bar(value);
        break;
    case PCI_CFG_DWORD_15:
        this->irq_line = value >> 24;
        break;
    default:
        PCIBase::pci_cfg_write(reg_offs, value, details);
    }
}
