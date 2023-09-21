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
    XIFC        = 0x000, // transceiver interface control
    TX_FIFO_CSR = 0x100,
    TX_FIFO_TH  = 0x110,
    RX_FIFO_CSR = 0x120,
    CHIP_ID     = 0x170,
    MIF_CSR     = 0x180,
    GLOB_STAT   = 0x200, // Apple: kSTAT, Sun: Global Status Register
    EVENT_MASK  = 0x210, // ambiguously called INT_DISABLE in the Apple source
    SROM_CSR    = 0x190,
    TX_SW_RST   = 0x420,
    TX_CONFIG   = 0x430,
    PEAK_ATT    = 0x4E0, // Apple: kPAREG, Sun: PeakAttempts Register
    NC_CNT      = 0x500, // Normal Collision Counter
    NT_CNT      = 0x510, // Apple: Network Collision Counter
    EX_CNT      = 0x520, // Excessive Collision Counter
    LT_CNT      = 0x530, // Late Collision Counter
    RNG_SEED    = 0x540,
    RX_SW_RST   = 0x620,
    RX_CONFIG   = 0x630,
    MAC_ADDR_2  = 0x660,
    MAC_ADDR_1  = 0x670,
    MAC_ADDR_0  = 0x680,
    RX_FRM_CNT  = 0x690, // Receive Frame Counter
    RX_LE_CNT   = 0x6A0, // Length Error Counter
    RX_AE_CNT   = 0x6B0, // Alignment Error Counter
    RX_FE_CNT   = 0x6C0, // FCS Error Counter
    RX_CVE_CNT  = 0x6E0, // Code Violation Error Counter
    HASH_TAB_3  = 0x700,
    HASH_TAB_2  = 0x710,
    HASH_TAB_1  = 0x720,
    HASH_TAB_0  = 0x730,
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
    PHY_BMSR    = 1,
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

protected:
    void chip_reset();

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
    uint16_t        tx_reset        = 0; // self-clearing one-bit register
    uint16_t        tx_if_ctrl      = 0;
    uint16_t        rng_seed;
    uint16_t        norm_coll_cnt   = 0;
    uint16_t        net_coll_cnt    = 0;
    uint16_t        excs_coll_cnt   = 0;
    uint16_t        late_coll_cnt   = 0;
    uint16_t        rcv_frame_cnt   = 0;
    uint8_t         len_err_cnt     = 0;
    uint8_t         align_err_cnt   = 0;
    uint8_t         fcs_err_cnt     = 0;
    uint8_t         cv_err_cnt      = 0;
    uint8_t         peak_attempts   = 0;
    uint8_t         tx_fifo_tresh   = 0;
    bool            tx_fifo_enable  = false;
    bool            rx_fifo_enable  = false;
    uint16_t        tx_fifo_size    = 0;
    uint16_t        rx_fifo_size    = 0;
    uint16_t        hash_table[4]   = {};
    uint16_t        mac_addr_flt[3] = {};
    uint16_t        tx_config       = 0;
    uint16_t        rx_config       = 0;

    // Interrupt state
    uint16_t        event_mask = 0xFFFFU; // inverted mask: 0 - enabled, 1 - disabled
    uint16_t        stat = 0;

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
