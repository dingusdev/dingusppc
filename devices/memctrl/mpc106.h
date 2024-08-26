/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-24 divingkatae and maximum
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

/** MPC106 (Grackle) definitions.

    Grackle IC is a combined memory and PCI controller manufactured by Motorola.
    It's the central device in the Gossamer architecture.
    Manual: https://www.nxp.com/docs/en/reference-manual/MPC106UM.pdf

    This code emulates as much functionality as needed to run Power Mac Beige G3.
    This implies that
    - we only support address map B
    - our virtual device reports revision 4.0 as expected by machine firmware
*/

#ifndef MPC106_H
#define MPC106_H

#include <devices/common/pci/pcidevice.h>
#include <devices/common/pci/pcihost.h>
#include <devices/memctrl/memctrlbase.h>
#include <machines/machinebase.h>

#include <cinttypes>
#include <memory>

class InterruptCtrl;

/** Grackle configuration space registers. */
enum GrackleReg : uint32_t {
    CFG10   = 0x40, // bus # + subordinate bus # + disconnect counter
    PMCR1   = 0x70, // power management config 1
    MSAR1   = 0x80, // memory starting address 1
    MSAR2   = 0x84, // memory starting address 2
    EMSAR1  = 0x88, // extended memory starting address 1
    EMSAR2  = 0x8C, // extended memory starting address 2
    MEAR1   = 0x90, // memory ending address 1
    MEAR2   = 0x94, // memory ending address 2
    EMEAR1  = 0x98, // extended memory ending address 1
    EMEAR2  = 0x9C, // extended memory ending address 2
    MBER    = 0xA0, // memory bank enable
    PICR1   = 0xA8, // processor interface configuration 1
    PICR2   = 0xAC, // processor interface configuration 2
    MCCR1   = 0xF0, // memory control configuration 1
    MCCR2   = 0xF4, // memory control configuration 2
    MCCR3   = 0xF8, // memory control configuration 3
    MCCR4   = 0xFC  // memory control configuration 4
};

/* MCCR1 bit definitions. */
enum {
    MEMGO = 1 << 19,
};

class MPC106 : public MemCtrlBase, public PCIDevice, public PCIHost {
public:
    MPC106();
    ~MPC106() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<MPC106>(new MPC106());
    }

    uint32_t read(uint32_t rgn_start, uint32_t offset, int size) override;
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) override;

    int device_postinit() override;

protected:
    /* my own PCI configuration registers access */
    uint32_t pci_cfg_read(uint32_t reg_offs, AccessDetails &details) override;
    void pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details) override;

    void setup_ram(void);

private:
    inline void cfg_setup(uint32_t offset, int size, int &bus_num, int &dev_num,
                          int &fun_num, uint8_t &reg_offs, AccessDetails &details,
                          PCIBase *&device);

    uint32_t config_addr;

    uint16_t pmcr1 = 0;          // power management config 1
    uint8_t  pmcr2 = 0;          // power management config 2
    uint8_t  odcr  = 0xCD;       // output driver control
    uint32_t picr1 = 0xFF100010; // ROM on CPU bus, address map B, CPU type = MPC601
    uint32_t picr2 = 0x000C060C;
    uint32_t mccr1 = 0xFF820000; // 64bit ROM bus
    uint32_t mccr2 = 3;
    uint32_t mccr3 = 0;
    uint32_t mccr4 = 0x00100000;

    uint32_t mem_start[2]       = {};
    uint32_t ext_mem_start[2]   = {};
    uint32_t mem_end[2]         = {};
    uint32_t ext_mem_end[2]     = {};
    uint8_t  mem_bank_en        = 0;
};

#endif // MPC106_H
