/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

(You may also contact divingkxt or powermax2286 on Discord)

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
#include <devices/common/dmacore.h>
#include <devices/serial/chario.h>

#include <cinttypes>
#include <memory>
#include <string>

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
    Enh_Reg_B       = 4, // extended Curio/Geoport features
    Enh_Reg_A       = 5, // extended Curio/Geoport features
};

extern const uint8_t compat_to_macrisc[6];

/** LocalTalk LTPC registers implemented in a MacIO controller. */
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

enum DirIndex : uint8_t {
    DIR_TX,
    DIR_RX,
    DIR_MIN = DIR_TX,
    DIR_MAX = DIR_RX,
};

enum ChIndex : uint8_t {
    CH_A,
    CH_B
};

/** ESCC Channel class. */
class EsccChannel : public DmaDevice {
public:
    EsccChannel(std::string name) { this->name = name; }
    ~EsccChannel() = default;

    void attach_backend(int id);
    void reset(bool hw_reset);
    uint8_t read_reg(int reg_num);
    void write_reg(int reg_num, uint8_t value);
    void send_byte(uint8_t value);
    uint8_t receive_byte();
    uint8_t get_enh_reg();
    void set_enh_reg(uint8_t value);

    void set_dma_channel(DirIndex dir_index, DmaChannel *ch_obj) {
        this->dma_channels[dir_index] = ch_obj;

        ch_obj->set_id(dir_index);
        ch_obj->set_type(dir_index == DIR_TX ? DMA_CH_TYPE_OUT : DMA_CH_TYPE_IN);
        ch_obj->connect(this);
    }

    // End of packet (EOP) flag is reflected by s5 bit of the DMA TX channel status
    void update_ltpc_eop_flag(uint8_t flag) {
        auto dbdma_ch = static_cast<DMAChannel*>(this->dma_channels[DIR_TX]);
        dbdma_ch->update_ch_stat(1 << 5, (flag & 1) << 5);
    }

    // DmaChannel methods
    int xfer_from(DmaChannel *ch_obj, uint8_t *buf, int len) override;
    int xfer_to  (DmaChannel *ch_obj, uint8_t *buf, int len) override;

private:
    DmaChannel*     dma_channels[2];

    std::string     name;
    uint8_t         read_regs[16] = {};
    uint8_t         write_regs[17] = {};
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

    void connect_dma_channel(ChIndex ch_idx, DirIndex dir_idx, DmaChannel *ch_obj) {
        switch (ch_idx) {
        case CH_A: ch_a->set_dma_channel(dir_idx, ch_obj); break;
        case CH_B: ch_b->set_dma_channel(dir_idx, ch_obj); break;
        }
    }

private:
    void reset();
    void write_internal(EsccChannel* ch, uint8_t value);
    uint8_t read_internal(EsccChannel* ch);

    std::unique_ptr<EsccChannel>    ch_a;
    std::unique_ptr<EsccChannel>    ch_b;

    int reg_ptr; // register pointer for reading/writing (same for both channels)

    uint8_t master_int_cntrl = 0;
    uint8_t int_vec = 0;
    uint8_t recovery_counter = 8;

    // LTPC state
    uint8_t start_a   = 0;
    uint8_t start_b   = 0;
    uint8_t detect_ab = 0;
};

#endif // ESCC_H
