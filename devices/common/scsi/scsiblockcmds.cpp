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

ScsiBlockCmds::ScsiBlockCmds(int cache_blocks) : BlockStorageDevice(cache_blocks) {
    this->enable_cmd(ScsiCommand::READ_6);
    this->enable_cmd(ScsiCommand::READ_10);
    this->enable_cmd(ScsiCommand::READ_12);
    this->enable_cmd(ScsiCommand::WRITE_6);
    this->enable_cmd(ScsiCommand::WRITE_10);
    this->enable_cmd(ScsiCommand::WRITE_12);
    this->enable_cmd(ScsiCommand::READ_CAPACITY);

    this->add_page_getter(this, ModePage::ERROR_RECOVERY,
                          &ScsiBlockCmds::get_error_recovery_page);
}

void ScsiBlockCmds::init_block_device(uint8_t medium_type, uint8_t dev_flags) {
    this->medium_type  = medium_type;
    this->device_flags = dev_flags;

    phy_impl->set_read_more_data_cb(
        [this](int* dsize, uint8_t** dptr) {
            if (this->remain_size) {
                *dsize = this->read_more();
                *dptr  = (uint8_t *)this->data_cache.get();
                return true;
            } else
                return false;
        }
    );
}

void ScsiBlockCmds::process_command() {
    int next_phase;

    // use non-disk buffer by default
    phy_impl->set_buffer(this->buf_ptr);

    switch(this->cdb_ptr[0]) {
    case ScsiCommand::READ_6:
    case ScsiCommand::READ_10:
    case ScsiCommand::READ_12:
        next_phase = this->read_new();
        break;
    case ScsiCommand::WRITE_6:
    case ScsiCommand::WRITE_10:
    case ScsiCommand::WRITE_12:
        next_phase = this->write_new();
        break;
    case ScsiCommand::READ_CAPACITY:
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

    this->set_fpos(this->get_lba());
    phy_impl->set_xfer_len(this->read_begin(nblocks));
    phy_impl->set_buffer((uint8_t *)this->data_cache.get());

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

    this->set_fpos(this->get_lba());
    phy_impl->set_xfer_len(this->write_begin(nblocks));
    phy_impl->set_buffer((uint8_t *)this->data_cache.get());

    phy_impl->set_post_xfer_action([this]() {
            this->write_cache();
        }
    );

    return ScsiPhase::DATA_OUT;
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
        this->set_field_pointer(2),
        this->invalid_cdb();
        return ScsiPhase::STATUS;
    }

    uint32_t last_lba = this->size_blocks - 1;
    uint32_t blk_len  = this->block_size;

    WRITE_DWORD_BE_A(&this->buf_ptr[0], last_lba);
    WRITE_DWORD_BE_A(&this->buf_ptr[4], blk_len);

    phy_impl->set_xfer_len(8);

    return ScsiPhase::DATA_IN;
}

int ScsiBlockCmds::format_block_descriptors(uint8_t* out_ptr) {
    uint8_t density_code = 0;

    WRITE_DWORD_BE_A(&out_ptr[0], (density_code << 24) | (this->size_blocks & 0xFFFFFF));
    WRITE_DWORD_BE_A(&out_ptr[4], (this->block_size & 0xFFFFFF));

    return 8;
}

int ScsiBlockCmds::get_error_recovery_page(uint8_t ctrl, uint8_t subpage,
                                           uint8_t *out_ptr, int avail_len)
{
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

    switch(cmd_len) {
    case 6:
        return ((this->cdb_ptr[1] & 0x1F) << 16) | READ_WORD_BE_A(&this->cdb_ptr[2]);
    case 10:
    case 12:
        return READ_DWORD_BE_U(&this->cdb_ptr[2]);
    default:
        ABORT_F("unsupported command length %d in get_lba", cmd_len);
    }
}
