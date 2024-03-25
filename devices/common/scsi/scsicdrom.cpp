/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-24 divingkatae and maximum
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

/** @file SCSI CD-ROM emulation. */

#include <devices/common/scsi/scsi.h>
#include <devices/common/scsi/scsicdrom.h>
#include <devices/deviceregistry.h>
#include <loguru.hpp>
#include <machines/machineproperties.h>
#include <memaccess.h>

#include <cstring>

using namespace std;

static char cdrom_vendor_sony_id[] = "SONY    ";
static char cdu8003a_product_id[]  = "CD-ROM CDU-8003A";
static char cdu8003a_revision_id[] = "1.9a";

ScsiCdrom::ScsiCdrom(std::string name, int my_id) : CdromDrive(), ScsiDevice(name, my_id)
{
    this->pre_xfer_action  = nullptr;
    this->post_xfer_action = nullptr;
}

void ScsiCdrom::process_command()
{
    uint32_t lba, xfer_len;

    this->pre_xfer_action  = nullptr;
    this->post_xfer_action = nullptr;

    // assume successful command execution
    this->status = ScsiStatus::GOOD;

    // use internal data buffer by default
    this->data_ptr = this->data_buf;

    uint8_t* cmd = this->cmd_buf;

    switch (cmd[0]) {
    case ScsiCommand::TEST_UNIT_READY:
        this->test_unit_ready();
        break;
    case ScsiCommand::REWIND:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::REQ_SENSE:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::FORMAT_UNIT:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::READ_BLK_LIMITS:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::READ_6:
        lba = ((cmd[1] & 0x1F) << 16) + (cmd[2] << 8) + cmd[3];
        this->read(lba, cmd[4], 6);
        break;
    case ScsiCommand::WRITE_6:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::SEEK_6:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::INQUIRY:
        this->inquiry();
        break;
    case ScsiCommand::VERIFY_6:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::MODE_SELECT_6:
        this->incoming_size = cmd[4];
        this->switch_phase(ScsiPhase::DATA_OUT);
        break;
    case ScsiCommand::RELEASE_UNIT:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::ERASE_6:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::MODE_SENSE_6:
        this->mode_sense();
        break;
    case ScsiCommand::START_STOP_UNIT:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::DIAG_RESULTS:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::SEND_DIAGS:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::PREVENT_ALLOW_MEDIUM_REMOVAL:
        this->eject_allowed = (cmd[4] & 1) == 0;
        this->switch_phase(ScsiPhase::STATUS);
        break;
    case ScsiCommand::READ_CAPACITY_10:
        this->read_capacity_10();
        break;
    case ScsiCommand::READ_10:
        lba      = READ_DWORD_BE_U(&cmd[2]);
        xfer_len = READ_WORD_BE_U(&cmd[7]);
        if (cmd[1] & 1) {
            ABORT_F("%s: RelAdr bit set in READ_10", this->name.c_str());
        }
        read(lba, xfer_len, 10);
        break;
    case ScsiCommand::WRITE_10:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::VERIFY_10:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::READ_LONG_10:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::MODE_SENSE_10:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::READ_12:
        this->illegal_command(cmd);
        break;

    // CD-ROM specific commands
    case ScsiCommand::READ_TOC:
        this->read_toc();
        break;
    case ScsiCommand::SET_CD_SPEED:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::READ_CD:
        this->illegal_command(cmd);
        break;
    default:
        this->illegal_command(cmd);
    }
}

bool ScsiCdrom::prepare_data()
{
    switch (this->cur_phase) {
    case ScsiPhase::DATA_IN:
        this->data_size = this->bytes_out;
        break;
    case ScsiPhase::DATA_OUT:
        this->data_size = 0;
        break;
    case ScsiPhase::STATUS:
        break;
    default:
        LOG_F(WARNING, "%s: unexpected phase in prepare_data", this->name.c_str());
        return false;
    }
    return true;
}

bool ScsiCdrom::get_more_data() {
    if (this->data_left()) {
        this->data_size = this->read_more();
        this->data_ptr  = (uint8_t *)this->data_cache.get();
    }

    return this->data_size != 0;
}

