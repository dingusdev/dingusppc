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

/** @file SCSI CD-ROM definitions. */

#ifndef SCSI_CDROM_H
#define SCSI_CDROM_H

#include <devices/common/scsi/scsi.h>

#include <cinttypes>
#include <fstream>
#include <memory>
#include <string>

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

class ScsiCdrom : public ScsiDevice {
public:
    ScsiCdrom(int my_id);
    ~ScsiCdrom() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<ScsiCdrom>(new ScsiCdrom(3));
    }

    void insert_image(std::string filename);

    virtual void process_command();
    virtual bool prepare_data();

protected:
    void    read(uint32_t lba, uint16_t transfer_len, uint8_t cmd_len);
    void    inquiry();
    void    mode_sense();
    void    read_toc();
    void    read_capacity();

private:
    AddrMsf lba_to_msf(const int lba);

private:
    std::fstream    cdr_img;
    uint64_t        img_size;
    int             total_frames;
    int             num_tracks;
    int             sector_size;
    bool            eject_allowed = true;
    int             bytes_out = 0;
    TrackDescriptor tracks[CDROM_MAX_TRACKS];

    char            data_buf[1 << 16]; // TODO: proper buffer management
};

#endif // SCSI_CDROM_H
