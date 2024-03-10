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

/** @file Generic SCSI Hard Disk emulation. */

#include <core/timermanager.h>
#include <devices/common/scsi/scsi.h>
#include <devices/common/scsi/scsihd.h>
#include <devices/deviceregistry.h>
#include <loguru.hpp>
#include <machines/machineproperties.h>
#include <memaccess.h>
#include <fstream>
#include <cstring>

using namespace std;

ScsiHardDisk::ScsiHardDisk(std::string name, int my_id) : ScsiDevice(name, my_id) {
}

void ScsiHardDisk::insert_image(std::string filename) {
    //We don't want to store everything in memory, but
    //we want to keep the hard disk available.
    if (!this->disk_img.open(filename))
        ABORT_F("%s: could not open image file %s", this->name.c_str(), filename.c_str());

    this->img_size = this->disk_img.size();
    uint64_t tb = (this->img_size + this->sector_size - 1) / this->sector_size;
    this->total_blocks = static_cast<int>(tb);
    if (this->total_blocks < 0 || tb != this->total_blocks) {
        ABORT_F("%s: file size is too large", this->name.c_str());
    }
}

void ScsiHardDisk::process_command() {
    uint32_t lba;

    this->pre_xfer_action  = nullptr;
    this->post_xfer_action = nullptr;

    // assume successful command execution
    this->status = ScsiStatus::GOOD;
    this->msg_buf[0] = ScsiMessage::COMMAND_COMPLETE;

    uint8_t* cmd = this->cmd_buf;

    switch (cmd[0]) {
    case ScsiCommand::TEST_UNIT_READY:
        this->test_unit_ready();
        break;
    case ScsiCommand::REWIND:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::REQ_SENSE:
        this->req_sense(cmd[4]);
        break;
    case ScsiCommand::FORMAT_UNIT:
        this->format();
        break;
    case ScsiCommand::READ_BLK_LIMITS:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::READ_6:
        lba = ((cmd[1] & 0x1F) << 16) + (cmd[2] << 8) + cmd[3];
        this->read(lba, cmd[4], 6);
        break;
    case ScsiCommand::WRITE_6:
        lba = ((cmd[1] & 0x1F) << 16) + (cmd[2] << 8) + cmd[3];
        this->write(lba, cmd[4], 6);
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
        mode_select_6(cmd[4]);
        break;
    case ScsiCommand::RELEASE_UNIT:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::ERASE_6:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::MODE_SENSE_6:
        this->mode_sense_6();
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
        lba = READ_DWORD_BE_U(&cmd[2]);
        if (cmd[1] & 1) {
            ABORT_F("%s: RelAdr bit set in READ_10", this->name.c_str());
        }
        this->read(lba, READ_WORD_BE_U(&cmd[7]), 10);
        break;
    case ScsiCommand::WRITE_10:
        lba = READ_DWORD_BE_U(&cmd[2]);
        this->write(lba, READ_WORD_BE_U(&cmd[7]), 10);
        this->switch_phase(ScsiPhase::DATA_OUT);
        break;
    case ScsiCommand::VERIFY_10:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::READ_BUFFER:
        read_buffer();
        break;
    case ScsiCommand::MODE_SENSE_10:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::READ_12:
        this->illegal_command(cmd);
        break;

    // CD-ROM specific commands
    case ScsiCommand::READ_TOC:
        this->illegal_command(cmd);
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

bool ScsiHardDisk::prepare_data() {
    switch (this->cur_phase) {
    case ScsiPhase::DATA_IN:
        this->data_ptr  = (uint8_t*)this->data_buf;
        this->data_size = this->bytes_out;
        break;
    case ScsiPhase::DATA_OUT:
        this->data_ptr  = (uint8_t*)this->data_buf;
        this->data_size = 0;
        break;
    case ScsiPhase::STATUS:
        if (!error) {
            this->data_buf[0] = ScsiStatus::GOOD;
        } else {
            this->data_buf[0] = ScsiStatus::CHECK_CONDITION;
        }
        this->bytes_out = 1;
        this->data_ptr = (uint8_t*)this->data_buf;
        this->data_size = this->bytes_out;
        break;
    case ScsiPhase::MESSAGE_IN:
        this->data_buf[0] = this->msg_code;
        this->bytes_out = 1;
        this->data_ptr = (uint8_t*)this->data_buf;
        this->data_size = this->bytes_out;
        break;
    default:
        LOG_F(WARNING, "%s: unexpected phase in prepare_data", this->name.c_str());
        return false;
    }
    return true;
}

int ScsiHardDisk::test_unit_ready() {
    this->switch_phase(ScsiPhase::STATUS);
    return ScsiError::NO_ERROR;
}

int ScsiHardDisk::req_sense(uint16_t alloc_len) {
    //if (!check_lun())
    //    return;

    int next_phase;

    int lun;
    if (this->last_selection_has_atention) {
        lun = this->last_selection_message & 7;
    }
    else {
        lun = cmd_buf[1] >> 5;
    }

    if (lun == this->lun) {
        this->status = ScsiStatus::GOOD;
        this->data_buf[ 2] = this->sense;      // Reserved:0xf0, Sense Key:0x0f ; e.g. ScsiSense::ILLEGAL_REQ
        this->data_buf[12] = this->asc;        // addition sense code
        this->data_buf[13] = this->ascq;       // additional sense qualifier
        this->data_buf[15] = this->sksv;       // SKSV:0x80, C/D:0x40, Reserved:0x30, BPV:8, Bit Pointer:7
        this->data_buf[16] = this->field >> 8; // field pointer
        this->data_buf[17] = this->field;
    }
    else {
        this->data_buf[ 2] = this->sense;      // Reserved:0xf0, Sense Key:0x0f ; e.g. ScsiSense::ILLEGAL_REQ
        this->data_buf[12] = 0x25;             // addition sense code = Logical Unit Not Supported
        this->data_buf[13] = 0;                // additional sense qualifier
        this->data_buf[15] = 0;                // SKSV:0x80, C/D:0x40, Reserved:0x30, BPV:8, Bit Pointer:7
        this->data_buf[16] = 0;                // field pointer
        this->data_buf[17] = 0;
    }

    {
        // FIXME: there should be a way to set the VALID and ILI bits.
        this->data_buf[ 0] = 0x70; // Valid:0x80, Error Code:0x7f
        this->data_buf[ 1] =    0; // segment number
        this->data_buf[ 3] =    0; // information
        this->data_buf[ 4] =    0;
        this->data_buf[ 5] =    0;
        this->data_buf[ 6] =    0;
        this->data_buf[ 7] =   10; // additional sense length
        this->data_buf[ 8] =    0; // command specific information
        this->data_buf[ 9] =    0;
        this->data_buf[10] =    0;
        this->data_buf[11] =    0;
        this->data_buf[14] =    0; // field replaceable unit code
        this->data_buf[18] =    0; // reserved
        this->data_buf[19] =    0; // reserved
    }

    this->bytes_out = alloc_len; // Open Firmware 1.0.5 asks for 18 bytes.

    this->switch_phase(ScsiPhase::DATA_IN);
    return ScsiError::NO_ERROR;
}

void ScsiHardDisk::inquiry() {
    int page_num  = cmd_buf[2];
    int alloc_len = cmd_buf[4];

    if (page_num) {
        ABORT_F("%s: invalid page number in INQUIRY", this->name.c_str());
    }

    if (alloc_len > 36) {
        LOG_F(INFO, "%s: %d bytes requested in INQUIRY", this->name.c_str(), alloc_len);
    }

    int lun;
    if (this->last_selection_has_atention) {
        LOG_F(INFO, "%s: INQUIRY (%d bytes) with ATN LUN = %02x & 7", this->name.c_str(), alloc_len, this->last_selection_message);
        lun = this->last_selection_message & 7;
    }
    else {
        LOG_F(INFO, "%s: INQUIRY (%d bytes) with NO ATN LUN = %02x >> 5", this->name.c_str(), alloc_len, cmd_buf[1]);
        lun = cmd_buf[1] >> 5;
    }

    this->data_buf[0] = (lun == this->lun) ? 0 : 0x7f; // device type: Direct-access block device (hard drive)
    this->data_buf[1] =    0; // non-removable media; 0x80 = removable media
    this->data_buf[2] =    2; // ANSI version: SCSI-2
    this->data_buf[3] =    1; // response data format
    this->data_buf[4] = 0x1F; // additional length
    this->data_buf[5] =    0;
    this->data_buf[6] =    0;
    this->data_buf[7] = 0x18; // supports synchronous xfers and linked commands
    std::memcpy(&this->data_buf[8], vendor_info, 8);
    std::memcpy(&this->data_buf[16], prod_info, 16);
    std::memcpy(&this->data_buf[32], rev_info, 4);
    //std::memcpy(&this->data_buf[36], serial_number, 8);
    //etc.

    if (alloc_len < 36) {
        LOG_F(ERROR, "%s: allocation length too small: %d", this->name.c_str(),
              alloc_len);
    }
    else {
        memset(&this->data_buf[36], 0, alloc_len - 36);
    }

    this->bytes_out = alloc_len;

    this->switch_phase(ScsiPhase::DATA_IN);
}

int ScsiHardDisk::send_diagnostic() {
    return 0x0;
}

void ScsiHardDisk::mode_select_6(uint8_t param_len) {
    if (!param_len) {
        this->switch_phase(ScsiPhase::STATUS);
        return;
    }

    this->incoming_size = param_len;

    std::memset(&this->data_buf[0], 0xDD, this->sector_size);

    this->post_xfer_action = [this]() {
        // TODO: parse the received mode parameter list here
    };

    this->switch_phase(ScsiPhase::DATA_OUT);
}

static char Apple_Copyright_Page_Data[] = "APPLE COMPUTER, INC   ";

void ScsiHardDisk::mode_sense_6() {
    uint8_t page_code = this->cmd_buf[2] & 0x3F;
    uint8_t page_ctrl = this->cmd_buf[2] >> 6;
    uint8_t sub_page_code = this->cmd_buf[3];
    uint8_t alloc_len = this->cmd_buf[4];

    if (page_ctrl == 1) {
        LOG_F(INFO, "%s: page_ctrl 1 CHANGEABLE VALUES is not implemented", this->name.c_str());
        this->status = ScsiStatus::CHECK_CONDITION;
        this->sense  = ScsiSense::ILLEGAL_REQ;
        this->asc    = 0x24; // Invalid Field in CDB
        this->ascq   = 0;
        this->sksv   = 0xc0; // sksv=1, C/D=Command, BPV=0, BP=0
        this->field  = 2;
        this->switch_phase(ScsiPhase::STATUS);
        return;
    }

    if (page_ctrl == 2) {
        LOG_F(ERROR, "%s: page_ctrl 2 DEFAULT VALUES is not implemented", this->name.c_str());
        this->status = ScsiStatus::CHECK_CONDITION;
        this->sense  = ScsiSense::ILLEGAL_REQ;
        this->asc    = 0x24; // Invalid Field in CDB
        this->ascq   = 0;
        this->sksv   = 0xc0; // sksv=1, C/D=Command, BPV=0, BP=0
        this->field  = 2;
        this->switch_phase(ScsiPhase::STATUS);
        return;
    }

    if (page_ctrl == 3) {
        LOG_F(INFO, "%s: page_ctrl 3 SAVED VALUES is not implemented", this->name.c_str());
        this->status = ScsiStatus::CHECK_CONDITION;
        this->sense  = ScsiSense::ILLEGAL_REQ;
        this->asc    = 0x39; // Saving Parameters Not Supported
        this->ascq   = 0;
        this->sksv   = 0;
        this->field  = 0;
        this->switch_phase(ScsiPhase::STATUS);
        return;
    }

    this->data_buf[ 1] =  0; // medium type
    this->data_buf[ 2] =  0; // 0:medium is not write protected; 0x80 write protected

    this->data_buf[ 3] =  8; // block description length
    WRITE_DWORD_BE_A(&this->data_buf[4], this->total_blocks);
    WRITE_DWORD_BE_A(&this->data_buf[8], this->sector_size);

    uint8_t *p_buf = &this->data_buf[12];
    bool got_page = false;
    int page_size;

    if (page_code == 1 || page_code == 0x3f) { // read-write error recovery page
        if (sub_page_code != 0x00 && sub_page_code != 0xff)
            goto bad_sub_page;
        page_size = 8;
        p_buf[0] = 1; // page code
        p_buf[1] = page_size - 2; // data size - 1
        std::memset(&p_buf[2], 0, 6);
        p_buf += page_size;
        got_page = true;
    }

    if (page_code == 3 || page_code == 0x3f) { // Format device page
        if (sub_page_code != 0x00 && sub_page_code != 0xff)
            goto bad_sub_page;
        page_size = 24;
        p_buf[ 0] =    3; // page code
        p_buf[ 1] = page_size - 2; // data size - 1
        std::memset(&p_buf[2], 0, 22);
        // default values taken from Empire 540/1080S manual
        WRITE_WORD_BE_U(&p_buf[ 2],   6); // tracks per defect zone
        WRITE_WORD_BE_U(&p_buf[ 4],   1); // alternate sectors per zone
        WRITE_WORD_BE_U(&p_buf[10],  92); // sectors per track in the outermost zone
        WRITE_WORD_BE_U(&p_buf[12], 512); // bytes per sector
        WRITE_WORD_BE_U(&p_buf[14],   1); // interleave factor
        WRITE_WORD_BE_U(&p_buf[16],  19); // track skew factor
        WRITE_WORD_BE_U(&p_buf[18],  25); // cylinder skew factor
        p_buf[20] = 0x80; // SSEC=1, HSEC=0, RMB=0, SURF=0, INS=0
        p_buf += page_size;
        got_page = true;
    }

    if (page_code == 0x30 || page_code == 0x3f) { // Copyright page for Apple certified drives
        if (sub_page_code != 0x00 && sub_page_code != 0xff)
            goto bad_sub_page;
        page_size = 24;
        p_buf[0] = 0x30; // page code
        p_buf[1] = page_size - 2; // data size - 1
        std::memcpy(&p_buf[2], Apple_Copyright_Page_Data, 22);
        p_buf += page_size;
        got_page = true;
    }

    if (!(got_page || page_code == 0x3f)) { // not any of the supported pages or all pages
        LOG_F(WARNING, "%s: unsupported page 0x%02x in MODE_SENSE_6", this->name.c_str(), page_code);
        this->status = ScsiStatus::CHECK_CONDITION;
        this->sense  = ScsiSense::ILLEGAL_REQ;
        this->asc    = 0x24; // Invalid Field in CDB
        this->ascq   = 0;
        this->sksv   = 0xc0; // sksv=1, C/D=Command, BPV=0, BP=0
        this->field  = 2;
        this->switch_phase(ScsiPhase::STATUS);
        return;
    bad_sub_page:
        LOG_F(WARNING, "%s: unsupported page/subpage %02xh/%02xh in MODE_SENSE_6", this->name.c_str(), page_code, sub_page_code);
        this->status = ScsiStatus::CHECK_CONDITION;
        this->sense  = ScsiSense::ILLEGAL_REQ;
        this->asc    = 0x24; // Invalid Field in CDB
        this->ascq   = 0;
        this->sksv   = 0xc0; // sksv=1, C/D=Command, BPV=0, BP=0
        this->field  = 3;
        this->switch_phase(ScsiPhase::STATUS);
        return;
    }

    // adjust for overall mode sense data length
    this->data_buf[0] = p_buf - this->data_buf - 1;

    this->bytes_out = std::min((int)alloc_len, (int)this->data_buf[0] + 1);

    this->switch_phase(ScsiPhase::DATA_IN);
}

void ScsiHardDisk::read_capacity_10() {
    uint32_t lba = READ_DWORD_BE_U(&this->cmd_buf[2]);

    if (this->cmd_buf[1] & 1) {
        ABORT_F("%s: RelAdr bit set in READ_CAPACITY_10", this->name.c_str());
    }

    if (!(this->cmd_buf[8] & 1) && lba) {
        LOG_F(ERROR, "%s: non-zero LBA for PMI=0", this->name.c_str());
        this->status = ScsiStatus::CHECK_CONDITION;
        this->sense  = ScsiSense::ILLEGAL_REQ;
        this->asc    = 0x24; // Invalid Field in CDB
        this->ascq   = 0;
        this->sksv   = 0xc0; // sksv=1, C/D=Command, BPV=0, BP=0
        this->field  = 8;
        this->switch_phase(ScsiPhase::STATUS);
        return;
    }

    if (!check_lun())
        return;

    uint32_t last_lba = this->total_blocks - 1;
    uint32_t blk_len  = this->sector_size;

    WRITE_DWORD_BE_A(&this->data_buf[0], last_lba);
    WRITE_DWORD_BE_A(&this->data_buf[4], blk_len);

    this->bytes_out = 8;

    this->switch_phase(ScsiPhase::DATA_IN);
}

void ScsiHardDisk::format() {
    LOG_F(WARNING, "%s: attempt to format the disk!", this->name.c_str());

    if (this->cmd_buf[1] & 0x10)
        ABORT_F("%s: defect list isn't supported yet", this->name.c_str());

    TimerManager::get_instance()->add_oneshot_timer(NS_PER_SEC, [this]() {
        this->switch_phase(ScsiPhase::STATUS);
    });
}

void ScsiHardDisk::read(uint32_t lba, uint16_t transfer_len, uint8_t cmd_len) {
    if (!check_lun())
        return;

    uint32_t transfer_size = transfer_len;

    std::memset(this->data_buf, 0, sizeof(this->data_buf));

    if (cmd_len == 6 && transfer_len == 0) {
        transfer_size = 256;
    }

    transfer_size *= this->sector_size;
    uint64_t device_offset = (uint64_t)lba * this->sector_size;

    this->disk_img.read(this->data_buf, device_offset, transfer_size);

    this->bytes_out = transfer_size;

    this->switch_phase(ScsiPhase::DATA_IN);
}

void ScsiHardDisk::write(uint32_t lba, uint16_t transfer_len, uint8_t cmd_len) {
    uint32_t transfer_size = transfer_len;

    if (cmd_len == 6 && transfer_len == 0) {
        transfer_size = 256;
    }

    transfer_size *= this->sector_size;
    uint64_t device_offset = (uint64_t)lba * this->sector_size;

    this->incoming_size = transfer_size;

    this->post_xfer_action = [this, device_offset]() {
        this->disk_img.write(this->data_buf, device_offset, this->incoming_size);
    };
}

void ScsiHardDisk::read_buffer() {
    uint8_t  mode = this->cmd_buf[1];
    uint32_t alloc_len = (this->cmd_buf[6] << 24) | (this->cmd_buf[7] << 16) |
                          this->cmd_buf[8];

    switch(mode) {
    case 0: // Combined header and data mode
        WRITE_DWORD_BE_A(&this->data_buf[0], 0x10000); // report buffer size of 64K
        break;
    default:
        ABORT_F("%s: unsupported mode %d in READ_BUFFER", this->name.c_str(), mode);
    }

    this->bytes_out = alloc_len;

    this->switch_phase(ScsiPhase::DATA_IN);
}
