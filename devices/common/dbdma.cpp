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
#include <devices/common/hwinterrupt.h>
#include <devices/common/mmiodevice.h>
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
DMACmd* DMAChannel::fetch_cmd(uint32_t cmd_addr, DMACmd* p_cmd, bool *is_writable) {
    MapDmaResult res = mmu_map_dma_mem(cmd_addr, 16, false);
    if (is_writable) *is_writable = res.is_writable;
    DMACmd* cmd_host = (DMACmd*)res.host_va;
    p_cmd->req_count = READ_WORD_LE_A(&cmd_host->req_count);
    p_cmd->cmd_bits  = cmd_host->cmd_bits;
    p_cmd->cmd_key   = cmd_host->cmd_key;
    p_cmd->address   = READ_DWORD_LE_A(&cmd_host->address);
    p_cmd->cmd_arg   = READ_DWORD_LE_A(&cmd_host->cmd_arg);
    p_cmd->res_count = READ_WORD_LE_A(&cmd_host->res_count);
    p_cmd->xfer_stat = READ_WORD_LE_A(&cmd_host->xfer_stat);
    return cmd_host;
}

uint8_t DMAChannel::interpret_cmd() {
    DMACmd cmd_struct;
    MapDmaResult res;

    if (this->cmd_in_progress) {
        // return current command if there is data to transfer
        if (this->queue_len)
            return this->cur_cmd;

        this->finish_cmd();
    }

    bool cmd_is_writable;
    DMACmd *cmd_host = fetch_cmd(this->cmd_ptr, &cmd_struct, &cmd_is_writable);

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
        res = mmu_map_dma_mem(cmd_struct.address, cmd_struct.req_count, false);
        this->queue_data = res.host_va;
        this->queue_len  = cmd_struct.req_count;
        this->res_count  = 0;
        this->cmd_in_progress = true;
        break;
    case DBDMA_Cmd::STORE_QUAD:
        if ((cmd_struct.cmd_key & 7) != 6)
            LOG_F(ERROR, "%s: Invalid key %d in STORE_QUAD", this->get_name().c_str(),
                cmd_struct.cmd_key & 7);
        this->xfer_quad(&cmd_struct, nullptr);
        break;
    case DBDMA_Cmd::LOAD_QUAD:
        if ((cmd_struct.cmd_key & 7) != 6) {
            LOG_F(ERROR, "%s: Invalid key %d in LOAD_QUAD", this->get_name().c_str(),
                cmd_struct.cmd_key & 7);
        }
        if (!cmd_is_writable)
            LOG_F(ERROR, "%s: DMACmd is not writeable!", this->get_name().c_str());
        this->xfer_quad(&cmd_struct, cmd_host);
        break;
    case DBDMA_Cmd::NOP:
        this->finish_cmd();
        break;
    case DBDMA_Cmd::STOP:
        this->ch_stat &= ~CH_STAT_ACTIVE;
        this->cmd_in_progress = false;
        break;
    default:
        LOG_F(ERROR, "%s: Unsupported DMA command 0x%X", this->get_name().c_str(),
            this->cur_cmd);
        this->ch_stat |= CH_STAT_DEAD;
        this->ch_stat &= ~CH_STAT_ACTIVE;
    }

    return this->cur_cmd;
}

