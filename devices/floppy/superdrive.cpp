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

#include <core/timermanager.h>
#include <devices/floppy/floppyimg.h>
#include <devices/floppy/superdrive.h>
#include <loguru.hpp>

#include <cinttypes>

using namespace MacSuperdrive;

MacSuperDrive::MacSuperDrive()
{
    this->name = "Superdrive";
    this->supported_types = HWCompType::FLOPPY_DRV;

    this->eject_latch = 0; // eject latch is off

    this->reset_params();
}

void MacSuperDrive::reset_params()
{
    this->media_kind    = MediaKind::high_density;
    this->has_disk      = 0; // drive is empty
    this->motor_stat    = 0; // spindle motor is off
    this->motor_on_time = 0;
    this->cur_track     = 0; // current head position
    this->is_ready      = 0; // drive not ready

    // come up in the MFM mode by default
    this->switch_drive_mode(RecMethod::MFM);
}

void MacSuperDrive::command(uint8_t addr, uint8_t value)
{
    uint8_t new_motor_stat;

    LOG_F(9, "Superdrive: command addr=0x%X, value=%d", addr, value);

    switch(addr) {
    case CommandAddr::Step_Direction:
        this->step_dir = value ? -1 : 1;
        break;
    case CommandAddr::Do_Step:
        if (!value) {
            this->cur_track += this->step_dir;
            if (this->cur_track < 0)
                this->cur_track = 0;
            this->track_zero = this->cur_track == 0;
        }
        break;
    case CommandAddr::Motor_On_Off:
        new_motor_stat = value ^ 1;
        if (this->motor_stat != new_motor_stat) {
            this->motor_stat = new_motor_stat;
            if (new_motor_stat) {
                this->motor_on_time = TimerManager::get_instance()->current_time_ns();
                this->track_start_time = 0;
                this->sector_start_time = 0;
                this->init_track_search(-1);
                this->is_ready = 1;
                LOG_F(INFO, "Superdrive: turn spindle motor on");
            } else {
                this->motor_on_time = 0;
                this->is_ready = 0;
                LOG_F(INFO, "Superdrive: turn spindle motor off");
            }
        }
        break;
    case CommandAddr::Eject_Disk:
        if (value) {
            LOG_F(INFO, "Superdrive: disk ejected");
            this->eject_latch = 1;
            this->reset_params();
        }
    case CommandAddr::Reset_Eject_Latch:
        if (value) {
            this->eject_latch = 0;
        }
        break;
    case CommandAddr::Switch_Drive_Mode:
        if (this->drive_mode != (value ^ 1)) {
            switch_drive_mode(value ^ 1); // reverse logic
            this->is_ready = 1;
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
    case StatusAddr::Step_Status:
        return 1; // not sure what should be returned here
    case StatusAddr::Motor_Status:
        return this->motor_stat ^ 1; // reverse logic
    case StatusAddr::Eject_Latch:
        return this->eject_latch;
    case StatusAddr::Select_Head_0:
        return this->cur_head = 0;
    case StatusAddr::MFM_Support:
        return 1; // Superdrive does support MFM encoding scheme
    case StatusAddr::Double_Sided:
        return 1; // yes, Superdrive is double sided
    case StatusAddr::Drive_Exists:
        return 0; // tell the world I'm here
    case StatusAddr::Disk_In_Drive:
        return this->has_disk ^ 1;   // reverse logic (active low)!
    case StatusAddr::Write_Protect:
        return this->wr_protect ^ 1; // reverse logic
    case StatusAddr::Track_Zero:
        return this->track_zero ^ 1; // reverse logic
    case StatusAddr::Select_Head_1:
        return this->cur_head = 1;
    case StatusAddr::Drive_Mode:
        return this->drive_mode;
    case StatusAddr::Drive_Ready:
        return this->is_ready ^ 1;   // reverse logic
    case StatusAddr::Media_Kind:
        return this->media_kind ^ 1; // reverse logic!
    default:
        LOG_F(WARNING, "Superdrive: unimplemented status request, addr=0x%X", addr);
        return 0;
    }
}

int MacSuperDrive::insert_disk(std::string& img_path, int write_flag = 0)
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

        // disk is write-enabled by default
        this->wr_protect = write_flag;

        // everything is set up, let's say we got a disk
        this->has_disk = 1;
    } else {
        this->has_disk = 0;
    }

    return 0;
}

void MacSuperDrive::set_disk_phys_params()
{
    this->rec_method  = this->img_conv->get_disk_rec_method();
    this->num_tracks  = this->img_conv->get_number_of_tracks();
    this->num_sides   = this->img_conv->get_number_of_sides();
    this->media_kind  = this->img_conv->get_rec_density();
    this->format_byte = this->img_conv->get_format_byte();

    switch_drive_mode(this->rec_method);
}

