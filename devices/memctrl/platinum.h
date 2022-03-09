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

/** Platinum Memory Controller definitions.

    Author: Max Poliakovski

    Platium is a single chip memory subsystem controller designed especially
    for the Power Macintosh 7200 computer, code name Catalyst.
*/

#ifndef PLATINUM_MEMCTRL_H
#define PLATINUM_MEMCTRL_H

#include <devices/common/hwcomponent.h>
#include <devices/common/mmiodevice.h>
#include <devices/memctrl/memctrlbase.h>

#include <cinttypes>

namespace Platinum {

/** Clock source encoding in the CPUID register */
enum {
    ClkSrc0 = 0x0000, // 33 MHz
    ClkSrc2 = 0x8000, // 20 MHz
    ClkSrc3 = 0x8040  // 31,3344 MHz
};

/** CPU/Bus speed encoding for ClkSrc0 */
enum CpuSpeed0 {
     CPU_66_BUS_33   =  0,
     CPU_80_BUS_40_1 =  1,
     CPU_80_BUS_27_1 =  2,
    CPU_100_BUS_50   =  3,
    CPU_100_BUS_33   =  4,
    CPU_120_BUS_60   =  5,
    CPU_120_BUS_40   =  6,
    CPU_120_BUS_30   =  7,
    CPU_133_BUS_66   =  8,
    CPU_133_BUS_44   =  9,
    CPU_133_BUS_33   = 10,
    CPU_150_BUS_75   = 11,
    CPU_150_BUS_50   = 12,
    CPU_150_BUS_37   = 13
};

/** CPU/Bus speed encoding for ClkSrc2 */
enum CpuSpeed2 {
    CPU_40_BUS_20   =  0,
    CPU_48_BUS_24   =  1,
    CPU_48_BUS_16   =  2,
    CPU_60_BUS_30   =  3,
    CPU_60_BUS_20   =  4,
    CPU_72_BUS_36   =  5,
    CPU_72_BUS_24   =  6,
    CPU_72_BUS_18   =  7,
    CPU_80_BUS_40_2 =  8,
    CPU_80_BUS_27_2 =  9,
    CPU_80_BUS_20   = 10,
    CPU_90_BUS_45   = 11,
    CPU_90_BUS_30   = 12,
    CPU_90_BUS_22   = 13
};

/** CPU/Bus speed encoding for ClkSrc3 */
enum CpuSpeed3 {
     CPU_62_BUS_31 =  0,
     CPU_75_BUS_38 =  1, // actual bus frequency: 37500000 Hz
     CPU_75_BUS_25 =  2,
     CPU_94_BUS_47 =  3,
     CPU_94_BUS_31 =  4,
    CPU_113_BUS_56 =  5,
    CPU_113_BUS_38 =  6, // actual bus frequency: 37500000 Hz
    CPU_113_BUS_28 =  7,
    CPU_125_BUS_64 =  8, // actual bus frequency: 63500000 Hz
    CPU_125_BUS_42 =  9,
    CPU_125_BUS_31 = 10,
    CPU_141_BUS_72 = 11, // actual bus frequency: 71500000 Hz
    CPU_141_BUS_47 = 12,
    CPU_141_BUS_35 = 13
};

/** Configuration and status register offsets. */
enum PlatinumReg : uint32_t {
    CPU_ID        = 0x000,
    ASIC_REVISION = 0x010,
    ROM_TIMING    = 0x020,
    CACHE_CONFIG  = 0x030,
    DRAM_TIMING   = 0x040,
    DRAM_REFRESH  = 0x050,
    BANK_0_BASE   = 0x060,
    BANK_1_BASE   = 0x070,
    BANK_2_BASE   = 0x080,
    BANK_3_BASE   = 0x090,
    BANK_4_BASE   = 0x0A0,
    BANK_5_BASE   = 0x0B0,
    BANK_6_BASE   = 0x0C0,
    BANK_7_BASE   = 0x0D0,
    GP_SW_SCRATCH = 0x0E0,
    PCI_ADDR_MASK = 0x0F0
};

}; // namespace Platinum

class PlatinumCtrl : public MemCtrlBase, public MMIODevice {
public:
    PlatinumCtrl();
    ~PlatinumCtrl() = default;

    bool supports_type(HWCompType type) {
        return (type == HWCompType::MEM_CTRL || type == HWCompType::MMIO_DEV);
    }

    /* MMIODevice methods */
    uint32_t read(uint32_t reg_start, uint32_t offset, int size);
    void write(uint32_t reg_start, uint32_t offset, uint32_t value, int size);

private:
    uint32_t    cpu_id;
    uint8_t     cpu_type; // 0 - MPC601, 1 - 603/604 CPU
};

#endif // PLATINUM_MEMCTRL_H