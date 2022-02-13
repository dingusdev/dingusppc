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

enum ScsiPhase : int {
    BUS_FREE = 0,
    ARBITRATION,
    SELECTION,
    RESELECTION,
    RESET,
};

enum ScsiMsg : int {
    CONFIRM_SEL = 1,
    BUS_PHASE_CHANGE,
};

/** Standard SCSI bus timing values measured in ns. */
#define BUS_SETTLE_DELAY    400
#define BUS_FREE_DELAY      800
#define BUS_CLEAR_DELAY     800
#define ARB_DELAY           2400
#define SEL_ABORT_TIME      200000
#define SEL_TIME_OUT        250000000

#define SCSI_MAX_DEVS   8

class ScsiDevice : public HWComponent {
public:
    ScsiDevice()  = default;
    ~ScsiDevice() = default;

    virtual void notify(ScsiMsg msg_type, int param) = 0;

private:
    int scsi_id;
};

/** This class provides a higher level abstraction for the SCSI bus. */
class ScsiBus : public HWComponent {
public:
    ScsiBus();
    ~ScsiBus() = default;

    // low-level control/status
    void register_device(int id, ScsiDevice* dev_obj);
    void assert_ctrl_line(int id, uint16_t mask);
    void release_ctrl_line(int id, uint16_t mask);
    void release_ctrl_lines(int id);
    int  current_phase() { return this->cur_phase; };

    // high-level control/status
    bool begin_arbitration(int id);
    bool end_arbitration(int id);
    bool begin_selection(int initiator_id, int target_id, bool atn);
    void confirm_selection(int target_id);
    bool end_selection(int initiator_id, int target_id);
    void disconnect(int dev_id);

protected:
    void change_bus_phase(int initiator_id);

private:
    std::string name;

    // SCSI devices registered with this bus
    std::array<ScsiDevice*, SCSI_MAX_DEVS> devices;

    // per-device state of the control lines
    uint16_t    dev_ctrl_lines[SCSI_MAX_DEVS];

    uint16_t    ctrl_lines;
    int         cur_phase;
    int         arb_winner_id;
    int         initiator_id;
    int         target_id;
    uint8_t     data_lines;
};

#endif // SCSI_H