void MacSuperDrive::switch_drive_mode(int mode)
{
    if (mode == RecMethod::GCR) {
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
                this->track2lblk[grp * 16 + trk] = blk_num;
                blk_num += (12 - grp) * this->num_sides;
            }
        }

        this->drive_mode = RecMethod::GCR;
    } else {
        int sectors_per_track = this->media_kind ? 18 : 9;

        // MFM disks use constant number of sectors per track
        // and the fixed rotational speed of 300 RPM
        for (int trk = 0; trk < 80; trk++) {
            this->sectors_per_track[trk] = sectors_per_track;
            this->rpm_per_track[trk] = 300;
            this->track2lblk[trk] = trk * sectors_per_track * 2;
        }

        // set up disk timing parameters
        this->index_delay     = MFM_INDX_MARK_DELAY;
        this->addr_mark_delay = MFM_ADR_MARK_DELAY;

        if (this->media_kind) {
            this->sector_delay = MFM_HD_SECTOR_DELAY;
            this->eot_delay    = MFM_HD_EOT_DELAY;
        } else {
            this->sector_delay = MFM_DD_SECTOR_DELAY;
            this->eot_delay    = MFM_DD_EOT_DELAY;
        }

        this->drive_mode = RecMethod::MFM;
    }
}

double MacSuperDrive::get_current_track_delay()
{
    return 60.0f / this->rpm_per_track[this->cur_track];
}

void MacSuperDrive::init_track_search(int pos)
{
    if (pos == -1) {
        // pick random sector number
        uint64_t seed = TimerManager::get_instance()->current_time_ns() >> 8;
        this->cur_sector = seed % this->sectors_per_track[this->cur_track];
        LOG_F(9, "Superdrive: current sector number set to %d", this->cur_sector);
    } else {
        this->cur_sector = pos;
    }
}

uint64_t MacSuperDrive::sync_to_disk()
{
    uint64_t cur_time_ns, track_time_ns;

    uint64_t track_delay = this->get_current_track_delay() * NS_PER_SEC;

    // look how much ns have been elapsed since the last motor enabling
    cur_time_ns = TimerManager::get_instance()->current_time_ns() - this->motor_on_time;

    if (!this->track_start_time ||
        (cur_time_ns - this->track_start_time) >= track_delay) {
        // subtract full disk revolutions
        track_time_ns = cur_time_ns % track_delay;

        this->track_start_time = cur_time_ns - track_time_ns;

        // return delay until the first address mark if we're currently
        // looking at the end of track
        if (track_time_ns >= this->eot_delay) {
            this->next_sector = 0;
            // CHEAT: don't account for the remaining time of the current track
            return this->index_delay + this->addr_mark_delay;
        }

        // return delay until the first address mark
        // if the first address mark was not reached yet
        if (track_time_ns < this->index_delay) {
            this->next_sector = 0;
            return this->index_delay + this->addr_mark_delay - track_time_ns;
        }
    } else {
        track_time_ns = cur_time_ns - this->track_start_time;
    }

    // subtract index field delay
    track_time_ns -= this->index_delay;

    // calculate current sector number from timestamp
    int cur_sect_num = this->cur_sector = (int)(track_time_ns / this->sector_delay);

    this->sector_start_time = this->track_start_time + cur_sect_num * this->sector_delay +
                              this->index_delay;

    if (this->cur_sector + 1 >= this->sectors_per_track[this->cur_track]) {
        uint64_t diff = track_delay - (cur_time_ns - this->track_start_time);
        this->next_sector = 0;
        this->track_start_time = 0;
        this->sector_start_time = 0;
        return diff + this->index_delay + this->addr_mark_delay;
    } else {
        this->next_sector = this->cur_sector + 1;
        uint64_t diff = cur_time_ns - this->sector_start_time;
        this->sector_start_time += this->sector_delay;
        return this->sector_delay - diff + this->addr_mark_delay;
    }
}

uint64_t MacSuperDrive::next_addr_mark_delay(uint8_t *next_sect_num)
{
    uint64_t delay;

    if (this->cur_sector + 1 >= this->sectors_per_track[this->cur_track]) {
        this->next_sector = 0;
        delay = this->index_delay + this->addr_mark_delay;
    } else {
        this->next_sector = this->cur_sector + 1;
        delay = this->addr_mark_delay;
    }

    if (next_sect_num) {
        *next_sect_num = this->next_sector + ((this->rec_method == RecMethod::MFM) ? 1 : 0);
    }

    return delay;
}

uint64_t MacSuperDrive::next_sector_delay()
{
    if (this->cur_sector + 1 >= this->sectors_per_track[this->cur_track]) {
        this->next_sector = 0;
        return this->sector_delay + this->index_delay + this->addr_mark_delay;
    }

    this->next_sector = this->cur_sector + 1;

    return this->sector_delay - this->addr_mark_delay;
}

uint64_t MacSuperDrive::sector_data_delay()
{
    return MFM_SECT_DATA_DELAY;
}

SectorHdr MacSuperDrive::current_sector_header()
{
    this->cur_sector = this->next_sector;

    // MFM sector numbering is 1-based so we need to bump sector number
    uint8_t sector_num = this->cur_sector + ((this->rec_method == RecMethod::MFM) ? 1 : 0);

    return SectorHdr {
        this->cur_track,
        this->cur_head,
        sector_num,
        this->format_byte
    };
}

char* MacSuperDrive::get_sector_data_ptr(int sector_num)
{
    return this->disk_data.get() +
        ((this->track2lblk[this->cur_track] +
         (this->cur_head * this->sectors_per_track[this->cur_track]) +
          sector_num - 1) * 512
    );
}