void DMAChannel::finish_cmd() {
    bool   branch_taken = false;

    // obtain real pointer to the descriptor of the command to be finished
    MapDmaResult res  = mmu_map_dma_mem(this->cmd_ptr, 16, false);
    uint8_t *cmd_desc = res.host_va;

    // get command code
    this->cur_cmd = cmd_desc[3] >> 4;

    // all commands except STOP update cmd.xferStatus and
    // perform actions under control of "i" interrupt, "b" branch, and "w" wait bits
    if (this->cur_cmd < DBDMA_Cmd::STOP) {
        // react to cmd.w (wait) bits
        if (cmd_desc[2] & 3) {
            bool cond = true;
            if ((cmd_desc[2] & 3) != 3) {
                uint16_t wt_mask = this->wait_select >> 16;
                cond = (this->ch_stat & wt_mask) == (this->wait_select & wt_mask);
                if ((cmd_desc[2] & 3) == 2) {
                    cond = !cond; // wait if cond = false
                }
            }

            if (cond)
                return;
        }

        if (res.is_writable)
            WRITE_WORD_LE_A(&cmd_desc[14], this->ch_stat | CH_STAT_ACTIVE);

        // react to cmd.b (branch) bits
        if (cmd_desc[2] & 0xC) {
            bool cond = true;
            if ((cmd_desc[2] & 0xC) != 0xC) {
                uint16_t br_mask = this->branch_select >> 16;
                cond = (this->ch_stat & br_mask) == (this->branch_select & br_mask);
                if ((cmd_desc[2] & 0xC) == 0x8) {
                    cond = !cond; // branch if cond = false
                }
            }
            if (cond) {
                this->cmd_ptr = READ_DWORD_LE_A(&cmd_desc[8]);
                branch_taken = true;
            }
        }

        this->update_irq();
    }

    // all INPUT and OUTPUT commands including LOAD_QUAD and STORE_QUAD update cmd.resCount
    if (this->cur_cmd < DBDMA_Cmd::NOP && res.is_writable) {
        WRITE_WORD_LE_A(&cmd_desc[12], this->res_count);
        this->queue_len = 0;
        this->res_count = 0;
    }

    if (!branch_taken)
        this->cmd_ptr += 16;

    this->cmd_in_progress = false;
}

void DMAChannel::xfer_quad(const DMACmd *cmd_desc, DMACmd *cmd_host) {
    MapDmaResult res;
    uint32_t addr;

    // parse and fix reqCount
    uint32_t xfer_size = cmd_desc->req_count & 7;
    if (xfer_size & 4) {
        xfer_size = 4;
    } else if (xfer_size & 2) {
        xfer_size = 2;
    } else {
        xfer_size = 1;
    }
    this->res_count = cmd_desc->req_count; // this is the value that gets written to cmd.resCount

    addr = cmd_desc->address;
    if (addr & (xfer_size - 1)) {
        LOG_F(ERROR, "%s: QUAD address 0x%08x is not aligned!", this->get_name().c_str(), addr);
        addr &= ~(xfer_size - 1);
    }

    res = mmu_map_dma_mem(addr, xfer_size, true);

    // prepare data pointers and perform data transfer
    if (!cmd_host) {
        if (res.type & RT_MMIO) {
            res.dev_obj->write(res.dev_base, addr - res.dev_base, cmd_desc->cmd_arg, xfer_size);
        } else if (res.is_writable) {
            switch (xfer_size) {
                case 1: *res.host_va = cmd_desc->cmd_arg; break;
                case 2: WRITE_WORD_LE_A(res.host_va, cmd_desc->cmd_arg); break;
                case 4: WRITE_DWORD_LE_A(res.host_va, cmd_desc->cmd_arg); break;
            }
        } else {
            LOG_F(ERROR, "SOS: DMA access is not to RAM %08X!\n", addr);
        }
    } else {
        uint32_t value;
        if (res.type & RT_MMIO) {
            value = res.dev_obj->read(res.dev_base, addr - res.dev_base, xfer_size);
        } else {
            switch (xfer_size) {
                case 1: value = *res.host_va; break;
                case 2: value = READ_WORD_LE_A(res.host_va); break;
                case 4: value = READ_DWORD_LE_A(res.host_va); break;
                default: value = 0; break;
            }
        }
        WRITE_DWORD_LE_A(&cmd_host->cmd_arg, value);
    }

    if (cmd_desc->cmd_bits & 0xC)
        ABORT_F("%s: cmd_bits.b should be zero for LOAD/STORE_QUAD!",
            this->get_name().c_str());

    this->finish_cmd();
}

void DMAChannel::update_irq() {
    // obtain real pointer to the descriptor of the completed command
    MapDmaResult res = mmu_map_dma_mem(this->cmd_ptr, 16, false);
    uint8_t *cmd_desc = res.host_va;

    // STOP doesn't generate interrupts
    if (this->cur_cmd < DBDMA_Cmd::STOP) {
        // react to cmd.i (interrupt) bits
        if (cmd_desc[2] & 0x30) {
            bool cond = true;
            if ((cmd_desc[2] & 0x30) != 0x30) {
                uint16_t int_mask = this->int_select >> 16;
                cond = (this->ch_stat & int_mask) == (this->int_select & int_mask);
                if ((cmd_desc[2] & 0x30) == 0x20) {
                    cond = !cond; // generate interrupt if cond = false
                }
            }
            if (cond) {
                if (int_ctrl)
                    this->int_ctrl->ack_dma_int(this->irq_id, 1);
                else
                    LOG_F(ERROR, "%s Interrupt ignored", this->get_name().c_str());
            }
        }
    }
}

