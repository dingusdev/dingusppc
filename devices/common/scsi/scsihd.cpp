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
        ABORT_F("ScsiHardDisk: file size is too large");
    }
}

void ScsiHardDisk::process_command() {
    uint32_t lba          = 0;
    uint16_t transfer_len = 0;
    uint16_t alloc_len    = 0;

    uint8_t  page_code    = 0;
    uint8_t  subpage_code = 0;

    this->pre_xfer_action  = nullptr;
    this->post_xfer_action = nullptr;

    // assume successful command execution
    this->status = ScsiStatus::GOOD;
    this->msg_buf[0] = ScsiMessage::COMMAND_COMPLETE;

    uint8_t* cmd = this->cmd_buf;

    switch (cmd[0]) {
    case ScsiCommand::TEST_UNIT_READY:
        test_unit_ready();
        break;
    case ScsiCommand::REWIND:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::REQ_SENSE:
        alloc_len = cmd[4];
        req_sense(alloc_len);
        break;
    case ScsiCommand::FORMAT_UNIT:
        this->format();
        break;
    case ScsiCommand::READ_BLK_LIMITS:
        this->illegal_command(cmd);
        break;
    case ScsiCommand::READ_6:
        lba = ((cmd[1] & 0x1F) << 16) + (cmd[2] << 8) + cmd[3];
        transfer_len = cmd[4];
        read(lba, transfer_len, 6);
        break;
    case ScsiCommand::WRITE_6:
        lba          = ((cmd[1] & 0x1F) << 16) + (cmd[2] << 8) + cmd[3];
        transfer_len = cmd[4];
        write(lba, transfer_len, 6);
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
    case ScsiCommand::READ_CAPACITY_10:
        this->read_capacity_10();
        break;
    case ScsiCommand::READ_10:
        lba          = (cmd[2] << 24) + (cmd[3] << 16) + (cmd[4] << 8) + cmd[5];
        transfer_len = (cmd[7] << 8) + cmd[8];
        read(lba, transfer_len, 10);
        break;
    case ScsiCommand::WRITE_10:
        lba          = (cmd[2] << 24) + (cmd[3] << 16) + (cmd[4] << 8) + cmd[5];
        transfer_len = (cmd[7] << 8) + cmd[8];
        write(lba, transfer_len, 10);
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
        this->data_size = 1;
        break;
    case ScsiPhase::MESSAGE_IN:
        this->data_buf[0] = this->msg_code;
        this->data_size = 1;
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
    if (alloc_len != 252) {
        LOG_F(WARNING, "%s: inappropriate Allocation Length: %d", this->name.c_str(),
              alloc_len);
    }
    return ScsiError::NO_ERROR;    // placeholder - no sense
}

void ScsiHardDisk::inquiry() {
    int page_num  = cmd_buf[2];
    int alloc_len = cmd_buf[4];

    if (page_num) {
        ABORT_F("%s: invalid page number in INQUIRY", this->name.c_str());
    }

    if (alloc_len >= 36) {
        this->data_buf[0] = 0;    // device type: Direct-access block device
        this->data_buf[1] = 0;    // non-removable media
        this->data_buf[2] = 2;    // ANSI version: SCSI-2
        this->data_buf[3] = 1;    // response data format
        this->data_buf[4] = 0;    // additional length
        this->data_buf[5] = 0;
        this->data_buf[6] = 0;
        this->data_buf[7] = 0x18; // supports synchronous xfers and linked commands
        std::memcpy(data_buf + 8, vendor_info, 8);
        std::memcpy(data_buf + 16, prod_info, 16);
        std::memcpy(data_buf + 32, rev_info, 4);

        this->bytes_out  = 36;

        this->switch_phase(ScsiPhase::DATA_IN);
    }
    else {
        LOG_F(WARNING, "%s: allocation length too small: %d", this->name.c_str(),
              alloc_len);
    }
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
    uint8_t alloc_len = this->cmd_buf[4];

    this->data_buf[ 0] = 13; // initial data size
    this->data_buf[ 1] =  0; // medium type
    this->data_buf[ 2] =  0; // medium is write enabled
    this->data_buf[ 3] =  8; // block description length

    this->data_buf[ 4] =  0; // density code
    this->data_buf[ 5] =  (this->total_blocks >> 16) & 0xFFU;
    this->data_buf[ 6] =  (this->total_blocks >>  8) & 0xFFU;
    this->data_buf[ 7] =  (this->total_blocks      ) & 0xFFU;
    WRITE_DWORD_BE_A(&this->data_buf[8], this->sector_size);

    this->data_buf[12] = page_code;

    switch(page_code) {
    case 1: // read-write error recovery page
        this->data_buf[13] = 6; // data size - 1
        std::memset(&this->data_buf[14], 0, 6);
        break;
    case 3: // Format device page
        this->data_buf[13] = 22; // data size - 1
        std::memset(&this->data_buf[14], 0, 22);
        // default values taken from Empire 540/1080S manual
        this->data_buf[15] =    6; // tracks per defect zone
        this->data_buf[17] =    1; // alternate sectors per zone
        this->data_buf[23] =   92; // sectors per track in the outermost zone
        this->data_buf[27] =    1; // interleave factor
        this->data_buf[29] =   19; // track skew factor
        this->data_buf[31] =   25; // cylinder skew factor
        this->data_buf[32] = 0x80; // SSEC=1, HSEC=0, RMB=0, SURF=0, INS=0
        WRITE_WORD_BE_U(&this->data_buf[24], 512); // bytes per sector
        break;
    case 0x30: // Copyright page for Apple certified drives
        this->data_buf[13] = 22; // data size - 1
        std::memcpy(&this->data_buf[14], Apple_Copyright_Page_Data, 22);
        break;
    default:
        ABORT_F("%s: unsupported page %d in MODE_SENSE_6", this->name.c_str(), page_code);
    };

    // adjust for overall mode sense data length
    this->data_buf[0] += this->data_buf[13] + 1;

    this->bytes_out = std::min(alloc_len, (uint8_t)this->data_buf[0]);

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
        this->switch_phase(ScsiPhase::STATUS);
        return;
    }

    uint32_t last_lba = this->total_blocks - 1;
    uint32_t blk_len  = this->sector_size;

    WRITE_DWORD_BE_A(&data_buf[0], last_lba);
    WRITE_DWORD_BE_A(&data_buf[4], blk_len);

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
    uint32_t transfer_size = transfer_len;

    std::memset(data_buf, 0, sizeof(data_buf));

    if (cmd_len == 6 && transfer_len == 0) {
        transfer_size = 256;
    }

    transfer_size *= this->sector_size;
    uint64_t device_offset = (uint64_t)lba * this->sector_size;

    this->disk_img.read(data_buf, device_offset, transfer_size);

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