void ScsiCdrom::read(const uint32_t lba, uint16_t nblocks, const uint8_t cmd_len)
{
    if (cmd_len == 6 && nblocks == 0)
        nblocks = 256;

    this->set_fpos(lba);
    this->data_ptr   = (uint8_t *)this->data_cache.get();
    this->bytes_out  = this->read_begin(nblocks, UINT32_MAX);

    this->msg_buf[0] = ScsiMessage::COMMAND_COMPLETE;
    this->switch_phase(ScsiPhase::DATA_IN);
}

int ScsiCdrom::test_unit_ready()
{
    this->switch_phase(ScsiPhase::STATUS);
    return ScsiError::NO_ERROR;
}

void ScsiCdrom::inquiry() {
    int page_num  = cmd_buf[2];
    int alloc_len = cmd_buf[4];

    if (page_num) {
        ABORT_F("%s: invalid page number in INQUIRY", this->name.c_str());
    }

    if (alloc_len > 36) {
        LOG_F(WARNING, "%s: more than 36 bytes requested in INQUIRY", this->name.c_str());
    }

    this->data_buf[0] =    5; // device type: CD-ROM
    this->data_buf[1] = 0x80; // removable media
    this->data_buf[2] =    2; // ANSI version: SCSI-2
    this->data_buf[3] =    1; // response data format
    this->data_buf[4] = 0x1F; // additional length
    this->data_buf[5] =    0;
    this->data_buf[6] =    0;
    this->data_buf[7] = 0x18; // supports synchronous xfers and linked commands
    std::memcpy(&this->data_buf[8],  cdrom_vendor_sony_id, 8);
    std::memcpy(&this->data_buf[16], cdu8003a_product_id, 16);
    std::memcpy(&this->data_buf[32], cdu8003a_revision_id, 4);

    this->bytes_out = 36;
    this->msg_buf[0] = ScsiMessage::COMMAND_COMPLETE;

    this->switch_phase(ScsiPhase::DATA_IN);
}

static char Apple_Copyright_Page_Data[] = "APPLE COMPUTER, INC   ";

void ScsiCdrom::mode_sense()
{
    uint8_t page_code = this->cmd_buf[2] & 0x3F;
    //uint8_t alloc_len = this->cmd_buf[4];

    int num_blocks = this->size_blocks;

    this->data_buf[ 0] =   13; // initial data size
    this->data_buf[ 1] =    0; // medium type
    this->data_buf[ 2] = 0x80; // medium is write protected
    this->data_buf[ 3] =    8; // block description length

    this->data_buf[ 4] =    0; // density code
    this->data_buf[ 5] = (num_blocks >> 16) & 0xFFU;
    this->data_buf[ 6] = (num_blocks >>  8) & 0xFFU;
    this->data_buf[ 7] = (num_blocks      ) & 0xFFU;
    this->data_buf[ 8] =    0;
    this->data_buf[ 9] =    0;
    this->data_buf[10] = (this->block_size >> 8) & 0xFFU;
    this->data_buf[11] = (this->block_size     ) & 0xFFU;

    this->data_buf[12] = page_code;

    switch (page_code) {
    case 0x01:
        this->data_buf[13] = 6; // data size
        std::memset(&this->data_buf[14], 0, 6);
        this->data_buf[0] += 7;
        break;
    case 0x30:
        this->data_buf[13] = 22; // data size
        std::memcpy(&this->data_buf[14], Apple_Copyright_Page_Data, 22);
        this->data_buf[0] += 23;
        break;
    case 0x31:
        this->data_buf[13] = 6; // data size
        std::memset(&this->data_buf[14], 0, 6);
        this->data_buf[14] = '.';
        this->data_buf[15] = 'A';
        this->data_buf[16] = 'p';
        this->data_buf[17] = 'p';
        break;
    default:
        ABORT_F("%s: unsupported page %d in MODE_SENSE_6", this->name.c_str(), page_code);
    }

    this->bytes_out = this->data_buf[0];
    this->msg_buf[0] = ScsiMessage::COMMAND_COMPLETE;

    this->switch_phase(ScsiPhase::DATA_IN);
}