uint32_t DMAChannel::reg_read(uint32_t offset, int size) {
    if (size != 4) {
        ABORT_F("%s: non-DWORD read from a DMA channel not supported",
            this->get_name().c_str());
    }

    switch (offset) {
    case DMAReg::CH_CTRL:
        return 0; // ChannelControl reads as 0 (DBDMA spec 5.5.1, table 74)
    case DMAReg::CH_STAT:
        return BYTESWAP_32(this->ch_stat);
    case DMAReg::CMD_PTR_LO:
        return BYTESWAP_32(this->cmd_ptr);
    default:
        LOG_F(WARNING, "%s: Unsupported DMA channel register read at 0x%X",
            this->get_name().c_str(), offset);
    }

    return 0;
}

void DMAChannel::reg_write(uint32_t offset, uint32_t value, int size) {
    uint16_t mask, old_stat, new_stat;

    if (size != 4) {
        ABORT_F("%s: non-DWORD writes to a DMA channel not supported",
            this->get_name().c_str());
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
            LOG_F(9, "%s: CommandPtrLo set to 0x%X", this->get_name().c_str(),
                this->cmd_ptr);
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
        LOG_F(WARNING, "%s: Unsupported DMA channel register write at 0x%X",
            this->get_name().c_str(), offset);
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
            this->res_count += req_len;
            this->queue_data += req_len;
        } else { // return less data than req_len
            LOG_F(9, "%s: Return queue_len = %d data", this->get_name().c_str(),
                this->queue_len);
            *p_data         = this->queue_data;
            *avail_len      = this->queue_len;
            this->res_count += this->queue_len;
            this->queue_len = 0;
        }
        return DmaPullResult::MoreData; // tell the caller there is more data
    }

    return DmaPullResult::NoMoreData; // tell the caller there is no more data
}

int DMAChannel::push_data(const char* src_ptr, int len) {
    if (this->ch_stat & CH_STAT_DEAD || !(this->ch_stat & CH_STAT_ACTIVE)) {
        LOG_F(WARNING, "%s: attempt to push data to dead/idle channel",
            this->get_name().c_str());
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
        this->res_count  += len;
        this->queue_len  -= len;
    }

    // proceed with the DBDMA program if the buffer became exhausted
    if (!this->queue_len) {
        this->interpret_cmd();
    }

    return 0;
}

bool DMAChannel::is_out_active() {
    if (this->ch_stat & CH_STAT_DEAD || !(this->ch_stat & CH_STAT_ACTIVE)) {
        return false;
    }
    else {
        return true;
    }
}

bool DMAChannel::is_in_active() {
    if (this->ch_stat & CH_STAT_DEAD || !(this->ch_stat & CH_STAT_ACTIVE)) {
        return false;
    }
    else {
        return true;
    }
}

void DMAChannel::start() {
    if (this->ch_stat & CH_STAT_PAUSE) {
        LOG_F(WARNING, "%s: Cannot start DMA channel, PAUSE bit is set",
            this->get_name().c_str());
        return;
    }

    this->queue_len = 0;

    this->cmd_in_progress = false;

    if (this->start_cb)
        this->start_cb();

    // some DBDMA programs contain commands that don't transfer data
    // between a device and memory (LOAD_QUAD, STORE_QUAD, NOP and STOP).
    // We thus interprete the DBDMA program until a data transfer between
    // a device and memory is queued or the channel becomes idle/dead.
    while (!this->cmd_in_progress && !(this->ch_stat & CH_STAT_DEAD) &&
           (this->ch_stat & CH_STAT_ACTIVE)) {
               this->interpret_cmd();
    }
}

void DMAChannel::resume() {
    if (this->ch_stat & CH_STAT_PAUSE) {
        LOG_F(WARNING, "%s: Cannot resume DMA channel, PAUSE bit is set",
            this->get_name().c_str());
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
