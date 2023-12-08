/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-23 divingkatae and maximum
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

/** @file Virtual CD-ROM drive definitions. */

#ifndef CD_ROM_DRIVE_H
#define CD_ROM_DRIVE_H

#include <devices/storage/blockstoragedevice.h>

#include <cinttypes>
#include <functional>

/* Original CD-ROM addressing mode expressed
   in minutes, seconds and frames */
typedef struct {
    int     min;
    int     sec;
    int     frm;
} AddrMsf;

/* Descriptor for CD-ROM tracks in TOC */
typedef struct {
    uint8_t     trk_num;
    uint8_t     adr_ctrl;
    uint32_t    start_lba;
} TrackDescriptor;

#define CDROM_MAX_TRACKS    100
#define LEAD_OUT_TRK_NUM    0xAA

class CdromDrive : public BlockStorageDevice {
public:
    CdromDrive();
    virtual ~CdromDrive() = default;

    void set_error_callback(std::function<void(uint8_t, uint8_t)>&& err_cb) {
        this->set_error = std::move(err_cb);
    }

    bool medium_present() { return this->is_ready; }

    void insert_image(std::string filename);

    virtual uint32_t inquiry(uint8_t *cmd_ptr, uint8_t *data_ptr);
    virtual uint32_t mode_sense_ex(uint8_t *cmd_ptr, uint8_t *data_ptr);
    virtual uint32_t request_sense(uint8_t *data_ptr, uint8_t sense_key, uint8_t asc,
                                   uint8_t ascq);
    virtual uint32_t report_capacity(uint8_t *data_ptr);
    virtual uint32_t read_toc(uint8_t *cmd_ptr, uint8_t *data_ptr);

protected:
    std::function<void(uint8_t, uint8_t)>  set_error;
    AddrMsf lba_to_msf(const int lba);

    TrackDescriptor tracks[CDROM_MAX_TRACKS];
    int             num_tracks;
};

#endif // CD_ROM_DRIVE_H
