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

/** @file Macintosh Superdrive definitions. */

#ifndef MAC_SUPERDRIVE_H
#define MAC_SUPERDRIVE_H

#include <core/timermanager.h>
#include <devices/common/hwcomponent.h>
#include <devices/floppy/floppyimg.h>

#include <cinttypes>
#include <memory>
#include <string>

// convert number of bytes to disk time = nbytes * bits_per_byte * 2 us
#define     MFM_BYTES_TO_DISK_TIME(bytes) USECS_TO_NSECS((bytes) * 8 * 2)

// timing constans for MFM disks
constexpr   uint32_t MFM_INDX_MARK_DELAY = MFM_BYTES_TO_DISK_TIME(146);
constexpr   uint32_t MFM_ADR_MARK_DELAY  = MFM_BYTES_TO_DISK_TIME(22);
constexpr   uint32_t MFM_SECT_DATA_DELAY = MFM_BYTES_TO_DISK_TIME(514);
constexpr   uint32_t MFM_DD_SECTOR_DELAY = MFM_BYTES_TO_DISK_TIME(658);
constexpr   uint32_t MFM_HD_SECTOR_DELAY = MFM_BYTES_TO_DISK_TIME(675);

constexpr   uint32_t MFM_DD_EOT_DELAY    = MFM_BYTES_TO_DISK_TIME( 6250 - 182);
constexpr   uint32_t MFM_HD_EOT_DELAY    = MFM_BYTES_TO_DISK_TIME(12500 - 204);

namespace MacSuperdrive {

/** Apple Drive status request addresses. */
enum StatusAddr : uint8_t {
    Step_Status   = 1,
    Motor_Status  = 2,
    Eject_Latch   = 3,
    Select_Head_0 = 4,
    MFM_Support   = 5,
    Double_Sided  = 6,
    Drive_Exists  = 7,
    Disk_In_Drive = 8,
    Write_Protect = 9,
    Track_Zero    = 0xA,
    Select_Head_1 = 0xC,
    Drive_Mode    = 0xD,
    Drive_Ready   = 0xE,
    Media_Kind    = 0xF
};

/** Apple Drive command addresses. */
enum CommandAddr : uint8_t {
    Step_Direction    = 0,
    Do_Step           = 1,
    Motor_On_Off      = 2,
    Eject_Disk        = 3,
    Reset_Eject_Latch = 4,
    Switch_Drive_Mode = 5,
};

/** Type of media currently in the drive. */
enum MediaKind : uint8_t {
    low_density  = 0,
    high_density = 1, // 1 or 2 MB disk
};

/** Disk recording method. */
enum RecMethod : int {
    GCR = 0,
    MFM = 1
};

typedef struct SectorHdr {
    int     track;
    int     side;
    int     sector;
    int     format;
} SectorHdr;

class MacSuperDrive : public HWComponent {
public:
    MacSuperDrive();
    ~MacSuperDrive() = default;

    void command(uint8_t addr, uint8_t value);
    uint8_t status(uint8_t addr);
    int insert_disk(std::string& img_path, int write_flag);
    void init_track_search(int pos);
    uint64_t sync_to_disk();
    uint64_t next_addr_mark_delay(uint8_t *next_sect_num);
    uint64_t next_sector_delay();
    SectorHdr current_sector_header();
    char* get_sector_data_ptr(int sector_num);
    uint64_t sector_data_delay();

    double get_current_track_delay();
    double get_address_mark_delay();

protected:
    void reset_params();
    void set_disk_phys_params();
    void switch_drive_mode(int mode);

private:
    uint8_t     has_disk;
    uint8_t     eject_latch;
    uint8_t     motor_stat;     // spindle motor status: 1 - on, 0 - off
    uint8_t     drive_mode;     // drive mode: 0 - GCR, 1 - MFM
    uint8_t     is_ready;
    uint8_t     track_zero;     // 1 - if head is at track zero
    int         step_dir;       // step direction -1/+1
    int         cur_track;      // track number the head is currently at
    int         cur_head;       // current head number: 1 - upper, 0 - lower
    int         cur_sector;     // current sector number
    int         next_sector;    // next sector number

    uint64_t    motor_on_time = 0;  // time in ns the spindle motor was switched on
    uint64_t    track_start_time = 0;
    uint64_t    sector_start_time = 0;

    // physical parameters of the currently inserted disk
    uint8_t     media_kind;
    uint8_t     wr_protect;
    uint8_t     format_byte;
    int         rec_method;
    int         num_tracks;
    int         num_sides;
    int         sectors_per_track[80];
    int         rpm_per_track[80];
    int         track2lblk[80];  // convert track number to first logical block number

    // timing parameters for MFM disks
    uint32_t    index_delay;     // number of ns needed to read the index mark
    uint32_t    addr_mark_delay; // number of ns needed to read an address index mark
    uint32_t    sector_delay;    // number of ns needed to read a sector
    uint32_t    eot_delay;       // number of ns until end of track gap

    std::unique_ptr<FloppyImgConverter>  img_conv;

    std::unique_ptr<char[]> disk_data;
};

}; // namespace MacSuperdrive

#endif // MAC_SUPERDRIVE_H
