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

#include <devices/common/scsi/scsi.h> // reuse SCSI definitions
#include <devices/storage/cdromdrive.h>
#include <loguru.hpp>
#include <memaccess.h>

#include <cinttypes>
#include <cstring>
#include <string>

CdromDrive::CdromDrive() : BlockStorageDevice(31, CDR_STD_DATA_SIZE, 0xfffffffe) {
    this->is_writeable = false;
}

void CdromDrive::insert_image(std::string filename) {
    if (this->set_host_file(filename) < 0)
        ABORT_F("Could not open CD-ROM image file, %s", filename.c_str());

    this->detect_raw_image();

    // create single track descriptor
    this->tracks[0]  = {1, /*.trk_num*/ 0x14, /*.adr_ctrl*/ 0 /*.start_lba*/};
    this->num_tracks = 1;

    // create Lead-out descriptor containing all data
    this->tracks[1] = {LEAD_OUT_TRK_NUM, /*.trk_num*/ 0x14, /*.adr_ctrl*/
        static_cast<uint32_t>(this->size_blocks + 1) /*.start_lba*/};
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

static char cdrom_vendor_dingus_id[] = "DINGUS  ";
static char cdrom_product_id[]  = "DINGUS CD-ROM   ";
static char cdrom_revision_id[] = "1.0 ";

uint32_t CdromDrive::inquiry(uint8_t *cmd_ptr, uint8_t *data_ptr) {
    int page_num  = cmd_ptr[2];
    int alloc_len = cmd_ptr[4];

    if (page_num) {
        ABORT_F("%s: invalid page number in INQUIRY", "CdromDrive");
    }

    if (alloc_len > 36) {
        LOG_F(WARNING, "%s: more than 36 bytes requested in INQUIRY", "CdromDrive");
    }

    data_ptr[0] =    5; // device type: CD-ROM
    data_ptr[1] = 0x80; // removable media
    data_ptr[2] =    2; // ANSI version: SCSI-2
    data_ptr[3] = 0x21; // ATAPI Version + response data format
    data_ptr[4] = 0x1F; // additional length
    data_ptr[5] =    0;
    data_ptr[6] =    0;
    data_ptr[7] =    0;
    std::memcpy(&data_ptr[8],  cdrom_vendor_dingus_id, 8);
    std::memcpy(&data_ptr[16], cdrom_product_id, 16);
    std::memcpy(&data_ptr[32], cdrom_revision_id, 4);
    //std::memcpy(&data_ptr[36], serial_number, 8);
    //etc.

    if (alloc_len < 36) {
        LOG_F(ERROR, "%s: allocation length too small: %d", "CdromDrive",
            alloc_len);
    }
    else {
        memset(&data_ptr[36], 0, alloc_len - 36);
    }

    return alloc_len;
}

uint32_t CdromDrive::mode_sense_ex(bool is_sense_6, uint8_t* cmd_ptr, uint8_t* data_ptr) {
    uint8_t *resp_ptr;

    uint8_t page_code = cmd_ptr[2] & 0x3F;
    uint8_t alloc_len = is_sense_6 ? READ_WORD_BE_U(&cmd_ptr[4]) : READ_WORD_BE_U(&cmd_ptr[7]);

    std::memset(data_ptr, 0, alloc_len);

    data_ptr[0] = 0; // initial data size MSB
    data_ptr[1] = 6; // initial data size LSB
    data_ptr[2] = 1; // medium type: 120mm CD-ROM
    data_ptr[3] = 0; // device-specific parameter
    data_ptr[4] = 0;
    data_ptr[5] = 0;
    data_ptr[6] = 0; // block description length MSB
    data_ptr[7] = 0; // block description length LSB

    resp_ptr = data_ptr + 8;

    resp_ptr[0] = page_code;

    switch (page_code) {
    case ModePage::ERROR_RECOVERY:
        resp_ptr[1] = 6; // data size
        std::memset(&resp_ptr[2], 0, 6);
        data_ptr[1] += 8; // adjust overall length
        break;
    case ModePage::CDROM_AUDIO:
        resp_ptr[1] = 16;  // page length
        std::memset(&resp_ptr[2], 0, 16);
        resp_ptr[2] = (1 << 2); //immediately return status

        WRITE_WORD_BE_A(&resp_ptr[6], 75);     // logical block per second of audio playback

        resp_ptr[8]  = 0x01;    // Left Channel
        resp_ptr[9]  = 0xFF;    // Volume (0-255)
        resp_ptr[10] = 0x02;    // Right Channel
        resp_ptr[11] = 0xFF;    // Volume (0-255)

        resp_ptr[12] = 0x04;  // Output Port 2
        resp_ptr[13] = 0x00;  // Volume (0-255)
        resp_ptr[14] = 0x08;  // Output Port 3
        resp_ptr[15] = 0x00;  // Volume (0-255)

        data_ptr[1] += 8;  // adjust overall length
        break;
    case ModePage::POWER_CONDITION:
        resp_ptr[1] = 10;  // page length
        std::memset(&resp_ptr[2], 0, 10);
        resp_ptr[3] = 0; //idle and standby off
        WRITE_DWORD_BE_A(&resp_ptr[4], 0); //  Idle Timer (100ms units)
        WRITE_DWORD_BE_A(&resp_ptr[8], 0); //  Standby Timer (100ms units)
        data_ptr[1] += 12;  // adjust overall length
        break;
    case ModePage::CDROM_CAPABILITIES:
        resp_ptr[1] = 18; // page data length
        std::memset(&resp_ptr[2], 0, 18);
        resp_ptr[2] = 0x03; // supports reading CD-R and CD-E
        resp_ptr[3] = 0x00; // doesn't support writing to CD-R and CD-E
        resp_ptr[6] = (1 << 5) | (1 << 3) | (1 << 0); // tray type + eject + lock support
        WRITE_WORD_BE_A(&resp_ptr[ 8], 706); // report 4x speed
        WRITE_WORD_BE_A(&resp_ptr[10],   2); // supports audio on/off only
        WRITE_WORD_BE_A(&resp_ptr[12], 512); // 512Kb cache
        WRITE_WORD_BE_A(&resp_ptr[14], 706); // report current speed (4x)
        data_ptr[1] += 20; // adjust overall length
        break;
    case ModePage::VENDOR_PAGE_31:
        resp_ptr[1] = 14;    // page data length
        std::memset(&resp_ptr[2], 0, 14);
        resp_ptr[8] = 0x31;
        resp_ptr[9] = 6;
        resp_ptr[10] = '.';
        resp_ptr[11] = 'A';
        resp_ptr[12] = 'p';
        resp_ptr[13] = 'p';
        data_ptr[1] += 2;    // adjust overall length
        break;
    default:
        LOG_F(ERROR, "ATAPI CD-ROM: Invalid Page Code 0x%x", page_code);
        this->set_error(ScsiSense::ILLEGAL_REQ, ScsiError::INVALID_CDB);
        return 0;
    }

    return data_ptr[1] + 2; // return total response length
}

uint32_t CdromDrive::request_sense(uint8_t *data_ptr, uint8_t sense_key,
    uint8_t asc, uint8_t ascq) {
    std::memset(data_ptr, 0, 18);

    data_ptr[ 0] = 0x80 | 0x70; // valid + current error
    data_ptr[ 2] = sense_key;
    data_ptr[ 7] = 10; // additional sense length
    data_ptr[12] = asc;
    data_ptr[13] = ascq;

    return 18;
}

uint32_t CdromDrive::report_capacity(uint8_t *data_ptr) {
    WRITE_DWORD_BE_A(data_ptr, static_cast<uint32_t>(this->size_blocks));
    WRITE_DWORD_BE_A(&data_ptr[4], this->block_size);
    return 8;
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

uint32_t CdromDrive::read_toc(uint8_t *cmd_ptr, uint8_t *data_ptr) {
    int         tot_tracks, data_len;
    uint8_t     start_track, session_num;
    uint8_t*    out_ptr;
    uint16_t    alloc_len   = READ_WORD_BE_U(&cmd_ptr[7]);
    bool        is_msf      = !!(cmd_ptr[1] & 2);

    // ATAPI specific handling of the format field
    uint8_t format = cmd_ptr[2] & 7;
    if (!format) {
        format = cmd_ptr[9] >> 6;
    }

    if (!alloc_len)
        return 0;

    switch(format) {
    case 0: // Formatted TOC
        start_track = cmd_ptr[6];
        if (!start_track) { // special case: track zero (lead-in) as starting track
            // return all tracks starting with track 1 plus lead-out
            start_track = 1;
            tot_tracks  = this->num_tracks + 1;
        } else if (start_track == LEAD_OUT_TRK_NUM) {
            start_track = this->num_tracks + 1;
            tot_tracks  = 1;
        } else if (start_track <= this->num_tracks) {
            tot_tracks  = (this->num_tracks - start_track) + 2;
        } else {
            LOG_F(ERROR, "CDROM: invalid starting track %d in READ_TOC", start_track);
            this->set_error(ScsiSense::ILLEGAL_REQ, ScsiError::INVALID_CDB);
            return 0;
        }

        out_ptr = data_ptr;
        data_len = (tot_tracks * 8) + 4;

        // write TOC data header
        WRITE_WORD_BE_A(out_ptr, data_len - 2);
        out_ptr[2] = 1; // 1st track number
        out_ptr[3] = this->num_tracks; // last track number
        out_ptr += 4;

        for (int tx = 0; tx < tot_tracks; tx++) {
            if ((out_ptr - data_ptr + 8) > alloc_len)
                break; // exit the loop if the output buffer length is exhausted

            TrackDescriptor& td = this->tracks[start_track + tx - 1];

            *out_ptr++ = 0; // reserved
            *out_ptr++ = td.adr_ctrl;
            *out_ptr++ = td.trk_num;
            *out_ptr++ = 0; // reserved

            if (is_msf) {
                AddrMsf msf = lba_to_msf(td.start_lba + 150);
                *out_ptr++ = 0; // reserved
                *out_ptr++ = msf.min;
                *out_ptr++ = msf.sec;
                *out_ptr++ = msf.frm;
            } else {
                WRITE_DWORD_BE_A(out_ptr, td.start_lba);
                out_ptr += 4;
            }
        }
        return alloc_len;
    case 1: // Multi-session info
        std::memset(data_ptr, 0, 12);
        data_ptr[1] = 10; // TOC data length
        data_ptr[2] =  1; // first session number
        data_ptr[3] =  1; // last session number
        data_ptr[5] = this->tracks[0].adr_ctrl;
        data_ptr[6] = 1;
        if (is_msf) {
            AddrMsf msf = lba_to_msf(this->tracks[0].start_lba + 150);
            data_ptr[ 8] = 0; // reserved
            data_ptr[ 9] = msf.min;
            data_ptr[10] = msf.sec;
            data_ptr[11] = msf.frm;
        }
        return 12;
    case 2: // Raw TOC
        session_num = cmd_ptr[6];
        if (!session_num)
            session_num = 1;
        if (session_num > 1) {
            LOG_F(ERROR, "CDROM: invalid session number %d", session_num);
            this->set_error(ScsiSense::ILLEGAL_REQ, ScsiError::INVALID_CDB);
            return 0;
        }
        std::memset(data_ptr, 0, 48);
        out_ptr = data_ptr;
        // prepare data header
        out_ptr[1] = 46; // TOC data length
        out_ptr[2] =  1; // first session number
        out_ptr[3] =  1; // last session number
        out_ptr += 4;
        // descriptor #1 -> first track
        out_ptr[0] = session_num;
        out_ptr[1] = this->tracks[0].adr_ctrl;
        out_ptr[3] = 0xA0; // point -> first track
        out_ptr[8] = 1; // first track number
        out_ptr += 11;
        // descriptor #2 -> last track
        out_ptr[0] = session_num;
        out_ptr[1] = this->tracks[0].adr_ctrl;
        out_ptr[3] = 0xA1; // point -> last track
        out_ptr[8] = 1; // last track number
        out_ptr += 11;
        // descriptor #3 -> lead-out
        out_ptr[0] = session_num;
        out_ptr[1] = this->tracks[0].adr_ctrl;
        out_ptr[3] = 0xA2; // point -> lead-out
        if (is_msf) {
            AddrMsf msf = lba_to_msf(this->tracks[1].start_lba + 150);
            out_ptr[ 7] = 0; // reserved
            out_ptr[ 8] = msf.min;
            out_ptr[ 9] = msf.sec;
            out_ptr[10] = msf.frm;
        } else {
            WRITE_DWORD_BE_U(&out_ptr[7], this->tracks[1].start_lba);
        }
        out_ptr += 11;
        // descriptor #4 -> data track
        out_ptr[0] = session_num;
        out_ptr[1] = this->tracks[0].adr_ctrl;
        out_ptr[3] = 1; // point -> data track
        if (is_msf) {
            AddrMsf msf = lba_to_msf(this->tracks[0].start_lba + 150);
            out_ptr[ 7] = 0; // reserved
            out_ptr[ 8] = msf.min;
            out_ptr[ 9] = msf.sec;
            out_ptr[10] = msf.frm;
        }
        return 48;
    default:
        LOG_F(ERROR, "CDROM: unsupported format in READ_TOC");
        this->set_error(ScsiSense::ILLEGAL_REQ, ScsiError::INVALID_CDB);
        return 0;
    }
}
