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

/** MPC106 (Grackle) emulation

    Grackle IC is a combined memory and PCI controller manufactured by Motorola.
    It's the central device in the Gossamer architecture.
    Manual: https://www.nxp.com/docs/en/reference-manual/MPC106UM.pdf

    This code emulates as much functionality as needed to run PowerMac Beige G3.
    This implies that
    - we only support address map B
    - our virtual device reports revision 4.0 as expected by machine firmware
*/

#ifndef MPC106_H_
#define MPC106_H_

#include <devices/common/mmiodevice.h>
#include <devices/common/pci/pcidevice.h>
#include <devices/common/pci/pcihost.h>
#include <devices/memctrl/memctrlbase.h>

#include <cinttypes>
#include <memory>
#include <unordered_map>

class MPC106 : public MemCtrlBase, public PCIDevice, public PCIHost {
public:
    MPC106();
    ~MPC106() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<MPC106>(new MPC106());
    }

    uint32_t read(uint32_t rgn_start, uint32_t offset, int size);
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size);

    int device_postinit();

protected:
    /* PCI access */
    void cfg_setup(uint32_t offset, int size, int &bus_num, int &dev_num, int &fun_num, uint8_t &reg_offs, AccessDetails &details, PCIDevice *&device);

    /* my own PCI configuration registers access */
    uint32_t pci_cfg_read(uint32_t reg_offs, AccessDetails &details);
    void pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details);

    void setup_ram(void);

private:
    uint8_t my_pci_cfg_hdr[256] = {
#define _ 0xFF,
/* 00 */    0x57, 0x10,             // vendor ID: Motorola
/* 02 */    0x02, 0x00,             // device ID: MPC106
/* 04 */    0x06, 0x00,             // PCI command
/* 06 */    0x80, 0x00,             // PCI status
/* 08 */    0x40,                   // revision ID: 4.0
/* 09 */    0x00,                   // standard programming
/* 0A */    0x00,                   // subclass code: host bridge
/* 0B */    0x06,                   // class code: bridge device
/* 0C */    0x08,                   // cache line size
/* 0D */    0x00,                   // latency timer
/* 0E */    0x00,                   // header type
/* 0F */    0x00,                   // BIST Control
/* 10 */    _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
/* 20 */    _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
/* 30 */    _ _ _ _ _ _ _ _ _ _ _ _
/* 3C */    0x00,                   // Interrupt line
/* 3D */    0x00,                   // Interrupt pin
/* 3E */    0x00,                   // MIN GNT
/* 3F */    0x00,                   // MAX LAT
/* 40 */    0x00,                   // Bus number
/* 41 */    0x00,                   // Subordinate bus number
/* 42 */    0x00,                   // Discount counter
/* 43 */    _ _ _ _ _
/* 48 */    0x00, 0x00, 0x00, 0x00, // Performance monitor command
/* 4C */    0x00, 0x00,             // Performance monitor mode control
/* 4E */    _ _
/* 50 */    0x00, 0x00, 0x00, 0x00, // Performance monitor counter 0
/* 54 */    0x00, 0x00, 0x00, 0x00, // Performance monitor counter 1
/* 58 */    0x00, 0x00, 0x00, 0x00, // Performance monitor counter 2
/* 5C */    0x00, 0x00, 0x00, 0x00, // Performance monitor counter 3
/* 60 */    _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
/* 70 */    0x00, 0x00,             // Power mgt config 1
/* 72 */    0x00,                   // Power mgt config 2
/* 73 */    0xCD,                   // Output driver control
/* 74 */    _ _ _ _ _ _ _ _ _ _ _ _
/* 80 */    0, 0, 0, 0, 0, 0, 0, 0, // Memory Starting Address
/* 88 */    0, 0, 0, 0, 0, 0, 0, 0, // Extended Memory Starting Address
/* 90 */    0, 0, 0, 0, 0, 0, 0, 0, // Memory Ending Address
/* 98 */    0, 0, 0, 0, 0, 0, 0, 0, // Extended Memory Ending Address
/* A0 */    0x00,                   // Memory bank enable
/* A1 */    _ _
/* A3 */    0x00,                   // Memory page mode
/* A4 */    _ _ _ _
/* A8 */    0x10, 0x00, 0x00, 0xFF, // PICR1
/* AC */    0x0C, 0x06, 0x0C, 0x00, // PICR2
/* B0 */    _ _ _ _ _ _ _ _
/* B8 */    0x00,                   // ECC single-bit error counter
/* B9 */    0x00,                   // ECC single-bit error trigger
/* BA */    0x04,                   // Alternate OS visible paramaters 1
/* BB */    0x01,                   // Alternate OS visible paramaters 2
/* BC */    _ _ _ _
/* C0 */    0x01,                   // Error enabling 1
/* C1 */    0x00,                   // Error detection 1
/* C2 */    _
/* C3 */    0x00,                   // 60x bus error status
/* C4 */    0x00,                   // Error enabling 2
/* C5 */    0x00,                   // Error detection 2
/* C6 */    _
/* C7 */    0x00,                   // PCI bus error status
/* C8 */    0x00, 0x00, 0x00, 0x00, // 60x/PCI ERROR address
/* CC */    _ _ _ _
/* D0 */    _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
/* E0 */    0x42, 0x00, 0xFF, 0x0F, // Emulation support config 1
/* E4 */    0x00, 0x00, 0x00, 0x00, // Modified memory status (no clear)
/* E8 */    0x20, 0x00, 0x00, 0x00, // Emulation support config 2
/* EC */    0x00, 0x00, 0x00, 0x00, // Modified memory status (clear)
/* F0 */    0x00, 0x00, 0x02, 0xFF, // Memory ctrl config 1
/* F4 */    0x03, 0x00, 0x00, 0x00, // Memory ctrl config 2
/* F8 */    0x00, 0x00, 0x00, 0x00, // Memory ctrl config 3
/* FC */    0x00, 0x00, 0x10, 0x00  // Memory ctrl config 4
#undef _
    };

    uint32_t config_addr;
};

#endif
