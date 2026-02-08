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
#include <devices/common/scsi/scsicommoncmds.h>
#include <devices/storage/cdromdrive.h>
#include <utils/imgfile.h>

#include <cinttypes>
#include <memory>
#include <string>

class ScsiCdrom : public ScsiPhysDevice, public CdromDrive, public ScsiCommonCmds {
public:
    ScsiCdrom(std::string name, int my_id);
    ~ScsiCdrom() = default;

    virtual void process_command() override;
    virtual bool prepare_data() override;
    virtual bool get_more_data() override;

protected:
    bool is_device_ready() override { return true; }

    // HACK: it shouldn't be here!
    void set_xfer_len(uint64_t len) override {
        this->bytes_out = len;
    }

    void    read(uint32_t lba, uint16_t nblocks, uint8_t cmd_len);
    void    mode_select_6(uint8_t param_len);

    void    mode_sense_6();
    void    read_capacity_10();

private:
    bool    eject_allowed = true;
    int     bytes_out = 0;
    uint8_t data_buf[2048] = {};
};

#endif // SCSI_CDROM_H
