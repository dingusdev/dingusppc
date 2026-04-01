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

#include <devices/common/scsi/scsicdrom.h>
#include <loguru.hpp>

#include <cstring>

static char my_vendor_id[]   = "SONY    ";
static char my_product_id[]  = "CD-ROM CDU-8003A";
static char my_revision_id[] = "1.9a";

ScsiCdrom::ScsiCdrom(std::string name, int my_id) : ScsiPhysDevice(name, my_id)
{
    this->set_phys_dev(this);
    this->set_cdb_ptr(this->cmd_buf);
    this->set_buf_ptr(this->data_buf);

    this->init_block_device(1, 0, 1, true);

    // populate device info for INQUIRY
    this->dev_type      = ScsiDevType::CD_ROM;
    this->is_removable  = true; // removable medium
    this->std_versions  = 2;    // ISO vers=0, ECMA vers=0, ANSI vers=2 (SCSI-2)
    this->resp_fmt      = 2;    // response data format: SCSI-2
    this->cap_flags     = CAP_SYNC_XFER; // supports synchronous transfers
    this->set_vendor_id(my_vendor_id);
    this->set_product_id(my_product_id);
    this->set_revision_id(my_revision_id);

    this->add_page_getter(this, 49, &ScsiCdrom::get_apple_page_49);
}

void ScsiCdrom::process_command() {
    uint32_t lba;

    if (this->verify_cdb() < 0) {
        this->switch_phase(ScsiPhase::STATUS);
        return;
    }

    // assume successful command execution
    phy_impl->set_status(ScsiStatus::GOOD);
    this->msg_buf[0] = ScsiMessage::COMMAND_COMPLETE;

    // use internal data buffer by default
    phy_impl->set_buffer(this->data_buf);

    uint8_t* cmd = this->cmd_buf;

    switch (cmd[0]) {
    case ScsiCommand::MODE_SELECT_6:
        this->mode_select_6(cmd[4]);
        break;
    default:
        ScsiCdromCmds::process_command();
    }
}

// Apple SCSI/ATAPI driver requests this page in the case of a device error.
// PearPC implements it under the name "Apple Features".
// I couldn't find any drive whose firmware supports it.
int ScsiCdrom::get_apple_page_49(uint8_t subpage, uint8_t ctrl, uint8_t *out_ptr,
                                 int avail_len)
{
    LOG_F(WARNING, "Page 0x31 requested");

    if (subpage && subpage != 0xFFU)
        return FORMAT_ERR_BAD_SUBPAGE;

    if (ctrl == 3)
        return FORMAT_ERR_BAD_CONTROL;

    int page_size = 6;

    if (page_size > avail_len)
        return FORMAT_ERR_DATA_TOO_BIG;

    std::memset(out_ptr, 0, page_size);

    out_ptr[0] = '.';
    out_ptr[1] = 'A';
    out_ptr[2] = 'p';
    out_ptr[3] = 'p';

    return page_size;
}

void ScsiCdrom::mode_select_6(uint8_t param_len)
{
    if (!param_len)
        return;

    phy_impl->set_xfer_len(param_len);

    std::memset(&this->data_buf[0], 0, 512);

    this->post_xfer_action = [this]() {
        // TODO: parse the received mode parameter list here
        LOG_F(INFO, "Mode Select: received mode parameter list");
    };

    this->switch_phase(ScsiPhase::DATA_OUT);
}
