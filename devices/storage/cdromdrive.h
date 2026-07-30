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

/** @file Virtual CD-ROM drive definitions. */

#ifndef CD_ROM_DRIVE_H
#define CD_ROM_DRIVE_H

#include <devices/storage/blockstoragedevice.h>

#include <cinttypes>
#include <functional>

/** CD/DVD drive read/write capabilities. */
enum CdDriveCapabilities : uint8_t {
    CDCAP_CDR_READ  = 0x01,
    CDCAP_CDE_READ  = 0x02,
    CDCAP_CDR_WRITE = 0x01,
    CDCAP_CDE_WRITE = 0x02,
};

/** Support for various disk formats and outputs. */
enum CdDriveFormatSupport : uint8_t {
    CDFMT_AUDIO_PLAY    = 1 << 0,
    CDFMT_COMPOSITE     = 1 << 1,
    CDFMT_PORT_1        = 1 << 2,
    CDFMT_PORT_2        = 1 << 3,
    CDFMT_MODE2_FORM1   = 1 << 4,
    CDFMT_MODE2_FORM2   = 1 << 5,
    CDFMT_MULTI_SESSION = 1 << 6,
};

/** Loading mechanism types. */
enum MechanismType : uint8_t {
    LDTYPE_CADDY    = 0,
    LDTYPE_TRAY     = 1,
    LDTYPE_POPUP    = 2,
};

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

constexpr auto CDROM_MAX_TRACKS = 100;
constexpr auto LEAD_OUT_TRK_NUM = 0xAA;
constexpr auto CDR_STD_DATA_SIZE = 2048;

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
    virtual uint32_t mode_sense_ex(bool is_sense_6, uint8_t *cmd_ptr, uint8_t *data_ptr);
    virtual uint32_t request_sense(uint8_t *data_ptr, uint8_t sense_key, uint8_t asc,
                                   uint8_t ascq);
    virtual uint32_t report_capacity(uint8_t *data_ptr);
    virtual uint32_t read_toc(uint8_t *cmd_ptr, uint8_t *data_ptr);

protected:
    std::function<void(uint8_t, uint8_t)>  set_error;
    uint8_t hex_to_bcd(const uint8_t val);
    AddrMsf lba_to_msf(const int lba);
    bool    detect_raw_image();

    TrackDescriptor tracks[CDROM_MAX_TRACKS];
    int             num_tracks;

    // drive capabilities
    uint8_t  read_cap     = CdDriveCapabilities::CDCAP_CDE_READ  |
                            CdDriveCapabilities::CDCAP_CDR_READ;
    uint8_t  write_cap    = 0;   // no write support
    uint8_t  fmt_support  = 0;
    uint8_t  ext_support  = 0;
    uint8_t  mech_type    = MechanismType::LDTYPE_TRAY;
    uint8_t  sw_lock_sup  = 1;   // drive supports locking/unlocking via SW
    uint8_t  sw_eject_sup = 1;   // drive supports ejecting via SW
    uint8_t  drive_locked = 0;   // 1 - drive is currently locked
    uint8_t  prevent_jump = 0;   // prevent jumper not present
    uint8_t  more_support = 0;
    uint16_t max_rd_speed = 706; // defaults to 4x
    uint16_t max_vol_levs = 2;   // audio can be only turned on and off
    uint16_t cur_rd_speed = 706; // defaults to 4x
    uint8_t  dgt_out_desc = 0;   // digital output format description
};

#endif // CD_ROM_DRIVE_H
