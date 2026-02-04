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

/** @file NCR53C94/Am53CF94 SCSI controller definitions. */

/* NOTE: Power Macintosh computers don't have a real NCR 53C94 chip.
   The corresponding functionality is provided by the 53CF94 compatible
   cell in the custom Curio IC (Am79C950).
*/

#ifndef SC_53C94_H
#define SC_53C94_H

#include <devices/common/scsi/scsi.h>
#include <devices/common/dbdma.h>

#include <cinttypes>
#include <functional>
#include <memory>

class InterruptCtrl;

/** 53C94 read registers */
namespace Read {
    enum Reg53C94 : uint8_t {
        Xfer_Cnt_LSB = 0,   // Current Transfer Count Register LSB
        Xfer_Cnt_MSB = 1,   // Current Transfer Count Register MSB
        FIFO         = 2,   // FIFO Register
        Command      = 3,   // Command Register
        Status       = 4,   // Status Register
        Int_Status   = 5,   // Interrupt Status Register
        Seq_Step     = 6,   // Internal State Register
        FIFO_Flags   = 7,   // Current FIFO/Internal State Register
        Config_1     = 8,   // Control Register 1
                            //
                            //
        Config_2     = 0xB, // Control Register 2
        Config_3     = 0xC, // Control Register 3
        Config_4     = 0xD, // Control Register 4
        Xfer_Cnt_Hi  = 0xE, // Current Transfer Count Register High ; Am53CF94 extension
                            //
    };
}

/** 53C94 write registers */
namespace Write {
    enum Reg53C94 : uint8_t {
        Xfer_Cnt_LSB = 0,   // Start Transfer Count Register LSB
        Xfer_Cnt_MSB = 1,   // Start Transfer Count Register MSB
        FIFO         = 2,   // FIFO Register
        Command      = 3,   // Command Register
        Dest_Bus_ID  = 4,   // SCSI Destination ID Register (DID)
        Sel_Timeout  = 5,   // SCSI Timeout Register
        Sync_Period  = 6,   // Synchronous Transfer Period Register
        Sync_Offset  = 7,   // Synchronous Offset Register
        Config_1     = 8,   // Control Register 1
        Clock_Factor = 9,   // Clock Factor Register
        Test_Mode    = 0xA, // Forced Test Mode Register
        Config_2     = 0xB, // Control Register 2
        Config_3     = 0xC, // Control Register 3
        Config_4     = 0xD, // Control Register 4                 ; Am53CF94 extension
        Xfer_Cnt_Hi  = 0xE, // Start Transfer Count Register High ; Am53CF94 extension
        Data_Align   = 0xF, // Data Alignment Register
    };
}

/** NCR53C94/Am53CF94 commands. */
enum {
    // General Commands
    CMD_NOP                             = 0x00, // no interrupt
    CMD_CLEAR_FIFO                      = 0x01, // no interrupt
    CMD_RESET_DEVICE                    = 0x02, // no interrupt
    CMD_RESET_BUS                       = 0x03,

    // Initiator commands
    CMD_XFER                            = 0x10,
    CMD_COMPLETE_STEPS                  = 0x11,
    CMD_MSG_ACCEPTED                    = 0x12,
    CMD_XFER_PAD_BYTES                  = 0x18,
    CMD_SET_ATN                         = 0x1A, // no interrupt
    CMD_RESET_ATN                       = 0x1B, // no interrupt

    // Target commands
  //CMD_SEND_MESSAGE                    = 0x20,
  //CMD_SEND_STATUS                     = 0x21,
  //CMD_SEND_DATA                       = 0x22,
  //CMD_DISCONNECT_STEPS                = 0x23,
  //CMD_TERMINATE_STEPS                 = 0x24,
  //CMD_TARGET_COMMAND_COMPLETE_STEPS   = 0x25,
  //CMD_DISCONNECT                      = 0x27, // no interrupt
  //CMD_RECEIVE_MESSAGE_STEPS           = 0x28,
  //CMD_RECEIVE_COMMAND                 = 0x29,
  //CMD_RECEIVE_DATA                    = 0x2A,
  //CMD_RECEIVE_COMMAND_STEPS           = 0x2B,
    CMD_DMA_STOP                        = 0x04, // no interrupt
  //CMD_ACCESS_FIFO_COMMAND             = 0x05,

    // Idle Commands
  //CMD_RESELECT_STEPS                  = 0x40,
    CMD_SELECT_NO_ATN                   = 0x41,
    CMD_SELECT_WITH_ATN                 = 0x42,
    CMD_SELECT_WITH_ATN_AND_STOP        = 0x43,
    CMD_ENA_SEL_RESEL                   = 0x44, // no interrupt
  //CMD_DISABLE_SEL_RESEL               = 0x45,
  //CMD_SELECT_WITH_ATN3_STEPS          = 0x46,
  //CMD_RESELECT_WITH_ATN3_STEPS        = 0x47,

