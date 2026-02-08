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

static char my_vendor_id[]   = "QUANTUM ";
static char my_product_id[]  = "Emulated Disk   ";
static char my_revision_id[] = "di01";

ScsiHardDisk::ScsiHardDisk(std::string name, int my_id) : ScsiPhysDevice(name, my_id),
    ScsiCommonCmds()
{
    this->data_buf_size = 1 << 22;
    this->data_buf_obj = std::unique_ptr<uint8_t[]>(new uint8_t[this->data_buf_size]);
    this->data_buf = this->data_buf_obj.get();
    this->set_phys_dev(this);
    this->set_cdb_ptr(this->cmd_buf);
    this->set_buf_ptr(this->data_buf);

    // populate device info for INQUIRY
    this->dev_type      = ScsiDevType::DIRECT_ACCESS; // direct access device (hard drive)
    this->is_removable  = false; // non-removable medium
    this->std_versions  = 2;     // ISO vers=0, ECMA vers=0, ANSI vers=2 (SCSI-2)
    this->resp_fmt      = 2;     // response data format: SCSI-2
    this->cap_flags     = CAP_SYNC_XFER; // indicate support for synchronous transfers
    this->set_vendor_id(my_vendor_id);
    this->set_product_id(my_product_id);
    this->set_revision_id(my_revision_id);
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

    if (this->verify_cdb() < 0) {
        this->switch_phase(ScsiPhase::STATUS);
        return;
    }

    this->pre_xfer_action  = nullptr;
    this->post_xfer_action = nullptr;

    // assume successful command execution
    this->status = ScsiStatus::GOOD;
    this->msg_buf[0] = ScsiMessage::COMMAND_COMPLETE;

    uint8_t* cmd = this->cmd_buf;

    switch (cmd[0]) {
    case ScsiCommand::FORMAT_UNIT:
        this->format();
        break;
    case ScsiCommand::REASSIGN:
        this->reassign();
        break;
    case ScsiCommand::READ_6:
        lba = ((cmd[1] & 0x1F) << 16) + (cmd[2] << 8) + cmd[3];
        this->read(lba, cmd[4], 6);
        break;
    case ScsiCommand::WRITE_6:
        lba = ((cmd[1] & 0x1F) << 16) + (cmd[2] << 8) + cmd[3];
        this->write(lba, cmd[4], 6);
        break;
    case ScsiCommand::MODE_SELECT_6:
        mode_select_6(cmd[4]);
        break;
    case ScsiCommand::MODE_SENSE_6:
        this->mode_sense_6();
        break;
    case ScsiCommand::START_STOP_UNIT:
        this->switch_phase(ScsiPhase::STATUS);
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
        break;
    case ScsiCommand::READ_BUFFER:
        read_buffer();
        break;
    default:
        ScsiCommonCmds::process_command();
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
        this->invalid_cdb();
        this->switch_phase(ScsiPhase::STATUS);
        return;
    }

    if (page_ctrl == 3) {
        LOG_F(INFO, "%s: page_ctrl 3 SAVED VALUES is not implemented", this->name.c_str());
        this->set_field_pointer(2), // error in the 3rd byte
        this->set_bit_pointer(7),   // starting with bit 7
        this->illegal_request(ScsiError::SAVING_NOT_SUPPORTED, 0);
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
        WRITE_WORD_BE_U(&p_buf[10],  64); // sectors per track in the outermost zone
        WRITE_WORD_BE_U(&p_buf[12], 512); // bytes per sector
        WRITE_WORD_BE_U(&p_buf[14],   1); // interleave factor
        WRITE_WORD_BE_U(&p_buf[16],  19); // track skew factor
        WRITE_WORD_BE_U(&p_buf[18],  25); // cylinder skew factor
        p_buf[20] = 0x80; // SSEC=1, HSEC=0, RMB=0, SURF=0, INS=0
        p_buf += page_size;
        got_page = true;
    }

    if (page_code == 4 || page_code == 0x3f) { // Rigid Disk Drive Geometry Page
        page_size = 24;
        p_buf[0]  =  4; // page code
        p_buf[1]  = page_size - 2; // page length
        std::memset(&p_buf[2], 0, 22);
        int num_cylinders = this->total_blocks / 64 / 4;
        p_buf[2] = (num_cylinders >> 16) & 0xFF;
        WRITE_WORD_BE_U(&p_buf[3], num_cylinders & 0xFFFFU); // number of cylinders
        p_buf[5] = 4; // number of heads
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
        this->invalid_cdb();
        this->switch_phase(ScsiPhase::STATUS);
        return;
    bad_sub_page:
        LOG_F(WARNING, "%s: unsupported page/subpage %02xh/%02xh in MODE_SENSE_6",
              this->name.c_str(), page_code, sub_page_code);
        this->set_field_pointer(2),
        this->invalid_cdb();
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
        this->set_field_pointer(2),
        this->invalid_cdb();
        this->switch_phase(ScsiPhase::STATUS);
        return;
    }

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

void ScsiHardDisk::reassign() {
    LOG_F(WARNING, "%s: attempt to reassign blocks on the disk!", this->name.c_str());

    TimerManager::get_instance()->add_oneshot_timer(
        NS_PER_SEC, [this]() { this->switch_phase(ScsiPhase::STATUS); });
}

void ScsiHardDisk::read(uint32_t lba, uint16_t transfer_len, uint8_t cmd_len) {
    uint32_t transfer_size = transfer_len;
    if (cmd_len == 6 && transfer_len == 0) {
        transfer_size = 256;
    }
    transfer_size *= this->sector_size;

    if (transfer_size > this->data_buf_size) {
        while (transfer_size > this->data_buf_size)
            this->data_buf_size <<= 1;
        this->data_buf_obj = std::unique_ptr<uint8_t[]>(new uint8_t[this->data_buf_size]);
        this->data_buf = this->data_buf_obj.get();
    }
    std::memset(this->data_buf, 0, transfer_size);

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

    if (transfer_size > this->data_buf_size) {
        while (transfer_size > this->data_buf_size)
            this->data_buf_size <<= 1;
        this->data_buf_obj = std::unique_ptr<uint8_t[]>(new uint8_t[this->data_buf_size]);
        this->data_buf = this->data_buf_obj.get();
    }

    uint64_t device_offset = (uint64_t)lba * this->sector_size;

    this->incoming_size = transfer_size;

    this->post_xfer_action = [this, device_offset]() {
        this->disk_img.write(this->data_buf, device_offset, this->incoming_size);
    };

    this->switch_phase(ScsiPhase::DATA_OUT);
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
