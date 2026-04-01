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

#include <devices/common/scsi/scsicdromcmds.h>
#include <loguru.hpp>
#include <memaccess.h>

#include <cstring>

ScsiCdromCmds::ScsiCdromCmds() {
    this->set_phys_block_dev(this);

    this->enable_cmd(ScsiCommand::READ_TOC);
}

void ScsiCdromCmds::process_command() {
    int next_phase;

    // use non-disk buffer by default
    phy_impl->set_buffer(this->buf_ptr);

    switch(this->cdb_ptr[0]) {
    case ScsiCommand::READ_TOC:
        next_phase = this->read_toc_new();
        break;
    default:
        ScsiBlockCmds::process_command();
        return;
    }

    phy_impl->switch_phase(next_phase);
}

int ScsiCdromCmds:: read_toc_new() {
    uint8_t start_track, session_num;
    int     tot_tracks, resp_len = 0;

    int     alloc_len  = this->get_xfer_len();
    bool    is_msf     = !!(this->cdb_ptr[1] & 2);
    bool    is_alt_fmt = false;

    // go directly to the STATUS phase if alloc_len = 0
    if (!alloc_len) {
        LOG_F(WARNING, "READ_TOC: skip data transfer because alloc_len = 0");
        return ScsiPhase::STATUS;
    }

    // SCSI-2 supports only one response format so the format field in CDB (byte 2)
    // is reserved and should be set to zero.
    // ATAPI CD-ROM draft rev 2.6 advises to grab format code from the upper
    // two bits of byte 9 if byte 2 is zero.
    // This extension is implemented in the most SCSI drives as well as in
    // Apple CD-ROM driver in Mac OS.
    // SCSI-3 devices should use byte 2 to specify the response format.
    // I decided to stick to the above described ATAPI way for now.
    // It should work for both SCSI and ATAPI as well as SCSI-3 devices.
    uint8_t format = this->cdb_ptr[2] & 7;

    if (!format) {
        format = this->cdb_ptr[9] >> 6;
        is_alt_fmt = true;
        LOG_F(9, "READ_TOC: grabbed format code 0x%X from byte 9", format);
    }

    uint8_t *out_ptr = this->buf_ptr;

    switch(format) {
    case 0: // Formatted TOC
        start_track = this->cdb_ptr[6];
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
            LOG_F(ERROR, "READ_TOC: invalid starting track %d", start_track);
            this->set_field_pointer(6),
            this->invalid_cdb();
            return ScsiPhase::STATUS;
        }

        // write TOC data header
        WRITE_WORD_BE_A(out_ptr, tot_tracks * 8 + 2); // TOC data length
        out_ptr[2] = 1; // 1st track number
        out_ptr[3] = this->num_tracks; // last track number
        out_ptr += 4;
        resp_len = 4;

        for (int tx = 0; tx < tot_tracks; tx++, out_ptr += 8, resp_len += 8) {
            if (resp_len >= alloc_len)
                break; // exit the loop if the output buffer is exhausted

            TrackDescriptor& td = this->tracks[start_track + tx - 1];

            // write TOC track descriptor
            out_ptr[0] = 0; // reserved
            out_ptr[1] = td.adr_ctrl;
            out_ptr[2] = td.trk_num;
            out_ptr[3] = 0; // reserved

            if (is_msf) {
                AddrMsf msf = lba_to_msf(td.start_lba + 150);
                out_ptr[4] = 0; // reserved
                out_ptr[5] = msf.min;
                out_ptr[6] = msf.sec;
                out_ptr[7] = msf.frm;
            } else
                WRITE_DWORD_BE_A(&out_ptr[4], td.start_lba);
        }
        break;
    case 1: // Multi-session info --> we currently support only one session
        resp_len = 12;
        std::memset(out_ptr, 0, resp_len);
        out_ptr[1] = 10; // TOC data length
        out_ptr[2] =  1; // first session number
        out_ptr[3] =  1; // last session number
        out_ptr[5] = this->tracks[0].adr_ctrl;
        out_ptr[6] = 1;
        if (is_msf) {
            AddrMsf msf = lba_to_msf(this->tracks[0].start_lba + 150);
            out_ptr[ 8] = 0; // reserved
            out_ptr[ 9] = msf.min;
            out_ptr[10] = msf.sec;
            out_ptr[11] = msf.frm;
        }
        break;
    case 2: // Raw TOC
        session_num = this->cdb_ptr[6];
        if (!session_num)
            session_num = 1;
        if (session_num > 1) {
            LOG_F(ERROR, "READ_TOC: invalid session number %d", session_num);
            this->set_field_pointer(6),
            this->invalid_cdb();
            return ScsiPhase::STATUS;
        }
        resp_len = 48;
        std::memset(out_ptr, 0, resp_len);
        // prepare data header
        out_ptr[1] = resp_len - 2; // TOC data length
        out_ptr[2] = 1; // first session number
        out_ptr[3] = 1; // last session number
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
        } else
            WRITE_DWORD_BE_U(&out_ptr[7], this->tracks[1].start_lba);
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
        break;
    default:
        LOG_F(ERROR, "READ_TOC: unsupported format %d", format);
        this->set_field_pointer(is_alt_fmt ? 9 : 2),
        this->set_bit_pointer(is_alt_fmt ? 7 : 3),
        this->invalid_cdb();
        return ScsiPhase::STATUS;
    }

    phy_impl->set_xfer_len(std::min(alloc_len, resp_len));

    return ScsiPhase::DATA_IN;
}