void ScsiCdrom::read_toc()
{
    int         tot_tracks;
    uint8_t     start_track = this->cmd_buf[6];
    uint16_t    alloc_len   = (this->cmd_buf[7] << 8) + this->cmd_buf[8];
    bool        is_msf      = !!(this->cmd_buf[1] & 2);

    if (this->cmd_buf[2] & 0xF) {
        ABORT_F("%s: unsupported format in READ_TOC", this->name.c_str());
    }

    if (!alloc_len) {
        LOG_F(WARNING, "%s: zero allocation length passed to READ_TOC", this->name.c_str());
        return;
    }

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
        LOG_F(ERROR, "%s: invalid starting track %d in READ_TOC", this->name.c_str(),
              start_track);
        this->status = ScsiStatus::CHECK_CONDITION;
        this->sense  = ScsiSense::ILLEGAL_REQ;
        this->switch_phase(ScsiPhase::STATUS);
        return;
    }

    uint8_t* buf_ptr = this->data_buf;

    int data_len = (tot_tracks * 8) + 2;

    // write TOC data header
    *buf_ptr++ = (data_len >> 8) & 0xFFU;
    *buf_ptr++ = (data_len >> 0) & 0xFFU;
    *buf_ptr++ = 1; // 1st track number
    *buf_ptr++ = this->num_tracks; // last track number

    for (int tx = 0; tx < tot_tracks; tx++) {
        if ((buf_ptr - this->data_buf + 8) > alloc_len)
            break; // exit the loop if the output buffer length is exhausted

        TrackDescriptor& td = this->tracks[start_track + tx - 1];

        *buf_ptr++ = 0; // reserved
        *buf_ptr++ = td.adr_ctrl;
        *buf_ptr++ = td.trk_num;
        *buf_ptr++ = 0; // reserved

        if (is_msf) {
            AddrMsf msf = lba_to_msf(td.start_lba + 150);
            *buf_ptr++ = 0; // reserved
            *buf_ptr++ = msf.min;
            *buf_ptr++ = msf.sec;
            *buf_ptr++ = msf.frm;
        } else {
            *buf_ptr++ = (td.start_lba >> 24) & 0xFFU;
            *buf_ptr++ = (td.start_lba >> 16) & 0xFFU;
            *buf_ptr++ = (td.start_lba >>  8) & 0xFFU;
            *buf_ptr++ = (td.start_lba >>  0) & 0xFFU;
        }
    }

    this->bytes_out  = alloc_len;
    this->msg_buf[0] = ScsiMessage::COMMAND_COMPLETE;

    this->switch_phase(ScsiPhase::DATA_IN);
}

void ScsiCdrom::read_capacity_10()
{
    uint32_t lba = READ_DWORD_BE_U(&this->cmd_buf[2]);

    if (this->cmd_buf[1] & 1) {
        ABORT_F("%s: RelAdr bit set in READ_CAPACITY_10", this->name.c_str());
    }

    if (!(this->cmd_buf[8] & 1) && lba) {
        LOG_F(ERROR, "%s: non-zero LBA for PMI=0", this->name.c_str());
        this->status = ScsiStatus::CHECK_CONDITION;
        this->sense  = ScsiSense::ILLEGAL_REQ;
        this->switch_phase(ScsiPhase::STATUS);
        return;
    }

    int last_lba = this->size_blocks - 1;

    this->data_buf[0] = (last_lba >> 24) & 0xFFU;
    this->data_buf[1] = (last_lba >> 16) & 0xFFU;
    this->data_buf[2] = (last_lba >>  8) & 0xFFU;
    this->data_buf[3] = (last_lba >>  0) & 0xFFU;
    this->data_buf[4] = 0;
    this->data_buf[5] = 0;
    this->data_buf[6] = (this->block_size >> 8) & 0xFFU;
    this->data_buf[7] = this->block_size & 0xFFU;

    this->bytes_out  = 8;
    this->msg_buf[0] = ScsiMessage::COMMAND_COMPLETE;

    this->switch_phase(ScsiPhase::DATA_IN);
}
