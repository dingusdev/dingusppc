/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-23 divingkatae and maximum
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

/** @file Descriptor-based direct memory access emulation. */

#include <cpu/ppc/ppcmmu.h>
#include <devices/common/dbdma.h>
#include <devices/common/dmacore.h>
#include <devices/common/pci/pcibase.h>
#include <endianswap.h>
#include <memaccess.h>

#include <cinttypes>
#include <cstring>
#include <loguru.hpp>

void DMAChannel::set_callbacks(DbdmaCallback start_cb, DbdmaCallback stop_cb) {
    this->start_cb = start_cb;
    this->stop_cb  = stop_cb;
}

/* Load DMACmd from physical memory. */
void DMAChannel::fetch_cmd(uint32_t cmd_addr, DMACmd* p_cmd) {
    memcpy((uint8_t*)p_cmd, mmu_get_dma_mem(cmd_addr, 16, nullptr), 16);
}

uint8_t DMAChannel::interpret_cmd() {
    DMACmd cmd_struct;
    bool   is_writable, branch_taken = false;

    if (this->cmd_in_progress) {
        // return current command if there is data to transfer
        if (this->queue_len)
            return this->cur_cmd;

        // obtain real pointer to the descriptor of the completed command
        uint8_t *cmd_desc = mmu_get_dma_mem(this->cmd_ptr, 16, &is_writable);

        // get command code
        this->cur_cmd = cmd_desc[3] >> 4;

        // all commands except STOP update cmd.xferStatus and
        // perform actions under control of "i" interrupt, "b" branch, and "w" wait bits
        if (this->cur_cmd < DBDMA_Cmd::STOP) {
            if (is_writable)
                WRITE_WORD_LE_A(&cmd_desc[14], this->ch_stat | CH_STAT_ACTIVE);

            // react to cmd.wait bits
            if (cmd_desc[2] & 3) {
                ABORT_F("%s: cmd.wait bits not implemented", this->get_name().c_str());
            }

            // react to cmd.branch bits
            if (cmd_desc[2] & 0xC) {
                bool cond = true;
                if ((cmd_desc[2] & 0xC) != 0xC) {
                    uint16_t br_mask = this->branch_select >> 16;
                    cond = (this->ch_stat & br_mask) == (this->branch_select & br_mask);
                    if ((cmd_desc[2] & 0xC) == 0x8) { // branch if cond cleared?
                        cond = !cond;
                    }
                }
                if (cond) {
                    this->cmd_ptr = READ_DWORD_LE_A(&cmd_desc[8]);
                    branch_taken = true;
                }
            }

            this->update_irq();
        }

        // all INPUT and OUTPUT commands update cmd.resCount
        if (this->cur_cmd < DBDMA_Cmd::STORE_QUAD && is_writable) {
            WRITE_WORD_LE_A(&cmd_desc[12], this->queue_len & 0xFFFFUL);
        }

        if (!branch_taken)
            this->cmd_ptr += 16;

        this->cmd_in_progress = false;
    }

    fetch_cmd(this->cmd_ptr, &cmd_struct);

    this->ch_stat &= ~CH_STAT_WAKE; // clear wake bit (DMA spec, 5.5.3.4)

    this->cur_cmd = cmd_struct.cmd_key >> 4;

    switch (this->cur_cmd) {
    case DBDMA_Cmd::OUTPUT_MORE:
    case DBDMA_Cmd::OUTPUT_LAST:
    case DBDMA_Cmd::INPUT_MORE:
    case DBDMA_Cmd::INPUT_LAST:
        if (cmd_struct.cmd_key & 7) {
            LOG_F(ERROR, "%s: Key > 0 not implemented", this->get_name().c_str());
            break;
        }
        this->queue_data = mmu_get_dma_mem(cmd_struct.address, cmd_struct.req_count, &is_writable);
        this->queue_len  = cmd_struct.req_count;
        this->cmd_in_progress = true;
        break;
    case DBDMA_Cmd::STORE_QUAD:
        LOG_F(ERROR, "%s: Unsupported DMA Command STORE_QUAD", this->get_name().c_str());
        break;
    case DBDMA_Cmd::LOAD_QUAD:
        LOG_F(ERROR, "%s: Unsupported DMA Command LOAD_QUAD", this->get_name().c_str());
        break;
    case DBDMA_Cmd::NOP:
        LOG_F(ERROR, "%s: Unsupported DMA Command NOP", this->get_name().c_str());
        break;
    case DBDMA_Cmd::STOP:
        this->ch_stat &= ~CH_STAT_ACTIVE;
        this->cmd_in_progress = false;
        break;
    default:
        LOG_F(ERROR, "%s: Unsupported DMA command 0x%X", this->get_name().c_str(), this->cur_cmd);
        this->ch_stat |= CH_STAT_DEAD;
        this->ch_stat &= ~CH_STAT_ACTIVE;
    }

    return this->cur_cmd;
}

