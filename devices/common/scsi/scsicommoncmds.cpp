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

#include <devices/common/scsi/scsicommoncmds.h>
#include <loguru.hpp>
#include <memaccess.h>

ScsiCommonCmds::ScsiCommonCmds() {
    this->enable_cmd(ScsiCommand::TEST_UNIT_READY);
    this->enable_cmd(ScsiCommand::REQ_SENSE);
    this->enable_cmd(ScsiCommand::INQUIRY);
}

void ScsiCommonCmds::process_command() {
    int next_phase;

    switch(this->cdb_ptr[0]) {
    case ScsiCommand::TEST_UNIT_READY:
        next_phase = this->test_unit_ready();
        break;
    case ScsiCommand::REQ_SENSE:
        next_phase = this->request_sense_new();
        break;
    case ScsiCommand::INQUIRY:
        next_phase = this->inquiry_new();
        break;
    default:
        LOG_F(ERROR, "no support for command 0x%X", this->cdb_ptr[0]);
        this->invalid_command();
        next_phase = ScsiPhase::STATUS;
    }

    phy_impl->switch_phase(next_phase);
}

// maps command group number to CDB length (SCSI-2 standard §7.2.1)
static const int CmdGroupLen[8] = {6, 10, 10, CMD_GRP_RESERVED, CMD_GRP_RESERVED, 12,
                                   CMD_GRP_VENDOR_SPECIFIC, CMD_GRP_VENDOR_SPECIFIC};

int ScsiCommonCmds::get_cdb_length(uint8_t op_code) {
    return CmdGroupLen[op_code >> 5];
}

int ScsiCommonCmds::verify_cdb() {
    if (phy_impl->get_phy_id() == PHY_ID_ATAPI)
        return 0; // don't try to verify ATAPI packets

    int cdb_len = this->get_cdb_length(this->cdb_ptr[0]);

    if (cdb_len < 0)
        return 0; // don't know how to verify non-standard CDBs

    // check the LUN field except for INQUIRY and REQ_SENSE
    if ((this->cdb_ptr[0] != ScsiCommand::INQUIRY) &&
        (this->cdb_ptr[0] != ScsiCommand::REQ_SENSE))
    {
        if (phy_impl->last_sel_has_attention() &&
           (phy_impl->last_sel_msg() & ScsiMessage::IDENTIFY))
        {
            uint8_t lun = phy_impl->last_sel_msg() & 7;

            if (!this->lun_supported(lun)) {
                LOG_F(ERROR, "unsupported LUN 0x%X", lun);
                this->invalid_lun();
                phy_impl->set_status(ScsiStatus::CHECK_CONDITION);
                return -1;
            }
        } else if (this->cdb_ptr[1] >> 5)
            LOG_F(WARNING, "non-zero LUN 0x%X in CDB", this->cdb_ptr[1] >> 5);
    }

    // check the control field
    uint8_t ctrl_byte = this->cdb_ptr[cdb_len - 1];

    if (ctrl_byte & 3) {
        if (this->linked_cmds_supported())
            this->link_ctrl = ctrl_byte;
        else {
            LOG_F(ERROR, "flag/link bit set in CDB control: 0x%X", ctrl_byte);
            this->invalid_cdb();
            return -1;
        }
    }

    return 0;
}

int ScsiCommonCmds::inquiry_new() {
    int alloc_len = this->get_xfer_len();
    int resp_len  = 36; // standard response length

    // go directly to the STATUS phase if alloc_len = 0
    if (!alloc_len) {
        LOG_F(WARNING, "INQUIRY: skip data transfer because alloc_len = 0");
        return ScsiPhase::STATUS;
    }

    if (phy_impl->get_phy_id() == PHY_ID_SCSI) {
        bool is_invalid = false;

        if (this->cdb_ptr[1] & 1) {
            LOG_F(ERROR, "INQUIRY: EVPD=1 not supported");
            this->set_field_pointer(1);
            this->set_bit_pointer(0);
            is_invalid = true;
        }

        if (this->cdb_ptr[2]) {
            LOG_F(ERROR, "INQUIRY: non-zero page code not supported");
            this->set_field_pointer(2);
            is_invalid = true;
        }

        if (is_invalid) {
            this->invalid_cdb();
            return ScsiPhase::STATUS;
        }
    }

    std::memset(this->buf_ptr, 0, resp_len);

    uint8_t lun = this->get_lun();

    if (lun) {
        // tell the host there is no device for this LUN
        this->buf_ptr[0] = (3 << 5) | ScsiDevType::UNKNOWN;
        LOG_F(WARNING, "INQUIRY: no device for LUN %d", lun);
    } else {
        this->buf_ptr[0] = this->dev_type;
        this->buf_ptr[1] = ((this->is_removable) ? 0x80 : 0) | this->dev_type_mod;
        this->buf_ptr[2] = this->std_versions;
        this->buf_ptr[3] = this->resp_fmt;
        this->buf_ptr[4] = 31; // additional length (n-5)
        this->buf_ptr[7] = this->cap_flags;
        std::memcpy(&this->buf_ptr[ 8], vendor_id,   sizeof(this->vendor_id));
        std::memcpy(&this->buf_ptr[16], product_id,  sizeof(this->product_id));
        std::memcpy(&this->buf_ptr[32], revision_id, sizeof(this->revision_id));
    }

    phy_impl->set_xfer_len(std::min(alloc_len, resp_len));

    return ScsiPhase::DATA_IN;
}

