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

/** @file SCSI CD-ROM definitions. */

#ifndef SCSI_CDROM_H
#define SCSI_CDROM_H

#include <devices/common/scsi/scsi.h>
#include <devices/common/scsi/scsicdromcmds.h>

#include <cinttypes>
#include <string>

class ScsiCdrom : public ScsiPhysDevice, public ScsiCdromCmds {
public:
    ScsiCdrom(std::string name, int my_id);
    ~ScsiCdrom() = default;

    virtual void process_command() override;

    void set_lock_state(bool is_locked) override {
        this->drive_locked = is_locked ? 1 : 0;
        ScsiPhysDevice::set_lock_state(is_locked);
    }

protected:
    bool is_device_ready() override { return this->is_ready; }
    uint8_t not_ready_reason() override { return ScsiError::MEDIUM_NOT_PRESENT; }

    void mode_select_6(uint8_t param_len);

private:
    uint8_t data_buf[2048]  = {};
};

#endif // SCSI_CDROM_H