void DMAChannel::update_irq() {
    bool   is_writable;

    // obtain real pointer to the descriptor of the completed command
    uint8_t *cmd_desc = mmu_get_dma_mem(this->cmd_ptr, 16, &is_writable);

    // STOP doesn't generate interrupts
    if (this->cur_cmd < DBDMA_Cmd::STOP) {
        // react to cmd.interrupt bits
        if (cmd_desc[2] & 0x30) {
            bool cond = true;
            if ((cmd_desc[2] & 0x30) != 0x30) {
                uint16_t int_mask = this->int_select >> 16;
                cond = (this->ch_stat & int_mask) == (this->int_select & int_mask);
                if ((cmd_desc[2] & 0x30) == 0x20) { // interrupt if cond cleared?
                    cond = !cond;
                }
            }
            if (cond) {
                this->int_ctrl->ack_dma_int(this->irq_id, 1);
            }
        }
    }
}

uint32_t DMAChannel::reg_read(uint32_t offset, int size) {
    uint32_t res = 0;

    if (size != 4) {
        ABORT_F("%s: non-DWORD read from a DMA channel not supported", this->get_name().c_str());
    }

    switch (offset) {
    case DMAReg::CH_CTRL:
        res = 0; // ChannelControl reads as 0 (DBDMA spec 5.5.1, table 74)
        break;
    case DMAReg::CH_STAT:
        res = BYTESWAP_32(this->ch_stat);
        break;
    default:
        LOG_F(WARNING, "%s: Unsupported DMA channel register read  @%02x.%c", this->get_name().c_str(), offset, SIZE_ARG(size));
    }

    return res;
}

void DMAChannel::reg_write(uint32_t offset, uint32_t value, int size) {
    uint16_t mask, old_stat, new_stat;

    if (size != 4) {
        ABORT_F("%s: non-DWORD writes to a DMA channel not supported", this->get_name().c_str());
    }

    value    = BYTESWAP_32(value);
    old_stat = this->ch_stat;

    switch (offset) {
    case DMAReg::CH_CTRL:
        mask     = value >> 16;
        new_stat = (value & mask & 0xF0FFU) | (old_stat & ~mask);
        LOG_F(9, "%s: New ChannelStatus value = 0x%X", this->get_name().c_str(), new_stat);

        // update ch_stat.s0...s7 if requested (needed for interrupt generation)
        if ((new_stat & 0xFF) != (old_stat & 0xFF)) {
            this->ch_stat |= new_stat & 0xFF;
        }

        // flush bit can be set at the same time the run bit is cleared.
        // That means we need to update memory before channel operation
        // is aborted to prevent data loss.
        if (new_stat & CH_STAT_FLUSH) {
            // NOTE: because this implementation doesn't currently support
            // partial memory updates no special action is taken here
            new_stat &= ~CH_STAT_FLUSH;
            this->ch_stat = new_stat;
        }

        if ((new_stat & CH_STAT_RUN) != (old_stat & CH_STAT_RUN)) {
            if (new_stat & CH_STAT_RUN) {
                new_stat |= CH_STAT_ACTIVE;
                this->ch_stat = new_stat;
                this->start();
            } else {
                this->abort();
                this->update_irq();
                new_stat &= ~CH_STAT_ACTIVE;
                new_stat &= ~CH_STAT_DEAD;
                this->cmd_in_progress = false;
                this->ch_stat = new_stat;
            }
        } else if ((new_stat & CH_STAT_WAKE) != (old_stat & CH_STAT_WAKE)) {
            new_stat |= CH_STAT_ACTIVE;
            this->ch_stat = new_stat;
            this->resume();
        } else if ((new_stat & CH_STAT_PAUSE) != (old_stat & CH_STAT_PAUSE)) {
            if (new_stat & CH_STAT_PAUSE) {
                new_stat &= ~CH_STAT_ACTIVE;
                this->ch_stat = new_stat;
                this->pause();
            }
        }
        break;
    case DMAReg::CH_STAT:
        break; // ingore writes to ChannelStatus
    case DMAReg::CMD_PTR_HI:
        if (value != 0) {
            LOG_F(WARNING, "%s: Unsupported DMA channel register write @%02x.%c = %0*x", this->get_name().c_str(), offset, SIZE_ARG(size), size * 2, value);
        }
        break;
    case DMAReg::CMD_PTR_LO:
        if (!(this->ch_stat & CH_STAT_RUN) && !(this->ch_stat & CH_STAT_ACTIVE)) {
            this->cmd_ptr = value;
            LOG_F(9, "%s: CommandPtrLo set to 0x%X", this->get_name().c_str(), this->cmd_ptr);
        }
        break;
    case DMAReg::INT_SELECT:
        this->int_select = value & 0xFF00FFUL;
        break;
    case DMAReg::BRANCH_SELECT:
        this->branch_select = value & 0xFF00FFUL;
        break;
    case DMAReg::WAIT_SELECT:
        this->wait_select = value & 0xFF00FFUL;
        break;
    default:
        LOG_F(WARNING, "%s: Unsupported DMA channel register write @%02x.%c = %0*x", this->get_name().c_str(), offset, SIZE_ARG(size), size * 2, value);
    }
}

