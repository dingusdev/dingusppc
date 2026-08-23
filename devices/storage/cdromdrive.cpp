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

/** @file Virtual CD-ROM device implementation. */

#include <devices/storage/cdromdrive.h>
#include <loguru.hpp>

#include <cinttypes>
#include <cstring>
#include <string>

CdromDrive::CdromDrive() : BlockStorageDevice(31, CDR_STD_DATA_SIZE, 0xfffffffe) {
    this->is_writeable = false;
}

bool CdromDrive::insert_image(const std::string& filename, bool notify_guest) {
    if (this->medium_present()) {
        LOG_F(ERROR, "Cannot insert CD-ROM image while media is present");
        return false;
    }

    if (this->set_host_file(filename) < 0) {
        LOG_F(ERROR, "Could not open CD-ROM image file, %s", filename.c_str());
        return false;
    }

    this->data_offset = 0;
    this->detect_raw_image();

    // create single track descriptor
    this->tracks[0]  = {1, /*.trk_num*/ 0x14, /*.adr_ctrl*/ 0 /*.start_lba*/};
    this->num_tracks = 1;

    // create Lead-out descriptor containing all data
    this->tracks[1] = {LEAD_OUT_TRK_NUM, /*.trk_num*/ 0x14, /*.adr_ctrl*/
        static_cast<uint32_t>(this->size_blocks + 1) /*.start_lba*/};

    this->media_changed |= notify_guest;
    return true;
}

bool CdromDrive::eject_image() {
    if (!this->is_ready)
        return false;

    this->is_ready = false;
    this->img_file.close();

    this->size_bytes   = 0;
    this->size_blocks  = 0;
    this->cur_fpos     = 0;
    this->write_size   = 0;
    this->remain_size  = 0;
    this->raw_blk_size = this->block_size;
    this->data_offset  = 0;
    this->num_tracks   = 0;
    this->media_changed = false;
    std::memset(this->tracks, 0, sizeof(this->tracks));

    return true;
}

bool CdromDrive::detect_raw_image() {
    uint8_t block_hdr[16];

    // let's see if the image data starts with the Mode 1/2 sync pattern
    this->img_file.read(block_hdr, 0, sizeof(block_hdr));

    for (int i = 1; i <= 10; i++)
        if (block_hdr[i] != 0xFF)
            return false;

    if (block_hdr[0] != 0 || block_hdr[11] != 0)
        return false;

    // for now, we only support Mode 1 images
    if (block_hdr[15] == 1) {
        this->set_block_size(2352);
        this->data_offset = 16;
        return true;
    }

    return false;
}

uint8_t CdromDrive::hex_to_bcd(const uint8_t val) {
    uint8_t hi = val / 10;
    uint8_t lo = val % 10;
    return (hi << 4) | lo;
}

AddrMsf CdromDrive::lba_to_msf(const int lba) {
    return {hex_to_bcd( lba / 4500),     /*.min*/
            hex_to_bcd((lba / 75) % 60), /*.sec*/
            hex_to_bcd( lba % 75)        /*.frm*/
    };
}
