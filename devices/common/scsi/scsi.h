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

/** @file SCSI bus definitions. */

#ifndef SCSI_H
#define SCSI_H

#include <devices/common/hwcomponent.h>
#include <devices/common/scsi/scsiphysinterface.h>

#include <array>
#include <cinttypes>
#include <functional>
#include <memory>
#include <string>

/** SCSI control signals.
    Bit positions follow the MESH controller convention for easier mapping.
 */
enum {
    SCSI_CTRL_IO  = 1 << 0,
    SCSI_CTRL_CD  = 1 << 1,
    SCSI_CTRL_MSG = 1 << 2,
    SCSI_CTRL_ATN = 1 << 3,
    SCSI_CTRL_ACK = 1 << 4,
    SCSI_CTRL_REQ = 1 << 5,
    SCSI_CTRL_SEL = 1 << 13,
    SCSI_CTRL_BSY = 1 << 14,
    SCSI_CTRL_RST = 1 << 15,
};

/** SCSI bus phases. */
namespace ScsiPhase {
    enum : int {
        BUS_FREE = 0,
        ARBITRATION,
        SELECTION,
        RESELECTION,
        COMMAND,
        DATA_IN,
        DATA_OUT,
        STATUS,
        MESSAGE_IN,
        MESSAGE_OUT,
        RESET,
    };
}

/** SCSI status codes. */
namespace ScsiStatus {
    enum : uint8_t {
        GOOD = 0,
        CHECK_CONDITION = 2,
    };
}

/** Standard message codes. */
namespace ScsiMessage {
    enum : uint8_t {
        COMMAND_COMPLETE = 0,

        TARGET_ROUTINE_NUMBER_MASK = 0x07,
        LOGICAL_UNIT_NUMBER_MASK   = 0x07,
        IS_TARGET_ROUTINE_NuMBER   = 0x20,
        HAS_DISCONNECT_PRIVILEDGE  = 0x40,
        IDENTIFY                   = 0x80,
    };
}

/** Extended message codes. */
namespace ScsiExtMessage {
    enum : uint8_t {
        MODIFY_DATA_PTR = 0,
        SYNCH_XFER_REQ  = 1,
        WIDE_XFER_REQ   = 3,
    };
}

/** Internal notification codes for our SCSI implementation. */
enum ScsiNotification : int {
    CONFIRM_SEL = 1,
    BUS_PHASE_CHANGE,
};

enum ScsiCommand : uint8_t {
    TEST_UNIT_READY              = 0x00,
    REWIND                       = 0x01,
    REQ_SENSE                    = 0x03,
    FORMAT_UNIT                  = 0x04,
    READ_BLK_LIMITS              = 0x05,
    REASSIGN                     = 0x07,
    READ_6                       = 0x08,
    WRITE_6                      = 0x0A,
    SEEK_6                       = 0x0B,
    INQUIRY                      = 0x12,
    VERIFY_6                     = 0x13,
    MODE_SELECT_6                = 0x15,
    RELEASE_UNIT                 = 0x17,
    ERASE_6                      = 0x19,
    MODE_SENSE_6                 = 0x1A,
    START_STOP_UNIT              = 0x1B,
    DIAG_RESULTS                 = 0x1C,
    SEND_DIAGS                   = 0x1D,
    PREVENT_ALLOW_MEDIUM_REMOVAL = 0x1E,
    READ_CAPACITY_10             = 0x25,
    READ_10                      = 0x28,
    WRITE_10                     = 0x2A,
    SEEK_10                      = 0x2B,
    VERIFY_WRITE_10              = 0x2E,
    VERIFY_10                    = 0x2F,
    READ_LONG_10                 = 0x35,
    WRITE_BUFFER                 = 0x3B,
    READ_BUFFER                  = 0x3C,
    GET_CONFIG                   = 0x46,
    GET_EVENT_STATUS_NOTIFY      = 0x4A,
    READ_DISC_INFO               = 0x51,
    MODE_SELECT_10               = 0x55,
    MODE_SENSE_10                = 0x5A,
    READ_12                      = 0xA8,
    WRITE_12                     = 0xAA,
    READ_DVD_STRUCTURE           = 0xAD,
    SET_SPEED                    = 0xBB,

