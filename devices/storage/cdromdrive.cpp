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

/** @file Virtual CD-ROM device implementation. */

#include <devices/common/scsi/scsi.h> // reuse SCSI definitions
#include <devices/storage/cdromdrive.h>
#include <loguru.hpp>
#include <memaccess.h>

#include <cinttypes>
#include <cstring>
#include <string>

CdromDrive::CdromDrive() : BlockStorageDevice(31, 2048, 0xfffffffe) {
    this->is_writeable = false;
}

void CdromDrive::insert_image(std::string filename) {
    this->set_host_file(filename);

    // create single track descriptor
    this->tracks[0]  = {1, /*.trk_num*/ 0x14, /*.adr_ctrl*/ 0 /*.start_lba*/};
    this->num_tracks = 1;

    // create Lead-out descriptor containing all data
    this->tracks[1] = {LEAD_OUT_TRK_NUM, /*.trk_num*/ 0x14, /*.adr_ctrl*/
        static_cast<uint32_t>(this->size_blocks + 1) /*.start_lba*/};
}

static char cdrom_vendor_dingus_id[] = "DINGUS  ";
static char cdrom_product_id[]  = "DINGUS CD-ROM   ";
static char cdrom_revision_id[] = "1.0 ";

uint32_t CdromDrive::inquiry(uint8_t *cmd_ptr, uint8_t *data_ptr) {
    uint8_t alloc_len = cmd_ptr[4];

    if (alloc_len > 36) {
        LOG_F(WARNING, "CD-ROM: more than 36 bytes requested in INQUIRY");
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

    return 36;
}

uint32_t CdromDrive::mode_sense_ex(uint8_t *cmd_ptr, uint8_t *data_ptr) {
    uint8_t *resp_ptr;

    uint8_t page_code = cmd_ptr[2] & 0x3F;
    uint8_t alloc_len = READ_WORD_BE_U(&cmd_ptr[7]);

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
    case 0x01:
        resp_ptr[1] = 6; // data size
        std::memset(&resp_ptr[2], 0, 6);
        data_ptr[1] += 8; // adjust overall length
        break;
    case 0x2A:
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
    default:
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

AddrMsf CdromDrive::lba_to_msf(const int lba) {
    return {lba / 4500, /*.min*/ (lba / 75) % 60, /*.sec*/ lba % 75 /*.frm*/};
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
        return data_len;
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
