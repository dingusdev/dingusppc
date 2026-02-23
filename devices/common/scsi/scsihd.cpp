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
#include <devices/common/scsi/scsihd.h>
#include <loguru.hpp>
#include <memaccess.h>

#include <cstring>

static char my_vendor_id[]   = "QUANTUM ";
static char my_product_id[]  = "Emulated Disk   ";
static char my_revision_id[] = "di01";

ScsiHardDisk::ScsiHardDisk(std::string name, int my_id) : ScsiPhysDevice(name, my_id)
{
    this->set_phys_dev(this);
    this->set_cdb_ptr(this->cmd_buf);
    this->set_buf_ptr(this->data_buf);
    this->set_buffer(this->data_buf);

    this->init_block_device(0, 0);

    // populate device info for INQUIRY
    this->dev_type      = ScsiDevType::DIRECT_ACCESS; // direct access device (hard drive)
    this->is_removable  = false; // non-removable medium
    this->std_versions  = 2;     // ISO vers=0, ECMA vers=0, ANSI vers=2 (SCSI-2)
    this->resp_fmt      = 2;     // response data format: SCSI-2
    this->cap_flags     = CAP_SYNC_XFER; // indicate support for synchronous transfers
    this->set_vendor_id(my_vendor_id);
    this->set_product_id(my_product_id);
    this->set_revision_id(my_revision_id);

    this->add_page_getter(this, ModePage::DEV_FORMAT_PARAMS, &ScsiHardDisk::get_dev_format_page);
    this->add_page_getter(this, ModePage::RIGID_DISK_GEOMETRY, &ScsiHardDisk::get_rigid_geometry_page);
    this->add_page_getter(this, 0x30, &ScsiHardDisk::get_apple_copyright_page);
}

void ScsiHardDisk::insert_image(std::string filename) {
    if (this->set_host_file(filename) < 0)
        ABORT_F("%s: could not open image file %s", this->name.c_str(), filename.c_str());

    if (this->size_blocks > 0xFFFFFFU)
        ABORT_F("%s: image file too large", this->name.c_str());

    this->is_writeable = true;
}

void ScsiHardDisk::process_command() {
    uint32_t lba;

    if (this->verify_cdb() < 0) {
        this->switch_phase(ScsiPhase::STATUS);
        return;
    }

    // use non-disk buffer by default
    phy_impl->set_buffer(this->data_buf);

    // assume successful command execution
    phy_impl->set_status(ScsiStatus::GOOD);
    this->msg_buf[0] = ScsiMessage::COMMAND_COMPLETE;

    uint8_t* cmd = this->cmd_buf;

    switch (cmd[0]) {
    case ScsiCommand::FORMAT_UNIT:
        this->format();
        break;
    case ScsiCommand::REASSIGN:
        this->reassign();
        break;
    case ScsiCommand::MODE_SELECT_6:
        mode_select_6(cmd[4]);
        break;
    case ScsiCommand::START_STOP_UNIT:
        this->switch_phase(ScsiPhase::STATUS);
        break;
    case ScsiCommand::PREVENT_ALLOW_MEDIUM_REMOVAL:
        this->eject_allowed = (cmd[4] & 1) == 0;
        this->switch_phase(ScsiPhase::STATUS);
        break;
    case ScsiCommand::READ_BUFFER:
        read_buffer();
        break;
    default:
        ScsiBlockCmds::process_command();
    }
}

void ScsiHardDisk::mode_select_6(uint8_t param_len) {
    if (!param_len) {
        this->switch_phase(ScsiPhase::STATUS);
        return;
    }

    phy_impl->set_xfer_len(param_len);

    std::memset(&this->data_buf[0], 0xDD, this->block_size);

    this->post_xfer_action = [this]() {
        // TODO: parse the received mode parameter list here
    };

    this->switch_phase(ScsiPhase::DATA_OUT);
}

int ScsiHardDisk::get_dev_format_page(uint8_t ctrl, uint8_t subpage, uint8_t *out_ptr,
                                      int avail_len)
{
    if (subpage && subpage != 0xFFU)
        return FORMAT_ERR_BAD_SUBPAGE;

    if (ctrl == 3)
        return FORMAT_ERR_BAD_CONTROL;

    int page_size = 22;

    if (page_size > avail_len)
        return FORMAT_ERR_DATA_TOO_BIG;

    std::memset(out_ptr, 0, page_size);

    // default values taken from Empire 540/1080S manual
    WRITE_WORD_BE_U(&out_ptr[ 0],   6); // tracks per defect zone
    WRITE_WORD_BE_U(&out_ptr[ 2],   1); // alternate sectors per zone
    WRITE_WORD_BE_U(&out_ptr[ 8],  64); // sectors per track in the outermost zone
    WRITE_WORD_BE_U(&out_ptr[10], this->block_size); // bytes per sector
    WRITE_WORD_BE_U(&out_ptr[12],   1); // interleave factor
    WRITE_WORD_BE_U(&out_ptr[14],  19); // track skew factor
    WRITE_WORD_BE_U(&out_ptr[16],  25); // cylinder skew factor

    out_ptr[18] = 0x80; // SSEC=1, HSEC=0, RMB=0, SURF=0, INS=0

    return page_size;
}

int ScsiHardDisk::get_rigid_geometry_page(uint8_t ctrl, uint8_t subpage, uint8_t *out_ptr,
                                          int avail_len)
{
    if (subpage && subpage != 0xFFU)
        return FORMAT_ERR_BAD_SUBPAGE;

    if (ctrl == 3)
        return FORMAT_ERR_BAD_CONTROL;

    int page_size = 22;

    if (page_size > avail_len)
        return FORMAT_ERR_DATA_TOO_BIG;

    std::memset(out_ptr, 0, page_size);

    // num_cylinders = total_blocks / sectors_per_track / number_of_heads
    int num_cylinders = this->size_blocks / 64 / 4;

    // num_cylinders is a 24bit value!
    out_ptr[0] = (num_cylinders >> 16) & 0xFF;
    WRITE_WORD_BE_U(&out_ptr[1], num_cylinders & 0xFFFFU);

    out_ptr[3] = 4; // number of heads

    return page_size;
}

static char Apple_Copyright_Page_Data[] = "APPLE COMPUTER, INC   ";

int ScsiHardDisk::get_apple_copyright_page(uint8_t ctrl, uint8_t subpage, uint8_t *out_ptr,
                                           int avail_len)
{
    if (subpage && subpage != 0xFFU)
        return FORMAT_ERR_BAD_SUBPAGE;

    if (ctrl == 3)
        return FORMAT_ERR_BAD_CONTROL;

    int page_size = 22;

    if (page_size > avail_len)
        return FORMAT_ERR_DATA_TOO_BIG;

    std::memcpy(out_ptr, Apple_Copyright_Page_Data, page_size);

    return page_size;
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

    phy_impl->set_xfer_len(alloc_len);

    this->switch_phase(ScsiPhase::DATA_IN);
}
