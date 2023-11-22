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

/** @file Enhanced Serial Communications Controller (ESCC) definitions. */

#ifndef ESCC_H
#define ESCC_H

#include <devices/common/hwcomponent.h>
#include <devices/serial/chario.h>

#include <cinttypes>
#include <memory>
#include <string>

class DmaBidirChannel;

/** ESCC register positions */
/* Please note that the registers below are provided
   by the Apple I/O controllers for accessing ESCC
   in a more convenient way. Actual physical addresses
   are controller dependent.
   The registers are ordered according with the MacRISC
   scheme used in the PCI Power Macintosh models.
   Pre-PCI Macs uses the so-called compatibility
   addressing. Please use compat_to_macrisc table
   below for converting from compat to MacRISC.
 */
enum EsccReg : uint8_t {
    Port_B_Cmd      = 0,
    Port_B_Data     = 1, // direct access to WR8/RR8
    Port_A_Cmd      = 2,
    Port_A_Data     = 3, // direct access to WR8/RR8
    Enh_Reg_B       = 4, // undocumented Apple extension
    Enh_Reg_A       = 5, // undocumented Apple extension
};

extern const uint8_t compat_to_macrisc[6];

/** LocalTalk LTPC registers provided by a MacIO controller. */
enum LocalTalkReg : uint8_t {
    Rec_Count   = 8,
    Start_A     = 9,
    Start_B     = 0xA,
    Detect_AB   = 0xB,
};

enum WR0Cmd : uint8_t {
    Point_High = 1,
};

/** ESCC reset commands. */
enum {
    RESET_ESCC = 0xC0,
    RESET_CH_A = 0x80,
    RESET_CH_B = 0x40
};

/** DPLL commands in WR14. */
enum {
    DPLL_NULL_CMD           = 0,
    DPLL_ENTER_SRC_MODE     = 1,
    DPLL_RST_MISSING_CLK    = 2,
    DPLL_DISABLE            = 3,
    DPLL_SET_SRC_BGR        = 4,
    DPLL_SET_SRC_RTXC       = 5,
    DPLL_SET_FM_MODE        = 6,
    DPLL_SET_NRZI_MODE      = 7
};

enum DpllMode : uint8_t {
    NRZI = 0,
    FM   = 1
};

/** ESCC Channel class. */
class EsccChannel {
public:
    EsccChannel(std::string name) { this->name = name; };
    ~EsccChannel() = default;

    void attach_backend(int id);
    void reset(bool hw_reset);
    uint8_t read_reg(int reg_num);
    void write_reg(int reg_num, uint8_t value);
    void send_byte(uint8_t value);
    uint8_t receive_byte();

private:
    std::string     name;
    uint8_t         read_regs[16];
    uint8_t         write_regs[16];
    uint8_t         wr7_enh;
    uint8_t         dpll_active;
    uint8_t         dpll_mode;
    uint8_t         dpll_clock_src;
    uint8_t         brg_active;
    uint8_t         brg_clock_src;

    std::unique_ptr<CharIoBackEnd>  chario;
};

/** ESCC Controller class. */
class EsccController : public HWComponent {
public:
    EsccController();
    ~EsccController() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<EsccController>(new EsccController());
    }

    // ESCC registers access
    uint8_t read(uint8_t reg_offset);
    void    write(uint8_t reg_offset, uint8_t value);

    void dma_start();
    void dma_stop();
    void set_dma_channel(int ch_index, DmaBidirChannel *dma_ch) {
        this->dma_ch[ch_index] = dma_ch;
    };

private:
    void reset();
    void write_internal(EsccChannel* ch, uint8_t value);

    std::unique_ptr<EsccChannel>    ch_a;
    std::unique_ptr<EsccChannel>    ch_b;

    int reg_ptr; // register pointer for reading/writing (same for both channels)

    uint8_t master_int_cntrl;
    uint8_t int_vec;

    DmaBidirChannel*    dma_ch[4];
};

#endif // ESCC_H