    // CD-ROM specific commands
    READ_TOC                     = 0x43,
    SET_CD_SPEED                 = 0xBB,
    READ_CD                      = 0xBE,
};

enum ScsiSense : uint8_t {
    NO_SENSE       = 0x0,
    RECOVERED      = 0x1,
    NOT_READY      = 0x2,
    MEDIUM_ERR     = 0x3,
    HW_ERROR       = 0x4,
    ILLEGAL_REQ    = 0x5,
    UNIT_ATTENTION = 0x6,
    DATA_PROTECT   = 0x7,
    BLANK_CHECK    = 0x8,
    VOL_OVERFLOW   = 0xD,
    MISCOMPARE     = 0xE,
    COMPLETED      = 0xF
};

enum ScsiError : uint8_t {
    NO_ERROR                = 0x00,
    NO_SECTOR               = 0x01,
    WRITE_FAULT             = 0x03,
    DEV_NOT_READY           = 0x04,
    INVALID_CMD             = 0x20,
    INVALID_LBA             = 0x21,
    INVALID_CDB             = 0x24,
    INVALID_LUN             = 0x25,
    WRITE_PROTECT           = 0x27,
    SAVING_NOT_SUPPORTED    = 0x39,
    MEDIUM_NOT_PRESENT      = 0x3A,
};

/** SCSI device types used in INQUIRY. */
namespace ScsiDevType {
    enum : uint8_t {
        DIRECT_ACCESS   = 0,
        SEQ_ACCESS      = 1,
        PRINTER         = 2,
        PROCESSOR       = 3,
        WRITE_ONCE      = 4,
        CD_ROM          = 5,
        SCANNER         = 6,
        OPT_MEMORY      = 7,
        MEDIUM_CHANGER  = 8,
        COMMUNICATION   = 9,
        UNKNOWN         = 0x1F
    };
};

/** SCSI device capacity flags. */
enum : uint8_t {
    CAP_REL_ADDR    = 1 << 7, // device supports relative addressing
    CAP_W32_XFER    = 1 << 6, // device supports 32-bit wide transfers
    CAP_W16_XFER    = 1 << 5, // device supports 16-bit wide transfers
    CAP_SYNC_XFER   = 1 << 4, // device supports synchronous transfers
    CAP_LINKED_CMDS = 1 << 3, // device supports linked commands
    CAP_CMD_QUEUE   = 1 << 1, // device supports command queuing
    CAP_CMD_SRST    = 1 << 0, // device supports soft resets
};

/** Standard SCSI bus timing values measured in ns. */
constexpr uint64_t BUS_SETTLE_DELAY = 400;
constexpr uint64_t BUS_FREE_DELAY   = 800;
constexpr uint64_t BUS_CLEAR_DELAY  = 800;
constexpr uint64_t ARB_DELAY        = 2400;
constexpr uint64_t SEL_ABORT_TIME   = 200000;
constexpr uint64_t SEL_TIME_OUT     = 250000000;

constexpr auto SCSI_MAX_DEVS    = 8;

class ScsiBus;

typedef std::function<void()> action_callback;

class ScsiPhysDevice : public ScsiPhysInterface, public HWComponent {
public:
    ScsiPhysDevice(std::string name, int my_id) : ScsiPhysInterface(PHY_ID_SCSI) {
        this->set_name(name);
        supports_types(HWCompType::SCSI_DEV);
        this->scsi_id = my_id;
        this->lun = 0,
        this->cur_phase = ScsiPhase::BUS_FREE;
    }
    ~ScsiPhysDevice() = default;

    // ScsiPhysInterface methods
    bool last_sel_has_attention() override {
        return this->last_selection_has_attention;
    }

    uint8_t last_sel_msg() override {
        return this->last_selection_message;
    }

