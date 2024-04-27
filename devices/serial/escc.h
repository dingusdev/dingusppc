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
#include <devices/common/dbdma.h>
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
    uint8_t get_enh_reg();
    void set_enh_reg(uint8_t value);

    void set_dma_channel(int dir_index, DmaBidirChannel *dma_ch) {
        this->dma_ch[dir_index] = dma_ch;
        auto dbdma_ch = dynamic_cast<DMAChannel*>(dma_ch);
        if (dbdma_ch) {
            switch (dir_index) {
            case 0:
                dbdma_ch->set_callbacks(
                    std::bind(&EsccChannel::dma_start_tx, this),
                    std::bind(&EsccChannel::dma_stop_tx, this)
                );
                dbdma_ch->set_data_callbacks(
                    std::bind(&EsccChannel::dma_in_tx, this),
                    std::bind(&EsccChannel::dma_out_tx, this),
                    std::bind(&EsccChannel::dma_flush_tx, this)
                );
                break;
            case 1:
                dbdma_ch->set_callbacks(
                    std::bind(&EsccChannel::dma_start_rx, this),
                    std::bind(&EsccChannel::dma_stop_rx, this)
                );
                dbdma_ch->set_data_callbacks(
                    std::bind(&EsccChannel::dma_in_rx, this),
                    std::bind(&EsccChannel::dma_out_rx, this),
                    std::bind(&EsccChannel::dma_flush_rx, this)
                );
                break;
            }
        }
    };

private:
    uint32_t timer_id_tx = 0;
    uint32_t timer_id_rx = 0;

    void dma_start_tx();
    void dma_stop_tx();
    void dma_in_tx();
    void dma_out_tx();
    void dma_flush_tx();

    void dma_start_rx();
    void dma_stop_rx();
    void dma_in_rx();
    void dma_out_rx();
    void dma_flush_rx();

    DmaBidirChannel*    dma_ch[2];

    std::string     name;
    uint8_t         read_regs[16] = {};
    uint8_t         write_regs[16] = {};
    uint8_t         wr7_enh = 0;
    uint8_t         dpll_active;
    DpllMode        dpll_mode;
    uint8_t         dpll_clock_src;
    uint8_t         brg_active;
    uint8_t         brg_clock_src;
    uint8_t         enh_reg = 0;

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

    void set_dma_channel(int ch_dir_index, DmaBidirChannel *dma_ch) {
        switch (ch_dir_index >> 1) {
        case 0: ch_a->set_dma_channel(ch_dir_index & 1, dma_ch); break;
        case 1: ch_b->set_dma_channel(ch_dir_index & 1, dma_ch); break;
        }
    };

private:
    void reset();
    void write_internal(EsccChannel* ch, uint8_t value);
    uint8_t read_internal(EsccChannel* ch);

    std::unique_ptr<EsccChannel>    ch_a;
    std::unique_ptr<EsccChannel>    ch_b;

    int reg_ptr; // register pointer for reading/writing (same for both channels)

    uint8_t master_int_cntrl = 0;
    uint8_t int_vec = 0;
};

#endif // ESCC_H
