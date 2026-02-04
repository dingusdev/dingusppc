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

/** @file SCSI bus controller definitions. */

#ifndef SCSI_BUS_CTRL_H
#define SCSI_BUS_CTRL_H

#include <devices/common/dmacore.h>
#include <devices/common/hwinterrupt.h>
#include <devices/common/scsi/scsi.h>

#include <cinttypes>
#include <string>

namespace Scsi_Bus_Controller {
    /** SCSI sequencer states. */
    enum SeqState : uint32_t {
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
}

/** Sequencer error codes. */
enum : int {
    ARB_LOST    = 1,
    SEL_TIMEOUT,
};

constexpr auto DATA_FIFO_DEPTH = 16;

class ScsiBusController : public ScsiPhysDevice, public DmaDevice {
public:
    ScsiBusController(std::string name, uint8_t my_bus_id=7) : ScsiPhysDevice(name, my_bus_id) {
        supports_types(HWCompType::SCSI_HOST | HWCompType::SCSI_DEV);
    }
    ~ScsiBusController() = default;

    // implements SCSI FSM
    void sequencer();

    virtual void step_completed() = 0;
    virtual void report_error(const int error) = 0;

    // ScsiPhysDevice methods
    void notify(ScsiNotification notif_type, int param) override;
    bool prepare_data() override { return false; }
    bool get_more_data() override { return false; }
    bool has_data() override { return false; }
    bool rcv_data();
    int  send_data(uint8_t* dst_ptr, int count) override;
    void process_command() override {}

    // DmaDevice methods
    int xfer_from(uint8_t *buf, int len) override;

protected:
    void seq_defer_state(uint64_t delay_ns);
    void update_irq();
    void fifo_push(const uint8_t data);
    uint8_t fifo_pop();

    ScsiBus*    bus_obj = nullptr;
    uint8_t     src_id;
    uint8_t     dst_id;
    bool        is_dma_cmd   = false;
    bool        is_initiator = true;
    bool        assert_atn   = false;

    // Data FIFO state
    int         fifo_pos  = 0;
    int         to_xfer   = 0;
    uint32_t    xfer_count = 0;
    int         bytes_out = 0;
    uint8_t     data_fifo[DATA_FIFO_DEPTH] = {};

    // Sequencer state
    uint32_t    seq_timer_id = 0;
    uint32_t    cur_state  = Scsi_Bus_Controller::SeqState::IDLE;
    uint32_t    next_state = Scsi_Bus_Controller::SeqState::IDLE;
    int         cur_bus_phase = 0;

    // interrupt related stuff
    InterruptCtrl*  int_ctrl = nullptr;
    uint64_t        irq_id   = 0;
    uint8_t         irq      = 0;
    uint8_t         int_mask = 0;
    uint8_t         int_stat = 0;
};

#endif // SCSI_BUS_CTRL_H
