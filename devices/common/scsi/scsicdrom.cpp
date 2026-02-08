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

/** @file SCSI CD-ROM emulation. */

#include <devices/common/scsi/scsi.h>
#include <devices/common/scsi/scsicdrom.h>
#include <devices/deviceregistry.h>
#include <loguru.hpp>
#include <machines/machineproperties.h>
#include <memaccess.h>

#include <cstring>

using namespace std;

static char my_vendor_id[]   = "SONY    ";
static char my_product_id[]  = "CD-ROM CDU-8003A";
static char my_revision_id[] = "1.9a";

ScsiCdrom::ScsiCdrom(std::string name, int my_id) :
    CdromDrive(), ScsiPhysDevice(name, my_id), ScsiCommonCmds()
{
    this->set_phys_dev(this);
    this->set_cdb_ptr(this->cmd_buf);
    this->set_buf_ptr(this->data_buf);
    this->set_error_callback(
        [this](uint8_t sense_key, uint8_t asc) {
            //his->report_error(sense_key, asc);
            this->invalid_cdb(); // HACK! to make CdromDrive::read_toc() work
        }
    );

    // populate device info for INQUIRY
    this->dev_type      = ScsiDevType::CD_ROM;
    this->is_removable  = true; // removable medium
    this->std_versions  = 2;    // ISO vers=0, ECMA vers=0, ANSI vers=2 (SCSI-2)
    this->resp_fmt      = 2;    // response data format: SCSI-2
    this->cap_flags     = CAP_SYNC_XFER; // supports synchronous transfers
    this->set_vendor_id(my_vendor_id);
    this->set_product_id(my_product_id);
    this->set_revision_id(my_revision_id);
}

void ScsiCdrom::process_command()
{
    uint32_t lba;

    uint32_t cdda_start, cdda_end, cdda_len;

    if (this->verify_cdb() < 0) {
        this->switch_phase(ScsiPhase::STATUS);
        return;
    }

    this->pre_xfer_action  = nullptr;
    this->post_xfer_action = nullptr;

    // assume successful command execution
    this->status = ScsiStatus::GOOD;
    this->msg_buf[0] = ScsiMessage::COMMAND_COMPLETE;

    // use internal data buffer by default
    this->data_ptr = this->data_buf;

    uint8_t* cmd = this->cmd_buf;

    switch (cmd[0]) {
    case ScsiCommand::READ_6:
        lba = ((cmd[1] & 0x1F) << 16) + (cmd[2] << 8) + cmd[3];
        this->read(lba, cmd[4], 6);
        break;
    case ScsiCommand::MODE_SELECT_6:
        this->mode_select_6(cmd[4]);
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
        read(lba, READ_WORD_BE_U(&cmd[7]), 10);
        break;

    // CD-ROM specific commands
    case ScsiCommand::READ_TOC:
        this->bytes_out = read_toc(cmd, this->data_buf);
        if (this->status == ScsiStatus::GOOD) {
            this->msg_buf[0] = ScsiMessage::COMMAND_COMPLETE;
            this->switch_phase(ScsiPhase::DATA_IN);
        }
        break;
    default:
        ScsiCommonCmds::process_command();
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

void ScsiCdrom::read(uint32_t lba, uint16_t nblocks, uint8_t cmd_len)
{
    if (cmd_len == 6 && nblocks == 0)
        nblocks = 256;

    this->set_fpos(lba);
    this->data_ptr   = (uint8_t *)this->data_cache.get();
    this->bytes_out  = this->read_begin(nblocks, UINT32_MAX);

    this->msg_buf[0] = ScsiMessage::COMMAND_COMPLETE;
    this->switch_phase(ScsiPhase::DATA_IN);
}

static char Apple_Copyright_Page_Data[] = "APPLE COMPUTER, INC   ";

void ScsiCdrom::mode_sense_6()
{
    uint8_t page_code = this->cmd_buf[2] & 0x3F;
    //uint8_t alloc_len = this->cmd_buf[4];

    this->data_buf[ 0] =   13; // initial data size
    this->data_buf[ 1] =    0; // medium type
    this->data_buf[ 2] = 0x80; // medium is write protected
    this->data_buf[ 3] =    8; // block description length

    this->data_buf[ 4] =    0; // density code
    this->data_buf[ 5] = (this->size_blocks >> 16) & 0xFFU;
    this->data_buf[ 6] = (this->size_blocks >>  8) & 0xFFU;
    this->data_buf[ 7] = (this->size_blocks      ) & 0xFFU;
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
        LOG_F(WARNING, "%s: unsupported page 0x%02x in MODE_SENSE_6", this->name.c_str(),
              page_code);
        this->set_field_pointer(2), // error is in the 3rd byte
        this->set_bit_pointer(5),   // starting with bit 5
        this->invalid_cdb();
        this->switch_phase(ScsiPhase::STATUS);
        return;
    }

    this->bytes_out = this->data_buf[0];
    this->msg_buf[0] = ScsiMessage::COMMAND_COMPLETE;

    this->switch_phase(ScsiPhase::DATA_IN);
}

void ScsiCdrom::mode_select_6(uint8_t param_len)
{
    if (!param_len)
        return;

    this->incoming_size = param_len;

    std::memset(&this->data_buf[0], 0, 512);

    this->post_xfer_action = [this]() {
        // TODO: parse the received mode parameter list here
        LOG_F(INFO, "Mode Select: received mode parameter list");
    };

    this->switch_phase(ScsiPhase::DATA_OUT);
}

void ScsiCdrom::read_capacity_10()
{
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

    int last_lba = (int)this->size_blocks - 1;

    WRITE_DWORD_BE_A(&this->data_buf[0], last_lba);
    WRITE_DWORD_BE_A(&this->data_buf[4], this->block_size);

    this->bytes_out  = 8;
    this->msg_buf[0] = ScsiMessage::COMMAND_COMPLETE;

    this->switch_phase(ScsiPhase::DATA_IN);
}
