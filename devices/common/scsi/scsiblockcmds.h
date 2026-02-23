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

#ifndef SCSI_BLOCK_COMMANDS_H
#define SCSI_BLOCK_COMMANDS_H

#include <devices/common/scsi/scsi.h>
#include <devices/common/scsi/scsicommoncmds.h>
#include <devices/storage/blockstoragedevice.h>

#include <cinttypes>

/** Base class for SCSI block device commands. */
class ScsiBlockCmds : public ScsiCommonCmds, public BlockStorageDevice {
public:
    ScsiBlockCmds(int cache_blocks = 256);
    ~ScsiBlockCmds() = default;

    void init_block_device(uint8_t medium_type, uint8_t dev_flags);

    virtual int read_new();
    virtual int write_new();
    virtual int read_capacity();

    void get_medium_type(uint8_t& medium_type, uint8_t& dev_flags) override {
        medium_type = this->medium_type;
        dev_flags   = this->device_flags;
    }

    int format_block_descriptors(uint8_t* out_ptr) override;

    int  format_error_recovery_page(uint8_t subpage, uint8_t ctrl, uint8_t *out_ptr,
                                    int avail_len);

protected:
    virtual void    process_command() override;

    uint32_t    get_lba();

    uint8_t medium_type  = 0;
    uint8_t device_flags = 0;
};

#endif // SCSI_BLOCK_COMMANDS_H
