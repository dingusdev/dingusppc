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

#include <devices/common/scsi/scsiblockcmds.h>
#include <loguru.hpp>
#include <memaccess.h>

ScsiBlockCmds::ScsiBlockCmds() {
    LOG_F(9, "ScsiBlockCmds: enabling block-command set");
    this->enable_cmd(ScsiCommand::READ_6);
    this->enable_cmd(ScsiCommand::READ_10);
    this->enable_cmd(ScsiCommand::READ_12);
    this->enable_cmd(ScsiCommand::WRITE_6);
    this->enable_cmd(ScsiCommand::WRITE_10);
    this->enable_cmd(ScsiCommand::WRITE_12);
    this->enable_cmd(ScsiCommand::START_STOP_UNIT);
    this->enable_cmd(ScsiCommand::PREVENT_ALLOW_MEDIUM_REMOVAL);
    this->enable_cmd(ScsiCommand::READ_CAPACITY);

    this->add_page_getter(this, ModePage::ERROR_RECOVERY,
                          &ScsiBlockCmds::get_error_recovery_page);
}

void ScsiBlockCmds::init_block_device(uint8_t medium_type, uint8_t dev_flags,
                                      uint8_t density_code, bool is_apple_compliant)
{
    LOG_F(9, "ScsiBlockCmds: init_block_device medium_type=0x%02X, flags=0x%02X, "
             "density=0x%02X, apple_compliant=%d",
          medium_type, dev_flags, density_code, is_apple_compliant);

    this->medium_type  = medium_type;
    this->device_flags = dev_flags;

    phy_impl->set_read_more_data_cb(
        [this](int* dsize, uint8_t** dptr) -> bool {
            if (this->blk_dev->get_remaining_size()) {
                *dsize = this->blk_dev->read_more();
                *dptr  = this->blk_dev->get_cache_ptr();
                return true;
            } else
                return false;
        }
    );

    if (this->blk_dev->medium_writable()) {
        phy_impl->set_write_more_data_cb(
            [this](int* dsize, uint8_t** dptr) -> bool {
                if (this->blk_dev->get_remaining_size()) {
                    *dsize = this->blk_dev->write_more();
                    *dptr  = this->blk_dev->get_cache_ptr();
                    return true;
                } else
                    return false;
            }
        );
    } else {
        phy_impl->set_write_more_data_cb(
            [this](int* dsize, uint8_t** dptr) {
                return false;
            }
        );
    }

    if (is_apple_compliant)
        this->add_page_getter(this, 0x30, &ScsiBlockCmds::get_apple_copyright_page);
}

void ScsiBlockCmds::process_command() {
    int next_phase;

    LOG_F(9, "ScsiBlockCmds: process_command opcode=0x%02X", this->cdb_ptr[0]);

    // use non-disk buffer by default
    phy_impl->set_buffer(this->buf_ptr);

    switch(this->cdb_ptr[0]) {
    case ScsiCommand::READ_6:
    case ScsiCommand::READ_10:
    case ScsiCommand::READ_12:
        LOG_F(9, "ScsiBlockCmds: dispatch READ_%d",
              this->cdb_ptr[0] == ScsiCommand::READ_6 ? 6 :
              this->cdb_ptr[0] == ScsiCommand::READ_10 ? 10 : 12);
        next_phase = this->read_new();
        break;
    case ScsiCommand::WRITE_6:
    case ScsiCommand::WRITE_10:
    case ScsiCommand::WRITE_12:
        LOG_F(9, "ScsiBlockCmds: dispatch WRITE_%d",
              this->cdb_ptr[0] == ScsiCommand::WRITE_6 ? 6 :
              this->cdb_ptr[0] == ScsiCommand::WRITE_10 ? 10 : 12);
        next_phase = this->write_new();
        break;
    case ScsiCommand::START_STOP_UNIT:
        LOG_F(9, "ScsiBlockCmds: dispatch START_STOP_UNIT");
        next_phase = this->start_stop_unit();
        break;
    case ScsiCommand::PREVENT_ALLOW_MEDIUM_REMOVAL:
        LOG_F(9, "ScsiBlockCmds: dispatch PREVENT_ALLOW_MEDIUM_REMOVAL, eject=%d",
              (this->cdb_ptr[4] & 1) == 0);
        phy_impl->set_eject_state((this->cdb_ptr[4] & 1) == 0);
        next_phase = ScsiPhase::STATUS;
        break;
    case ScsiCommand::READ_CAPACITY:
        LOG_F(9, "ScsiBlockCmds: dispatch READ_CAPACITY");
        next_phase = this->read_capacity();
        break;
    default:
        ScsiCommonCmds::process_command();
        return;
    }

    phy_impl->switch_phase(next_phase);
}

int ScsiBlockCmds::read_new() {
    int nblocks = this->get_xfer_len();

    // special case: zero transfer length means 256 blocks for READ_6
    if (this->cdb_ptr[0] == ScsiCommand::READ_6) {
        if (!nblocks)
            nblocks = 256;
    } else {
        if ((this->cdb_ptr[1] & 1) && !this->linked_cmds_supported()) {
            LOG_F(WARNING, "READ: RelAdr bit set");
            this->set_field_pointer(1);
            this->set_bit_pointer(0);
            this->invalid_cdb();
            return ScsiPhase::STATUS;
        }
    }

    // go directly to the STATUS phase if nblocks = 0
    if (!nblocks) {
        LOG_F(WARNING, "READ: skip data transfer because nblocks = 0");
        return ScsiPhase::STATUS;
    }

    this->blk_dev->set_fpos(this->get_lba());
    phy_impl->set_xfer_len(this->blk_dev->read_begin(nblocks));
    phy_impl->set_buffer(this->blk_dev->get_cache_ptr());

    return ScsiPhase::DATA_IN;
}

