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

/** @file SCSI hard drive definitions. */

#ifndef SCSI_HD_H
#define SCSI_HD_H

#include <devices/common/scsi/scsi.h>
#include <devices/common/scsi/scsicommoncmds.h>
#include <utils/imgfile.h>

#include <cinttypes>
#include <memory>
#include <string>

class ScsiHardDisk : public ScsiPhysDevice, public ScsiCommonCmds {
public:
    ScsiHardDisk(std::string name, int my_id);
    ~ScsiHardDisk() = default;

    void insert_image(std::string filename);
    void process_command() override;
    bool prepare_data() override;
    bool get_more_data() override { return false; }

protected:
    bool is_device_ready() override { return true; }

    // HACK: it shouldn't be here!
    void set_xfer_len(uint64_t len) override {
        this->bytes_out = len;
    }

    void mode_select_6(uint8_t param_len);

    void mode_sense_6();
    void format();
    void reassign();
    void read_capacity_10();
    void read(uint32_t lba, uint16_t transfer_len, uint8_t cmd_len);
    void write(uint32_t lba, uint16_t transfer_len, uint8_t cmd_len);
    void seek(uint32_t lba);
    void rewind();
    void read_buffer();

private:
    ImgFile         disk_img;
    uint64_t        img_size;
    int             total_blocks;
    uint64_t        file_offset = 0;
    static const int sector_size = 512;
    bool            eject_allowed = true;
    int             bytes_out = 0;

    std::unique_ptr<uint8_t[]> data_buf_obj = nullptr;
    uint8_t*        data_buf = nullptr;
    uint32_t        data_buf_size = 0;

    uint8_t         error = ScsiError::NO_ERROR;
    uint8_t         msg_code = 0;
};

#endif // SCSI_HD_H
