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

/** @file NCR53C94/Am53CF94 SCSI controller definitions. */

/* NOTE: Power Macintosh computers don't have a real NCR 53C94 chip.
   The corresponding functionality is provided by the 53CF94 compatible
   cell in the custom Curio IC (Am79C950).
*/

#ifndef SC_53C94_H
#define SC_53C94_H

#include <devices/common/dmacore.h>
#include <devices/common/scsi/scsi.h>
#include <devices/common/hwinterrupt.h>

#include <cinttypes>
#include <memory>

#define DATA_FIFO_MAX   16

/** 53C94 read registers */
namespace Read {
    enum Reg53C94 : uint8_t {
        Xfer_Cnt_LSB = 0,
        Xfer_Cnt_MSB = 1,
        FIFO         = 2,
        Command      = 3,
        Status       = 4,
        Int_Status   = 5,
        Seq_Step     = 6,
        FIFO_Flags   = 7,
        Config_1     = 8,
        Config_2     = 0xB,
        Config_3     = 0xC,
        Config_4     = 0xD, // Am53CF94 extension
        Xfer_Cnt_Hi  = 0xE, // Am53CF94 extension
    };
};

/** 53C94 write registers */
namespace Write {
    enum Reg53C94 : uint8_t {
        Xfer_Cnt_LSB = 0,
        Xfer_Cnt_MSB = 1,
        FIFO         = 2,
        Command      = 3,
        Dest_Bus_ID  = 4,
        Sel_Timeout  = 5,
        Sync_Period  = 6,
        Sync_Offset  = 7,
        Config_1     = 8,
        Clock_Factor = 9,
        Test_Mode    = 0xA,
        Config_2     = 0xB,
        Config_3     = 0xC,
        Config_4     = 0xD, // Am53CF94 extension
        Xfer_Cnt_Hi  = 0xE, // Am53CF94 extension
        Data_Align   = 0xF
    };
};

/** NCR53C94/Am53CF94 commands. */
enum {
    CMD_NOP             =    0,
    CMD_CLEAR_FIFO      =    1,
    CMD_RESET_DEVICE    =    2,
    CMD_RESET_BUS       =    3,
    CMD_DMA_STOP        =    4,
    CMD_XFER            = 0x10,
    CMD_COMPLETE_STEPS  = 0x11,
    CMD_MSG_ACCEPTED    = 0x12,
    CMD_SELECT_NO_ATN   = 0x41,
    CMD_SELECT_WITH_ATN = 0x42,
    CMD_ENA_SEL_RESEL   = 0x44,
};

/** Status register bits. **/
enum {
    STAT_TC         = 1 << 4, // Terminal count (NCR) / count to zero (AMD)
    STAT_GE         = 1 << 6, // Gross Error (NCR) / Illegal Operation Error (AMD)
};

/** Interrupt status register bits. */
enum {
    INTSTAT_SRST    = 0x80, // bus reset
    INTSTAT_ICMD    = 0x40, // invalid command
    INTSTAT_DIS     = 0x20, // disconnected
    INTSTAT_SR      = 0x10, // service request
    INTSTAT_SO      = 0x08, // successful operation
    INTSTAT_RESEL   = 0x04, // reselected
    INTSTAT_SELA    = 0x02, // selected as a target with attention
    INTSTAT_SEL     = 0x01, // selected as a target without attention
};

enum {
    CFG2_ENF        = 0x40, // Am53CF94: enable features (ENF) bit
};

/** Sequencer states. */
namespace SeqState {
    enum {
        IDLE = 0,
        BUS_FREE,
        ARB_BEGIN,
        ARB_END,
        SEL_BEGIN,
        SEL_END,
        SEND_MSG,
        SEND_CMD,
        CMD_COMPLETE,
        XFER_BEGIN,
        XFER_END,
        SEND_DATA,
        RCV_DATA,
        RCV_STATUS,
        RCV_MESSAGE,
    };
};

/** Sequence descriptor for sequencer commands. */
typedef struct {
    int next_step;
    int step_num;
    int status;
} SeqDesc;

class Sc53C94 : public ScsiDevice {
public:
    Sc53C94(uint8_t chip_id=12, uint8_t my_id=7);
    ~Sc53C94() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<Sc53C94>(new Sc53C94());
    }

    // HWComponent methods
    int device_postinit();

    // 53C94 registers access
    uint8_t  read(uint8_t reg_offset);
    void     write(uint8_t reg_offset, uint8_t value);
    uint16_t pseudo_dma_read();

    // real DMA control
    void     real_dma_xfer(int direction);

    void set_dma_channel(DmaBidirChannel *dma_ch) {
        this->dma_ch = dma_ch;
    };

    // ScsiDevice methods
    void notify(ScsiBus* bus_obj, ScsiMsg msg_type, int param);
    bool prepare_data() { return false; };
    bool has_data() { return this->data_fifo_pos != 0; };
    int  send_data(uint8_t* dst_ptr, int count);
    void process_command() {};

protected:
    void reset_device();
    void update_command_reg(uint8_t cmd);
    void exec_command();
    void exec_next_command();
    void fifo_push(const uint8_t data);
    uint8_t fifo_pop();

    void sequencer();
    void seq_defer_state(uint64_t delay_ns);

    bool rcv_data();

    void update_irq();

private:
    uint8_t     chip_id;
    uint8_t     my_bus_id;
    ScsiBus*    bus_obj;
    uint32_t    my_timer_id;

    uint8_t     cmd_fifo[2];
    uint8_t     data_fifo[16];
    int         cmd_fifo_pos;
    int         data_fifo_pos;
    int         bytes_out;
    bool        on_reset = false;
    uint32_t    xfer_count;
    uint32_t    set_xfer_count;
    uint8_t     status;
    uint8_t     target_id;
    uint8_t     int_status;
    uint8_t     seq_step;
    uint8_t     sel_timeout;
    uint8_t     sync_offset;
    uint8_t     clk_factor;
    uint8_t     config1;
    uint8_t     config2;
    uint8_t     config3;

    // sequencer state
    uint32_t    seq_timer_id;
    uint32_t    cur_state;
    uint32_t    next_state;
    SeqDesc*    cmd_steps;
    bool        is_initiator;
    uint8_t     cur_cmd;
    int         cur_bus_phase;

    // interrupt related stuff
    InterruptCtrl* int_ctrl = nullptr;
    uint32_t       irq_id   = 0;
    uint8_t        irq      = 0;

    // DMA related stuff
    DmaBidirChannel*    dma_ch;
};

#endif // SC_53C94_H
