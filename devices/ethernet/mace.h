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

/** @file Media Access Controller for Ethernet (MACE) definitions. */

#ifndef MACE_H
#define MACE_H

#include <devices/common/dmacore.h>
#include <devices/common/hwcomponent.h>

#include <cinttypes>
#include <memory>

/** Known MACE chip IDs. */
#define MACE_ID_REV_B0 0x0940 // Darwin-0.3 source
#define MACE_ID_REV_A2 0x0941 // Darwin-0.3 source & Curio datasheet

/** MACE registers offsets. */
// Refer to the Am79C940 datasheet for details
namespace MaceEnet {
    enum MaceReg : uint8_t {
        Rcv_FIFO        =  0,
        Xmit_FIFO       =  1,
        Xmit_Frame_Ctrl =  2,
        Xmit_Frame_Stat =  3,
        Xmit_Retry_Cnt  =  4,
        Rcv_Frame_Ctrl  =  5,
        Rcv_Frame_Stat  =  6,
        FIFO_Frame_Cnt  =  7,
        Interrupt       =  8,
        Interrupt_Mask  =  9,
        Poll            = 10,
        BIU_Config_Ctrl = 11,
        FIFO_Config     = 12,
        MAC_Config_Ctrl = 13,
        PLS_Config_Ctrl = 14,
        PHY_Config_Ctrl = 15,
        Chip_ID_Lo      = 16,
        Chip_ID_Hi      = 17,
        Int_Addr_Config = 18,
        Log_Addr_Flt    = 20,
        Phys_Addr       = 21,
        Missed_Pkt_Cnt  = 24,
        Runt_Pkt_Cnt    = 26, // not used in Macintosh?
        Rcv_Collis_Cnt  = 27, // not used in Macintosh?
        User_Test       = 29,
        Rsrvd_Test_1    = 30, // not used in Macintosh?
        Rsrvd_Test_2    = 31, // not used in Macintosh?
    };

    /** Bit definitions for BIU_Config_Ctrl register. */
    enum {
        BIU_SWRST   = 1 << 0,
    };

    /** Bit definitions for the internal configuration register. */
    enum {
        IAC_LOGADDR = 1 << 1,
        IAC_PHYADDR = 1 << 2,
        IAC_ADDRCHG = 1 << 7
    };

}; // namespace MaceEnet

class MaceController : public DmaDevice, public HWComponent {
public:
    MaceController(uint16_t id) {
        this->chip_id = id;
        this->set_name("MACE");
        this->supports_types(HWCompType::MMIO_DEV | HWCompType::ETHER_MAC);
    };
    ~MaceController() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<MaceController>(new MaceController(MACE_ID_REV_A2));
    }

    // MACE registers access
    uint8_t read(uint8_t reg_offset);
    void    write(uint8_t reg_offset, uint8_t value);

private:
    uint16_t    chip_id; // per-instance MACE Chip ID
    uint8_t     addr_cfg  = 0;
    uint8_t     addr_ptr  = 0;
    uint8_t     rcv_fc    = 1;
    uint8_t     biu_ctrl  = 0;
    uint8_t     mac_cfg   = 0;
    uint64_t    phys_addr = 0;
    uint64_t    log_addr  = 0;

    // interrupt stuff
    uint8_t     int_stat  = 0;
    uint8_t     int_mask  = 0;
};

#endif // MACE_H
