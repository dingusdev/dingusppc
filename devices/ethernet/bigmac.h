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

/** @file BigMac Ethernet controller definitions. */

#ifndef BIG_MAC_H
#define BIG_MAC_H

#include <devices/common/hwcomponent.h>

#include <cinttypes>
#include <memory>

/* Ethernet cell IDs for various MacIO ASICs. */
enum EthernetCellId : uint8_t {
    Heathrow    = 0xB1,
    Paddington  = 0xC7,
};

/* BigMac HW registers. */
enum BigMacReg : uint16_t {
    CHIP_ID     = 0x170,
    MIF_CSR     = 0x180,
    SROM_CSR    = 0x190,
    TX_SW_RST   = 0x420,
    RX_SW_RST   = 0x620,
};

/* MIF_CSR bit definitions. */
enum {
    Mif_Clock       = 1 << 0,
    Mif_Data_Out    = 1 << 1,
    Mif_Data_Out_En = 1 << 2,
    Mif_Data_In     = 1 << 3
};

/* SROM_CSR bit definitions. */
enum {
    Srom_Chip_Select = 1 << 0,
    Srom_Clock       = 1 << 1,
    Srom_Data_In     = 1 << 2,
    Srom_Data_Out    = 1 << 3,
};

/* Serial EEPROM states (see ST93C46 datasheet). */
enum {
    Srom_Start,
    Srom_Opcode,
    Srom_Address,
    Srom_Read_Data,
};

/* MII frame states. */
enum MII_FRAME_SM {
    Preamble,
    Start,
    Opcode,
    Phy_Address,
    Reg_Address,
    Turnaround,
    Read_Data,
    Write_Data,
    Stop
};

/* PHY control/status registers. */
enum {
    PHY_BMCR    = 0,
    PHY_ID1     = 2,
    PHY_ID2     = 3,
    PHY_ANAR    = 4,
};

class BigMac : public HWComponent {
public:
    BigMac(uint8_t id);
    ~BigMac() = default;

    static std::unique_ptr<HWComponent> create_for_heathrow() {
        return std::unique_ptr<BigMac>(new BigMac(EthernetCellId::Heathrow));
    }

    static std::unique_ptr<HWComponent> create_for_paddington() {
        return std::unique_ptr<BigMac>(new BigMac(EthernetCellId::Paddington));
    }

    // BigMac register accessors
    uint16_t read(uint16_t reg_offset);
    void     write(uint16_t reg_offset, uint16_t value);

    // MII methods
    bool mii_rcv_value(uint16_t& var, uint8_t num_bits, uint8_t next_bit);
    void mii_rcv_bit();
    void mii_xmit_bit(const uint8_t bit_val);
    void mii_reset();

    // PHY control/status methods
    void phy_reset();
    uint16_t phy_reg_read(uint8_t reg_num);
    void phy_reg_write(uint8_t reg_num, uint16_t value);

    // MAC Serial EEPROM methods
    void srom_reset();
    bool srom_rcv_value(uint16_t& var, uint8_t num_bits, uint8_t next_bit);
    void srom_xmit_bit(const uint8_t bit_val);

private:
    uint8_t chip_id; // BigMac Chip ID

    // BigMac state
    uint16_t        tx_reset = 0;

    // MII state
    uint8_t         mif_csr_old = 0;
    uint8_t         mii_bit_counter = 0;
    uint16_t        mii_start = 0;
    uint16_t        mii_stop = 0;
    uint16_t        mii_opcode = 0;
    uint16_t        mii_phy_address = 0;
    uint16_t        mii_reg_address = 0;
    uint16_t        mii_turnaround = 0;
    uint16_t        mii_data = 0;
    uint8_t         mii_in_bit = 1;
    MII_FRAME_SM    mii_state = MII_FRAME_SM::Preamble;

    // PHY state
    uint32_t        phy_oui;
    uint8_t         phy_model;
    uint8_t         phy_rev;
    uint16_t        phy_bmcr;
    uint16_t        phy_anar;

    // MAC SROM state
    uint8_t         srom_csr_old = 0;
    uint8_t         srom_bit_counter = 0;
    uint16_t        srom_opcode = 0;
    uint16_t        srom_address = 0;
    uint8_t         srom_in_bit = 0;
    uint8_t         srom_state = Srom_Start;
    uint16_t        srom_data[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                     0xDEAD, 0xBEEF, 0xBABE}; // bogus MAC!!!
};

#endif // BIG_MAC_H
