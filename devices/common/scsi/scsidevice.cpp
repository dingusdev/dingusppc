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

#include <core/timermanager.h>
#include <devices/common/scsi/scsi.h>
#include <loguru.hpp>

#include <cinttypes>

void ScsiDevice::notify(ScsiBus* bus_obj, ScsiMsg msg_type, int param)
{
    if (msg_type == ScsiMsg::BUS_PHASE_CHANGE) {
        switch (param) {
        case ScsiPhase::RESET:
            LOG_F(INFO, "ScsiDevice %d: bus reset aknowledged", this->scsi_id);
            break;
        case ScsiPhase::SELECTION:
            // check if something tries to select us
            if (bus_obj->get_data_lines() & (1 << scsi_id)) {
                LOG_F(INFO, "ScsiDevice %d selected", this->scsi_id);
                TimerManager::get_instance()->add_oneshot_timer(
                    BUS_SETTLE_DELAY,
                    [this, bus_obj]() {
                        // don't confirm selection if BSY or I/O are asserted
                        if (bus_obj->test_ctrl_lines(SCSI_CTRL_BSY | SCSI_CTRL_IO))
                            return;
                        bus_obj->assert_ctrl_line(this->scsi_id, SCSI_CTRL_BSY);
                        bus_obj->confirm_selection(this->scsi_id);
                        if (bus_obj->test_ctrl_lines(SCSI_CTRL_ATN)) {
                            LOG_F(WARNING, "ScsiDevice: MESSAGE_OUT isn't supported yet");
                        }
                        bus_obj->transfer_command(this->cmd_buf);
                        this->process_command();
                        bus_obj->switch_phase(this->scsi_id, ScsiPhase::DATA_IN);
                        this->cur_phase = ScsiPhase::DATA_IN;
                });
            }
            break;
        }
    }
}

void ScsiDevice::next_step(ScsiBus* bus_obj)
{
    switch (this->cur_phase) {
    case ScsiPhase::DATA_IN:
        if (this->has_data()) {
            LOG_F(WARNING, "ScsiDevice: attempt to leave DATA_IN phase with xfer != 0");
        } else {
            bus_obj->switch_phase(this->scsi_id, ScsiPhase::STATUS);
            this->cur_phase = ScsiPhase::STATUS;
        }
        break;
    case ScsiPhase::STATUS:
        bus_obj->switch_phase(this->scsi_id, ScsiPhase::MESSAGE_IN);
        this->cur_phase = ScsiPhase::MESSAGE_IN;
        break;
    case ScsiPhase::MESSAGE_IN:
        this->cur_phase = ScsiPhase::BUS_FREE;
        break;
    case ScsiPhase::BUS_FREE:
        bus_obj->release_ctrl_lines(this->scsi_id);
        bus_obj->switch_phase(this->scsi_id, ScsiPhase::BUS_FREE);
        this->cur_phase = ScsiPhase::BUS_FREE;
        break;
    default:
        LOG_F(WARNING, "ScsiDevice: nothing to do for the current phase %d", this->cur_phase);
    }
}