int ScsiCommonCmds::request_sense_new() {
    int alloc_len = this->get_xfer_len();
    int resp_len  = 18; // standard response length

    // go directly to the STATUS phase if alloc_len = 0
    if (!alloc_len) {
        LOG_F(WARNING, "REQ_SENSE: skip data transfer because alloc_len = 0");
        return ScsiPhase::STATUS;
    }

    std::memset(this->buf_ptr, 0, resp_len);

    this->buf_ptr[0] = 0x80 | 0x70; // valid bit + current errors
    this->buf_ptr[7] = 10; // additional sense length

    uint8_t lun = this->get_lun();

    if (lun) // unsupported LUN
        this->invalid_lun();

    this->buf_ptr[ 2] = this->sense_key;
    this->buf_ptr[12] = this->asc;
    this->buf_ptr[13] = this->ascq;

    if (this->sense_key == ScsiSense::ILLEGAL_REQ && this->field_ptr_valid) {
        this->buf_ptr[15] = this->is_cdb_err ? 0xC0 : 0x80;

        WRITE_WORD_BE_A(&this->buf_ptr[16], this->field_ptr);

        if (this->bit_ptr_valid)
            this->buf_ptr[15] |= (this->bit_ptr & 7) | 8;
    }

    this->reset_sense(); // clear last sense information

    phy_impl->set_xfer_len(std::min(alloc_len, resp_len));

    return ScsiPhase::DATA_IN;
}

int ScsiCommonCmds::test_unit_ready() {
    // Assume that LUN is okay and the status has been set to GOOD

    if (!phy_impl->is_device_ready()) {
        this->sense_key = ScsiSense::NOT_READY;
        this->asc       = phy_impl->not_ready_reason();
        this->ascq      = 0;
        phy_impl->set_status(ScsiStatus::CHECK_CONDITION);
    }

    return ScsiPhase::STATUS;
}

uint8_t ScsiCommonCmds::get_lun() {
    if (phy_impl->get_phy_id() == PHY_ID_ATAPI)
        return 0;

    if (phy_impl->last_sel_has_attention() &&
        (phy_impl->last_sel_msg() & ScsiMessage::IDENTIFY))
        return phy_impl->last_sel_msg() & 7;
    else
        return this->cdb_ptr[1] >> 5;
}

uint32_t ScsiCommonCmds::get_xfer_len() {
    switch(this->get_cdb_length(this->cdb_ptr[0])) {
    case 6:
        return this->cdb_ptr[4];
    case 10:
        return READ_WORD_BE_U(&this->cdb_ptr[7]);
    case 12:
        return READ_DWORD_BE_U(&this->cdb_ptr[6]);
    default:
        return 0;
    }
}

void ScsiCommonCmds::reset_sense() {
    this->sense_key         = ScsiSense::NO_SENSE;
    this->asc               = ScsiError::NO_ERROR;
    this->ascq              = 0;
    this->field_ptr_valid   = false;
    this->bit_ptr_valid     = false;
}

void ScsiCommonCmds::invalid_lun() {
    this->sense_key         = ScsiSense::ILLEGAL_REQ;
    this->asc               = ScsiError::INVALID_LUN;
    this->ascq              = 0;
    this->is_cdb_err        = true;
    this->field_ptr_valid   = true;
    this->field_ptr         = 1; // error in the 2nd byte
    this->bit_ptr_valid     = true;
    this->bit_ptr           = 7; // MSB number of the invalid bitfield
}

void ScsiCommonCmds::invalid_cdb() {
    this->sense_key     = ScsiSense::ILLEGAL_REQ;
    this->asc           = ScsiError::INVALID_CDB;
    this->ascq          = 0;
    this->is_cdb_err    = true;

    phy_impl->set_status(ScsiStatus::CHECK_CONDITION);
}

void ScsiCommonCmds::invalid_command() {
    LOG_F(ERROR, "unsupported command: 0x%02x", this->cdb_ptr[0]);

    this->sense_key         = ScsiSense::ILLEGAL_REQ;
    this->asc               = ScsiError::INVALID_CMD;
    this->ascq              = 0;
    this->is_cdb_err        = true;
    this->field_ptr_valid   = true;
    this->field_ptr         = 0; // error in the 1st byte

    phy_impl->set_status(ScsiStatus::CHECK_CONDITION);
}

void ScsiCommonCmds::illegal_request(uint8_t asc, uint8_t ascq, bool is_cdb) {
    this->sense_key     = ScsiSense::ILLEGAL_REQ;
    this->asc           = asc;
    this->ascq          = ascq;
    this->is_cdb_err    = is_cdb;

    phy_impl->set_status(ScsiStatus::CHECK_CONDITION);
}
