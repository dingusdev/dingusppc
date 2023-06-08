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

#ifndef PCI_DEVICE_H
#define PCI_DEVICE_H

#include <devices/common/pci/pcibase.h>

/** PCI configuration space registers offsets */
enum {
    PCI_CFG_BAR2      = 0x18, // base address register 2
    PCI_CFG_BAR3      = 0x1C, // base address register 3
    PCI_CFG_BAR4      = 0x20, // base address register 4
    PCI_CFG_BAR5      = 0x24, // base address register 5
    PCI_CFG_CIS_PTR   = 0x28, // Cardbus CIS Pointer
    PCI_CFG_SUBSYS_ID = 0x2C, // Subsysten IDs
    PCI_CFG_ROM_BAR   = 0x30, // expansion ROM base address
};

class PCIDevice : public PCIBase {
public:
    PCIDevice(std::string name);
    virtual ~PCIDevice() = default;

    // configuration space access methods
    virtual uint32_t pci_cfg_read(uint32_t reg_offs, AccessDetails &details);
    virtual void pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details);

protected:
    // PCI configuration space state
    uint8_t     max_lat = 0;
    uint8_t     min_gnt = 0;
    uint16_t    subsys_id = 0;
    uint16_t    subsys_vndr = 0;
};

#endif /* PCI_DEVICE_H */