    void set_xfer_len(uint64_t len) override {}

    void set_status(uint8_t status_code) override {
        this->status = status_code;
    }

    virtual void notify(ScsiNotification notif_type, int param);
    virtual void next_step();
    virtual void prepare_xfer(ScsiBus* bus_obj, int& bytes_in, int& bytes_out);
    virtual void switch_phase(const int new_phase) override;
    virtual bool allow_phase_change();

    virtual bool has_data() { return this->data_size != 0; }
    virtual int  xfer_data();
    virtual int  send_data(uint8_t* dst_ptr, int count);
    virtual int  rcv_data(const uint8_t* src_ptr, const int count);

    virtual bool prepare_data() = 0;
    virtual bool get_more_data() = 0;
    virtual void process_command() = 0;
    virtual void process_message();

    void set_bus_object_ptr(ScsiBus *bus_obj_ptr) {
        this->bus_obj = bus_obj_ptr;
    }

protected:
    uint8_t     cmd_buf[16] = {};
    uint8_t     msg_buf[16] = {}; // TODO: clarify how big this one should be
    int         scsi_id;
    int         lun;
    int         initiator_id;
    int         cur_phase;
    uint8_t*    data_ptr = nullptr;
    int         data_size;
    int         incoming_size;
    uint8_t     status;

    bool        last_selection_has_attention = false;
    uint8_t     last_selection_message = 0;
    ScsiBus*    bus_obj;

    int         *seq_steps = nullptr;

    action_callback pre_xfer_action  = nullptr;
    action_callback post_xfer_action = nullptr;
};

/** This class provides a higher level abstraction for the SCSI bus. */
class ScsiBus : public HWComponent {
public:
    ScsiBus(const std::string name);
    ~ScsiBus() = default;

    static std::unique_ptr<HWComponent> create_ScsiMesh() {
        return std::unique_ptr<ScsiBus>(new ScsiBus("ScsiMesh"));
    }

    static std::unique_ptr<HWComponent> create_ScsiCurio() {
        return std::unique_ptr<ScsiBus>(new ScsiBus("ScsiCurio"));
    }

    void attach_scsi_devices(const std::string bus_suffix);

    // low-level state management
    void    register_device(int id, ScsiPhysDevice* dev_obj);

    int current_phase() const {
        return this->cur_phase;
    }

    int get_initiator_id() const {
        return this->initiator_id;
    }

    int get_target_id() const {
        return this->target_id;
    }

    // reading/writing control lines
    void        assert_ctrl_line(int id, uint16_t mask);
    void        release_ctrl_line(int id, uint16_t mask);
    void        release_ctrl_lines(int id);
    uint16_t    test_ctrl_lines(uint16_t mask);

    // reading/writing data lines
    uint8_t get_data_lines() { return this->data_lines; }

    // high-level control/status
    int  switch_phase(int id, int new_phase);
    bool begin_arbitration(int id);
    bool end_arbitration(int id);
    bool begin_selection(int initiator_id, int target_id, bool atn);
    void confirm_selection(int target_id);
    bool end_selection(int initiator_id, int target_id);
    void disconnect(int dev_id);
    bool pull_data(const int id, uint8_t* dst_ptr, const int size);
    bool push_data(const int id, const uint8_t* src_ptr, const int size);
    int  target_xfer_data();
    void target_next_step();
    bool negotiate_xfer(int& bytes_in, int& bytes_out);

protected:
    void change_bus_phase(int initiator_id);

private:
    // SCSI devices registered with this bus
    std::array<ScsiPhysDevice*, SCSI_MAX_DEVS> devices;

    // per-device state of the control lines
    uint16_t    dev_ctrl_lines[SCSI_MAX_DEVS] = {};

    uint16_t    ctrl_lines;
    int         cur_phase = ScsiPhase::BUS_FREE;
    int         arb_winner_id;
    int         initiator_id;
    int         target_id;
    uint8_t     data_lines;
};

#endif // SCSI_H