    // Flags
    CMD_OPCODE                          = 0x7F,
    CMD_ISDMA                           = 0x80,
};

/** Status register bits. */
enum {
    STAT_PHASE_MASK = 0x07, // mask for I/O, Command and Message bits
    STAT_GCV        = 0x08, // Group Code Valid
    STAT_TC         = 0x10, // Terminal count (NCR) / count to zero (AMD)
    STAT_PE         = 0x20, // Parity Error
    STAT_GE         = 0x40, // Gross Error (NCR) / Illegal Operation Error (AMD)
    STAT_INT        = 0x80, // Interrupt
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

/** Configuration register 1 bits. */
enum {
    CFG1_DISR       = 0x40, // disable interrupt on SCSI reset
};

/** Configuration register 2 bits. */
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
        SEND_MSG_EX,
        SEND_CMD,
        CMD_COMPLETE,
        XFER_BEGIN,
        XFER_END,
        SEND_DATA,
        RCV_DATA,
        RCV_STATUS,
        RCV_MESSAGE,
    };
}

constexpr auto DATA_FIFO_MAX = 16;

/** Sequence descriptor for multistep commands. */
typedef struct {
    int step_num;
    int expected_phase;
    int next_state;
    int status;
} SeqDesc;

typedef std::function<void(const uint8_t drq_state)> DrqCb;

class Sc53C94 : public ScsiPhysDevice, public DmaDevice {
public:
    Sc53C94(uint8_t chip_id=12, uint8_t my_id=7);
    ~Sc53C94() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<Sc53C94>(new Sc53C94());
    }

    // HWComponent methods
    int device_postinit() override;

    // 53C94 registers access
    uint8_t  read(uint8_t reg_offset);
    void     write(uint8_t reg_offset, uint8_t value);
    uint16_t pseudo_dma_read();
    void     pseudo_dma_write(uint16_t data);

    // real DMA control
    void real_dma_xfer_out();
    void real_dma_xfer_in();

    void dma_start();
    void dma_wait();
    void dma_stop();
    void set_dma_channel(DmaBidirChannel *dma_ch) {
        this->dma_ch = dma_ch;
        auto dbdma_ch = dynamic_cast<DMAChannel*>(dma_ch);
        if (dbdma_ch) {
            dbdma_ch->set_callbacks(
                std::bind(&Sc53C94::dma_start, this),
                std::bind(&Sc53C94::dma_stop, this)
            );
        }
    }

    void set_drq_callback(DrqCb cb) {
        this->drq_cb = cb;
    }

    // ScsiPhysDevice methods
    void notify(ScsiNotification notif_type, int param) override;
    bool prepare_data() override { return false; }
    bool get_more_data() override { return false; }
    bool has_data() override { return this->data_fifo_pos != 0; }
    int  send_data(uint8_t* dst_ptr, int count) override;
    void process_command() override {}

    // DmaDevice methods
    int xfer_from(uint8_t *buf, int len) override;
    int xfer_to(uint8_t *buf, int len) override;
    int tell_xfer_size() override {
        return this->xfer_count;
    }

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
    uint8_t     chip_id = 0;
    uint8_t     my_bus_id = 0;
    uint32_t    my_timer_id = 0;

    uint8_t     cmd_fifo[2];
    uint8_t     data_fifo[DATA_FIFO_MAX];
    int         cmd_fifo_pos = 0;
    int         data_fifo_pos = 0;
    int         bytes_out = 0;
    bool        on_reset = false;
    uint32_t    xfer_count = 0;
    uint32_t    set_xfer_count = 0;
    uint8_t     status = 0;
    uint8_t     target_id = 0;
    uint8_t     int_status = 0;
    uint8_t     seq_step = 0;
    uint8_t     sel_timeout = 0;
    uint8_t     sync_period = 5;
    uint8_t     sync_offset = 0;
    uint8_t     clk_factor = 0;
    uint8_t     config1 = 0;
    uint8_t     config2 = 0;
    uint8_t     config3 = 0;

    // sequencer state
    uint32_t    seq_timer_id = 0;
    uint32_t    cur_state = 0;
    uint32_t    next_state = 0;
    SeqDesc*    cmd_steps = nullptr;
    bool        is_initiator = false;
    uint8_t     cur_cmd = 0;
    bool        is_dma_cmd = false;
    int         cur_bus_phase = 0;
    uint8_t     cur_step = 0; // internal step counter for multistep commands

    // interrupt related stuff
    InterruptCtrl* int_ctrl = nullptr;
    uint64_t       irq_id   = 0;
    uint8_t        irq      = 0;

    // DMA related stuff
    DmaBidirChannel*    dma_ch = nullptr;
    DrqCb               drq_cb = nullptr;
    uint32_t            dma_timer_id = 0;
};

#endif // SC_53C94_H
