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

/** @file SCSI bus definitions. */

#ifndef SCSI_H
#define SCSI_H

#include <devices/common/hwcomponent.h>

#include <array>
#include <cinttypes>
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
    SCSI_CTRL_SEL = 1 << 6,
    SCSI_CTRL_BSY = 1 << 7,
    SCSI_CTRL_RST = 1 << 8,
};

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
};

namespace ScsiStatus {
    enum : uint8_t {
        GOOD = 0,
        CHECK_CONDITION = 2,
    };
};

namespace ScsiMessage {
    enum : uint8_t {
        COMMAND_COMPLETE = 0,
    };
};

enum ScsiMsg : int {
    CONFIRM_SEL = 1,
    BUS_PHASE_CHANGE,
    SEND_CMD_BEGIN,
    SEND_CMD_END,
};

enum ScsiCommand : int {
    TEST_UNIT_READY  = 0x0,
    REWIND           = 0x1,
    REQ_SENSE        = 0x3,
    FORMAT           = 0x4,
    READ_BLK_LIMITS  = 0x5,
    READ_6           = 0x8,
    WRITE_6          = 0xA,
    SEEK_6           = 0xB,
    INQUIRY          = 0x12,
    VERIFY_6         = 0x13,
    MODE_SELECT_6    = 0x15,
    RELEASE_UNIT     = 0x17,
    ERASE_6          = 0x19,
    MODE_SENSE_6     = 0x1A,
    DIAG_RESULTS     = 0x1C,
    SEND_DIAGS       = 0x1D,
    READ_CAPAC_10    = 0x25,
    READ_10          = 0x28,
    WRITE_10         = 0x2A,
    VERIFY_10        = 0x2F,
    READ_LONG_10     = 0x35,
};

enum ScsiSense : int {
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

enum ScsiError : int {
    NO_ERROR       = 0x0,
    NO_SECTOR      = 0x1,
    WRITE_FAULT    = 0x3,
    DEV_NOT_READY  = 0x4,
    INVALID_CMD    = 0x20,
    INVALID_LBA    = 0x21,
    INVALID_CDB    = 0x24,
    INVALID_LUN    = 0x25,
    WRITE_PROTECT  = 0x27
};

/** Standard SCSI bus timing values measured in ns. */
#define BUS_SETTLE_DELAY    400
#define BUS_FREE_DELAY      800
#define BUS_CLEAR_DELAY     800
#define ARB_DELAY           2400
#define SEL_ABORT_TIME      200000
#define SEL_TIME_OUT        250000000

#define SCSI_MAX_DEVS   8

class ScsiBus;

class ScsiDevice : public HWComponent {
public:
    ScsiDevice(int my_id) {
        this->scsi_id = my_id;
        this->cur_phase = ScsiPhase::BUS_FREE;
    };
    ~ScsiDevice() = default;

    virtual void notify(ScsiBus* bus_obj, ScsiMsg msg_type, int param);
    virtual void next_step(ScsiBus* bus_obj);

    virtual bool prepare_data() = 0;
    virtual bool has_data() = 0;
    virtual bool send_bytes(uint8_t* dst_ptr, int count) = 0;

    virtual void process_command() = 0;

protected:
    uint8_t cmd_buf[16] = {};
    int     cur_phase;
    int     scsi_id;
};

/** This class provides a higher level abstraction for the SCSI bus. */
class ScsiBus : public HWComponent {
public:
    ScsiBus();
    ~ScsiBus() = default;

    // low-level state management
    void    register_device(int id, ScsiDevice* dev_obj);
    int     current_phase() { return this->cur_phase; };

    // reading/writing control lines
    void        assert_ctrl_line(int id, uint16_t mask);
    void        release_ctrl_line(int id, uint16_t mask);
    void        release_ctrl_lines(int id);
    uint16_t    test_ctrl_lines(uint16_t mask);

    // reading/writing data lines
    uint8_t get_data_lines() { return this->data_lines; };

    // high-level control/status
    int  switch_phase(int id, int new_phase);
    bool begin_arbitration(int id);
    bool end_arbitration(int id);
    bool begin_selection(int initiator_id, int target_id, bool atn);
    void confirm_selection(int target_id);
    bool end_selection(int initiator_id, int target_id);
    bool transfer_command(uint8_t* dst_ptr);
    void disconnect(int dev_id);
    bool target_request_data();
    bool target_pull_data(uint8_t* dst_ptr, int size);
    void target_next_step();

protected:
    void change_bus_phase(int initiator_id);

private:
    std::string name;

    // SCSI devices registered with this bus
    std::array<ScsiDevice*, SCSI_MAX_DEVS> devices;

    // per-device state of the control lines
    uint16_t    dev_ctrl_lines[SCSI_MAX_DEVS] = {};

    uint16_t    ctrl_lines;
    int         cur_phase;
    int         arb_winner_id;
    int         initiator_id;
    int         target_id;
    uint8_t     data_lines;
};

#endif // SCSI_H
