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

/** @file Macintosh Superdrive emulation. */

#include <devices/floppy/floppyimg.h>
#include <devices/floppy/superdrive.h>
#include <loguru.hpp>

#include <cinttypes>
#include <memory>

using namespace MacSuperdrive;

MacSuperDrive::MacSuperDrive()
{
    this->name = "Superdrive";
    this->supported_types = HWCompType::FLOPPY_DRV;

    this->media_kind = MediaKind::high_density;
    this->has_disk   = 0; // drive is empty
}

void MacSuperDrive::command(uint8_t addr, uint8_t value)
{
    LOG_F(9, "Superdrive: command addr=0x%X, value=%d", addr, value);

    switch(addr) {
    case CommandAddr::Motor_On_Off:
        if (value) {
            LOG_F(INFO, "Superdrive: turn spindle motor off");
        } else {
            LOG_F(INFO, "Superdrive: turn spindle motor on");
        }
        break;
    default:
        LOG_F(WARNING, "Superdrive: unimplemented command, addr=0x%X", addr);
    }
}

uint8_t MacSuperDrive::status(uint8_t addr)
{
    LOG_F(9, "Superdrive: status request, addr = 0x%X", addr);

    switch(addr) {
    case StatusAddr::MFM_Support:
        return 1; // Superdrive does support MFM encoding scheme
    case StatusAddr::Double_Sided:
        return 1; // yes, Superdrive is double sided
    case StatusAddr::Drive_Exists:
        return 0; // tell the world I'm here
    case StatusAddr::Disk_In_Drive:
        return this->has_disk ^ 1; // reverse logic (active low)!
    case StatusAddr::Media_Kind:
        return this->media_kind ^ 1; // reverse logic!
    default:
        LOG_F(WARNING, "Superdrive: unimplemented status request, addr=0x%X", addr);
        return 0;
    }
}

int MacSuperDrive::insert_disk(std::string& img_path)
{
    if (this->has_disk) {
        LOG_F(ERROR, "Superdrive: drive is not empty!");
        return -1;
    }

    FloppyImgConverter* img_conv = open_floppy_image(img_path);
    if (img_conv != nullptr) {
        this->img_conv = std::unique_ptr<FloppyImgConverter>(img_conv);
        set_disk_phys_params();

        // allocate memory for raw disk data
        this->disk_data = std::unique_ptr<char[]>(new char[this->img_conv->get_data_size()]);

        // swallow all raw disk data at once!
        this->img_conv->get_raw_disk_data(this->disk_data.get());

        // everything is set up, let's say we got a disk
        this->has_disk = 1;
    } else {
        this->has_disk = 0;
    }

    return 0;
}

void MacSuperDrive::set_disk_phys_params()
{
    this->rec_method = this->img_conv->get_disk_rec_method();
    this->num_tracks = this->img_conv->get_number_of_tracks();
    this->num_sides  = this->img_conv->get_number_of_sides();
    this->media_kind = this->img_conv->get_rec_density();

    if (rec_method == RecMethod::GCR) {
        // Apple GCR speeds per group of 16 tracks
        static int gcr_rpm_per_group[5] = {394, 429, 472, 525, 590};

        // initialize three lookup tables:
        // sectors per track
        // rpm per track
        // logical block number of the first sector in each track
        // for easier navigation
        for (int grp = 0, blk_num = 0; grp < 5; grp++) {
            for (int trk = 0; trk < 16; trk++) {
                this->sectors_per_track[grp * 16 + trk] = 12 - grp;
                this->rpm_per_track[grp * 16 + trk] = gcr_rpm_per_group[grp];
                this->track_start_block[grp * 16 + trk] = blk_num;
                blk_num += 12 - grp;
            }
        }
    } else {
        int sectors_per_track = this->media_kind ? 18 : 9;

        // MFM disks use constant number of sectors per track
        // and the fixed rotational speed of 300 RPM
        for (int trk = 0; trk < 80; trk++) {
            this->sectors_per_track[trk] = sectors_per_track;
            this->rpm_per_track[trk] = 300;
            this->track_start_block[trk] = trk * sectors_per_track;
        }
    }
}
