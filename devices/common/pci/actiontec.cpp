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

#include <devices/common/pci/actiontec.h>
#include <memaccess.h>

#include <cinttypes>

enum {
    PCI_VENDOR_ACTIONTEC   = 0x1668, // Actiontec Electronics Inc
};

ActiontecBridge::ActiontecBridge(std::string name) : PCIBridge(name)
{
    supports_types(HWCompType::PCI_HOST | HWCompType::PCI_DEV);

    // set up PCI configuration space header
    /* 00 */ this->vendor_id        = PCI_VENDOR_ACTIONTEC;
    /* 02 */ this->device_id        = 0x0100; // Mini-PCI bridge
    /* 04 */ this->command          = 0x0004; // 0004 = Bus Master
    /* 06 */ this->status           = 0x0290; // 0000  0 01 0  1 0 0 1  0 000 : Capabilities list, Fast Back to Back, DevSel Speed: Medium
    /* 08 */ this->class_rev        = (0x060400 << 8) | 0x11; // Bridge:PCI bridge:Normal decode; revision 11
    /* 0C */ this->cache_ln_sz      = 0x08; // 32 bytes
    /* 0D */ this->lat_timer        = 0x20; // 20 PCI clocks
//  /* 0E */ this->hdr_type
//  /* 0F */ this->bist
//  /* 10 */ this->bars_cfg[0]
//  /* 14 */ this->bars_cfg[1]
//  /* 18 */ this->primary_bus
//  /* 19 */ this->secondary_bus
//  /* 1A */ this->subordinate_bus
//  /* 1B */ this->sec_latency_timer
//  /* 1C */ this->io_base
//  /* 1D */ this->io_limit
    /* 1E */ this->sec_status       = 0x0280; // Fast Back to Back, DevSel Speed: medium
//  /* 20 */ this->memory_base
//  /* 22 */ this->memory_limit
//  /* 24 */ this->pref_memory_base
//  /* 26 */ this->pref_memory_limit
//  /* 28 */ this->pref_base_upper32
//  /* 2C */ this->pref_limit_upper32
//  /* 30 */ this->io_base_upper16
//  /* 32 */ this->io_limit_upper16
    /* 34 */ this->cap_ptr          = 0x80;
//  /* 38 */ this->exp_rom_bar
//  /* 3C */ this->irq_line
    /* 3D */ this->irq_pin          = 0x00;
//  /* 3E */ this->bridge_control   = 0x0004; // ISA Enable
}

uint32_t ActiontecBridge::pci_cfg_read(uint32_t reg_offs, AccessDetails &details)
{
    if (reg_offs < 0x40) {
        return PCIBridge::pci_cfg_read(reg_offs, details);
    }

    switch (reg_offs) {
        case 0x40: return 0x00000001;

        case 0x6C: return 0x0A000000;

        case 0x80: return 0xF6029001;
        case 0x84: return 0x00C00000;
            // +0: 01 = PCI Power Management
            // +1: 90 = next capability
            // +2: F602 = 1111 0 1 1 000 0 0 0 010 : Power Management version 2; Flags: PMEClk- DSI- D1+ D2+ AuxCurrent=0mA PME(D0-,D1+,D2+,D3hot+,D3cold+)
            // +4: 0000 = 0 00 0000 0 000000 00    : Status: D0 NoSoftRst- PME-Enable- DSel=0 DScale=0 PME-
            // +6: C0 = 1 1 000000                 : Bridge: PM+ B3-
            // +7: 00                              : Data

        case 0x90: return 0x00000006;
            // +0: 06 = CompactPCI hot-swap <?>
    }
    LOG_READ_UNIMPLEMENTED_CONFIG_REGISTER();
    return 0;
}

void ActiontecBridge::pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details)
{
    if (reg_offs < 0x40) {
        PCIBridge::pci_cfg_write(reg_offs, value, details);
        return;
    }
    LOG_WRITE_UNIMPLEMENTED_CONFIG_REGISTER();
}

static const DeviceDescription Actiontec_Descriptor = {
    ActiontecBridge::create, {}, {}
};

REGISTER_DEVICE(ActiontecBridge, Actiontec_Descriptor);