DmaPullResult DMAChannel::pull_data(uint32_t req_len, uint32_t *avail_len, uint8_t **p_data)
{
    *avail_len = 0;

    if (this->ch_stat & CH_STAT_DEAD || !(this->ch_stat & CH_STAT_ACTIVE)) {
        // dead or idle channel? -> no more data
        LOG_F(WARNING, "%s: Dead/idle channel -> no more data", this->get_name().c_str());
        return DmaPullResult::NoMoreData;
    }

    // interpret DBDMA program until we get data or become idle
    while ((this->ch_stat & CH_STAT_ACTIVE) && !this->queue_len) {
        this->interpret_cmd();
    }

    // dequeue data if any
    if (this->queue_len) {
        if (this->queue_len >= req_len) {
            LOG_F(9, "%s: Return req_len = %d data", this->get_name().c_str(), req_len);
            *p_data    = this->queue_data;
            *avail_len = req_len;
            this->queue_len -= req_len;
            this->queue_data += req_len;
        } else { // return less data than req_len
            LOG_F(9, "%s: Return queue_len = %d data", this->get_name().c_str(), this->queue_len);
            *p_data         = this->queue_data;
            *avail_len      = this->queue_len;
            this->queue_len = 0;
        }
        return DmaPullResult::MoreData; // tell the caller there is more data
    }

    return DmaPullResult::NoMoreData; // tell the caller there is no more data
}

int DMAChannel::push_data(const char* src_ptr, int len) {
    if (this->ch_stat & CH_STAT_DEAD || !(this->ch_stat & CH_STAT_ACTIVE)) {
        LOG_F(WARNING, "%s: attempt to push data to dead/idle channel", this->get_name().c_str());
        return -1;
    }

    // interpret DBDMA program until we get buffer to fill in or become idle
    while ((this->ch_stat & CH_STAT_ACTIVE) && !this->queue_len) {
        this->interpret_cmd();
    }

    if (this->queue_len) {
        len = std::min((int)this->queue_len, len);
        std::memcpy(this->queue_data, src_ptr, len);
        this->queue_data += len;
        this->queue_len  -= len;
    }

    // proceed with the DBDMA program if the buffer became exhausted
    if (!this->queue_len) {
        this->interpret_cmd();
    }

    return 0;
}

bool DMAChannel::is_active() {
    if (this->ch_stat & CH_STAT_DEAD || !(this->ch_stat & CH_STAT_ACTIVE)) {
        return false;
    }
    else {
        return true;
    }
}

void DMAChannel::start() {
    if (this->ch_stat & CH_STAT_PAUSE) {
        LOG_F(WARNING, "%s: Cannot start DMA channel, PAUSE bit is set", this->get_name().c_str());
        return;
    }

    this->queue_len = 0;

    if (this->start_cb)
        this->start_cb();
}

void DMAChannel::resume() {
    if (this->ch_stat & CH_STAT_PAUSE) {
        LOG_F(WARNING, "%s: Cannot resume DMA channel, PAUSE bit is set", this->get_name().c_str());
        return;
    }

    LOG_F(INFO, "%s: Resuming DMA channel", this->get_name().c_str());
}

void DMAChannel::abort() {
    LOG_F(9, "%s: Aborting DMA channel", this->get_name().c_str());
    if (this->stop_cb)
        this->stop_cb();
}

void DMAChannel::pause() {
    LOG_F(INFO, "%s: Pausing DMA channel", this->get_name().c_str());
    if (this->stop_cb)
        this->stop_cb();
}
