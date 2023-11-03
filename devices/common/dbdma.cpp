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
void DMAChannel::fetch_cmd(uint32_t cmd_addr, DMACmd* p_cmd) {
    MapDmaResult res = mmu_map_dma_mem(cmd_addr, 16, false);
    memcpy((uint8_t*)p_cmd, res.host_va, 16);
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

    fetch_cmd(this->cmd_ptr, &cmd_struct);

    this->ch_stat &= ~CH_STAT_WAKE; // clear wake bit (DMA spec, 5.5.3.4)

    this->cur_cmd = cmd_struct.cmd_key >> 4;

    switch (this->cur_cmd) {
    case DBDMA_Cmd::OUTPUT_MORE:
    case DBDMA_Cmd::OUTPUT_LAST:
    case DBDMA_Cmd::INPUT_MORE:
    case DBDMA_Cmd::INPUT_LAST:
        if (cmd_struct.cmd_key & 7) {
            LOG_F(ERROR, "Key > 0 not implemented");
            break;
        }
        res = mmu_map_dma_mem(cmd_struct.address, cmd_struct.req_count, false);
        this->queue_data = res.host_va;
        this->queue_len  = cmd_struct.req_count;
        this->cmd_in_progress = true;
        break;
    case DBDMA_Cmd::STORE_QUAD:
        if ((cmd_struct.cmd_key & 7) != 6)
            LOG_F(9, "Invalid key %d in STORE_QUAD", cmd_struct.cmd_key & 7);
        this->xfer_quad(&cmd_struct, true);
        break;
    case DBDMA_Cmd::LOAD_QUAD:
        if ((cmd_struct.cmd_key & 7) != 6)
            LOG_F(9, "Invalid key %d in LOAD_QUAD", cmd_struct.cmd_key & 7);
        this->xfer_quad(&cmd_struct, false);
        break;
    case DBDMA_Cmd::NOP:
        this->finish_cmd();
        break;
    case DBDMA_Cmd::STOP:
        this->ch_stat &= ~CH_STAT_ACTIVE;
        this->cmd_in_progress = false;
        break;
    default:
        LOG_F(ERROR, "Unsupported DMA command 0x%X", this->cur_cmd);
        this->ch_stat |= CH_STAT_DEAD;
        this->ch_stat &= ~CH_STAT_ACTIVE;
    }

    return this->cur_cmd;
}

void DMAChannel::finish_cmd() {
    DMACmd cmd_struct;
    bool   branch_taken = false;

    // obtain real pointer to the descriptor of the command to be finished
    MapDmaResult res  = mmu_map_dma_mem(this->cmd_ptr, 16, false);
    uint8_t *cmd_desc = res.host_va;

    // get command code
    this->cur_cmd = cmd_desc[3] >> 4;

    // all commands except STOP update cmd.xferStatus and
    // perform actions under control of "i", "b" and "w" bits
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

    // all INPUT and OUTPUT commands update cmd.resCount
    if (this->cur_cmd < DBDMA_Cmd::STORE_QUAD && res.is_writable) {
        WRITE_WORD_LE_A(&cmd_desc[12], this->queue_len & 0xFFFFUL);
    }

    if (!branch_taken)
        this->cmd_ptr += 16;

    this->cmd_in_progress = false;
}

void DMAChannel::xfer_quad(const DMACmd *cmd_desc, const bool is_store) {
    MapDmaResult res;
    uint8_t *src, *dst;
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

    addr = cmd_desc->address & ~(xfer_size - 1);

    // prepare data pointers and perform data transfer
    if (is_store) {
        res = mmu_map_dma_mem(this->cmd_ptr, 16, false);
        src = res.host_va + 8; // move src to cmd.data32
        res = mmu_map_dma_mem(addr, xfer_size, true);
        if (res.type & RT_MMIO) {
            res.dev_obj->write(res.dev_base, addr - res.dev_base,
                read_mem_rev(src, xfer_size), xfer_size);
        } else if (res.is_writable) {
            std::memcpy(res.host_va, src, xfer_size);
        }
    } else {
        res = mmu_map_dma_mem(this->cmd_ptr, 16, false);
        if (res.is_writable) {
            dst = res.host_va + 8; // move dst to cmd.data32

            res = mmu_map_dma_mem(addr, xfer_size, true);
            if (res.type & RT_MMIO) {
                write_mem_rev(dst,
                    res.dev_obj->read(res.dev_base, addr - res.dev_base, xfer_size),
                    xfer_size);
            } else {
                std::memcpy(dst, res.host_va, xfer_size);
            }
        }
    }

    if (cmd_desc->cmd_bits & 0xC)
        ABORT_F("DBDMA: cmd_bits.b should be zero for LOAD/STORE_QUAD!");

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
                this->int_ctrl->ack_dma_int(this->irq_id, 1);
            }
        }
    }
}