int ScsiBlockCmds::write_new() {
    int nblocks = this->get_xfer_len();

    // special case: zero transfer length means 256 blocks for WRITE_6
    if (this->cdb_ptr[0] == ScsiCommand::WRITE_6) {
        if (!nblocks)
            nblocks = 256;
    } else {
        if ((this->cdb_ptr[1] & 1) && !this->linked_cmds_supported()) {
            LOG_F(WARNING, "WRITE: RelAdr bit set");
            this->set_field_pointer(1);
            this->set_bit_pointer(0);
            this->invalid_cdb();
            return ScsiPhase::STATUS;
        }
    }

    // go directly to the STATUS phase if nblocks = 0
    if (!nblocks) {
        LOG_F(WARNING, "WRITE: skip data transfer because nblocks = 0");
        return ScsiPhase::STATUS;
    }

    this->blk_dev->set_fpos(this->get_lba());
    phy_impl->set_xfer_len(this->blk_dev->write_begin(nblocks));
    phy_impl->set_buffer(this->blk_dev->get_cache_ptr());

    phy_impl->set_post_xfer_action([this]() {
            this->blk_dev->write_cache();
        }
    );

    return ScsiPhase::DATA_OUT;
}

int ScsiBlockCmds::start_stop_unit() {
    if (this->cdb_ptr[4] & 1) {
        if (this->cdb_ptr[4] & 2)
            LOG_F(INFO, "START_STOP_UNIT: medium load requested");
    } else {
        if (this->cdb_ptr[4] & 2) {
            LOG_F(INFO, "START_STOP_UNIT: medium eject requested");
        }
    }
    return ScsiPhase::STATUS;
}

int ScsiBlockCmds::read_capacity() {
    uint32_t lba = this->get_lba();

    if ((this->cdb_ptr[1] & 1) && !this->linked_cmds_supported()) {
        LOG_F(WARNING, "READ_CAPACITY: RelAdr bit set");
        this->set_field_pointer(1);
        this->set_bit_pointer(0);
        this->invalid_cdb();
        return ScsiPhase::STATUS;
    }

    if (!(this->cdb_ptr[8] & 1) && lba) {
        LOG_F(ERROR, "READ_CAPACITY: non-zero LBA for PMI=0");
        this->set_field_pointer(2);
        this->invalid_cdb();
        return ScsiPhase::STATUS;
    }

    uint32_t last_lba = (uint32_t)std::min(
        this->blk_dev->get_size_in_blocks() - 1, (uint64_t)0xFFFFFFFFU);
    uint32_t blk_len = this->blk_dev->get_block_size();

    WRITE_DWORD_BE_A(&this->buf_ptr[0], last_lba);
    WRITE_DWORD_BE_A(&this->buf_ptr[4], blk_len);

    phy_impl->set_xfer_len(8);

    return ScsiPhase::DATA_IN;
}

int ScsiBlockCmds::format_block_descriptors(uint8_t* out_ptr) {
    uint32_t nblocks = (uint32_t)std::min(
        this->blk_dev->get_size_in_blocks(), (uint64_t)0xFFFFFFFFU);

    LOG_F(9, "ScsiBlockCmds: format_block_descriptors nblocks=%u, density=0x%02X, blk_size=%u",
          nblocks, this->density_code, this->blk_dev->get_block_size());

    WRITE_DWORD_BE_A(&out_ptr[0], nblocks);
    WRITE_DWORD_BE_A(&out_ptr[4], (this->density_code << 24) |
                    (this->blk_dev->get_block_size() & 0xFFFFFF));

    return 8;
}

int ScsiBlockCmds::get_error_recovery_page(uint8_t ctrl, uint8_t subpage,
                                           uint8_t *out_ptr, int avail_len)
{
    LOG_F(9, "ScsiBlockCmds: get_error_recovery_page ctrl=%d, subpage=0x%02X, avail=%d",
          ctrl, subpage, avail_len);

    if (subpage && subpage != 0xFFU)
        return FORMAT_ERR_BAD_SUBPAGE;

    if (ctrl == 3)
        return FORMAT_ERR_BAD_CONTROL;

    int page_size = 6;

    if (page_size > avail_len)
        return FORMAT_ERR_DATA_TOO_BIG;

    std::memset(out_ptr, 0, page_size);

    return page_size;
}

uint32_t ScsiBlockCmds::get_lba() {
    int cmd_len = this->get_cdb_length(this->cdb_ptr[0]);
    uint32_t lba;

    switch(cmd_len) {
    case 6:
        lba = ((this->cdb_ptr[1] & 0x1F) << 16) | READ_WORD_BE_A(&this->cdb_ptr[2]);
        break;
    case 10:
    case 12:
        lba = READ_DWORD_BE_U(&this->cdb_ptr[2]);
        break;
    default:
        ABORT_F("unsupported command length %d in get_lba", cmd_len);
    }

    LOG_F(9, "ScsiBlockCmds: get_lba opcode=0x%02X, cdb_len=%d, lba=%u",
          this->cdb_ptr[0], cmd_len, lba);
    return lba;
}

static char Apple_Copyright_Page_Data[] = "APPLE COMPUTER, INC   ";

int ScsiBlockCmds::get_apple_copyright_page(uint8_t ctrl, uint8_t subpage, uint8_t *out_ptr,
                                            int avail_len)
{
    LOG_F(9, "ScsiBlockCmds: get_apple_copyright_page ctrl=%d, subpage=0x%02X, avail=%d",
          ctrl, subpage, avail_len);

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