uint32_t DMAChannel::reg_read(uint32_t offset, int size) {
    if (size != 4) {
        ABORT_F("DBDMA: non-DWORD read from a DMA channel not supported");
    }

    switch (offset) {
    case DMAReg::CH_CTRL:
        return 0; // ChannelControl reads as 0 (DBDMA spec 5.5.1, table 74)
    case DMAReg::CH_STAT:
        return BYTESWAP_32(this->ch_stat);
    case DMAReg::CMD_PTR_LO:
        return BYTESWAP_32(this->cmd_ptr);
    default:
        LOG_F(WARNING, "Unsupported DMA channel register 0x%X", offset);
    }

    return 0;
}

void DMAChannel::reg_write(uint32_t offset, uint32_t value, int size) {
    uint16_t mask, old_stat, new_stat;

    if (size != 4) {
        ABORT_F("DBDMA: non-DWORD writes to a DMA channel not supported");
    }

    value    = BYTESWAP_32(value);
    old_stat = this->ch_stat;

    switch (offset) {
    case DMAReg::CH_CTRL:
        mask     = value >> 16;
        new_stat = (value & mask & 0xF0FFU) | (old_stat & ~mask);
        LOG_F(9, "New ChannelStatus value = 0x%X", new_stat);

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
    case DMAReg::CMD_PTR_HI: // Mac OS X writes this optional register with zero
        LOG_F(9, "CommandPtrHi set to 0x%X", value);
        break;
    case DMAReg::CMD_PTR_LO:
        if (!(this->ch_stat & CH_STAT_RUN) && !(this->ch_stat & CH_STAT_ACTIVE)) {
            this->cmd_ptr = value;
            LOG_F(9, "CommandPtrLo set to 0x%X", this->cmd_ptr);
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
        LOG_F(WARNING, "Unsupported DMA channel register 0x%X", offset);
    }
}

DmaPullResult DMAChannel::pull_data(uint32_t req_len, uint32_t *avail_len, uint8_t **p_data)
{
    *avail_len = 0;

    if (this->ch_stat & CH_STAT_DEAD || !(this->ch_stat & CH_STAT_ACTIVE)) {
        // dead or idle channel? -> no more data
        LOG_F(WARNING, "Dead/idle channel -> no more data");
        return DmaPullResult::NoMoreData;
    }

    // interpret DBDMA program until we get data or become idle
    while ((this->ch_stat & CH_STAT_ACTIVE) && !this->queue_len) {
        this->interpret_cmd();
    }

    // dequeue data if any
    if (this->queue_len) {
        if (this->queue_len >= req_len) {
            LOG_F(9, "Return req_len = %d data", req_len);
            *p_data    = this->queue_data;
            *avail_len = req_len;
            this->queue_len -= req_len;
            this->queue_data += req_len;
        } else { // return less data than req_len
            LOG_F(9, "Return queue_len = %d data", this->queue_len);
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
        LOG_F(WARNING, "DBDMA: attempt to push data to dead/idle channel");
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
        LOG_F(WARNING, "Cannot start DMA channel, PAUSE bit is set");
        return;
    }

    this->queue_len = 0;

    if (this->start_cb)
        this->start_cb();

    this->cmd_in_progress = false;

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
        LOG_F(WARNING, "Cannot resume DMA channel, PAUSE bit is set");
        return;
    }

    LOG_F(INFO, "Resuming DMA channel");
}

void DMAChannel::abort() {
    LOG_F(9, "Aborting DMA channel");
    if (this->stop_cb)
        this->stop_cb();
}

void DMAChannel::pause() {
    LOG_F(INFO, "Pausing DMA channel");
    if (this->stop_cb)
        this->stop_cb();
}
