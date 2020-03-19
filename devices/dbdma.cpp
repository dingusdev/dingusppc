/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-20 divingkatae and maximum
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

#include <cinttypes>
#include <thirdparty/loguru.hpp>
#include "dbdma.h"
#include "endianswap.h"
#include "cpu/ppc/ppcmmu.h"

void DMAChannel::get_next_cmd(uint32_t cmd_addr, DMACmd *p_cmd)
{
    /* load DMACmd from physical memory */
    memcpy((uint8_t *)p_cmd, mmu_get_dma_mem(cmd_addr, 16), 16);
}

uint8_t DMAChannel::interpret_cmd()
{
    DMACmd cmd_struct;

    get_next_cmd(this->cmd_ptr, &cmd_struct);

    this->ch_stat &= ~CH_STAT_WAKE; /* clear wake bit (DMA spec, 5.5.3.4) */

    switch(cmd_struct.cmd_key >> 4) {
    case 0:
        LOG_F(INFO, "Executing DMA Command OUTPUT_MORE");
        if (cmd_struct.cmd_key & 7) {
            LOG_F(ERROR, "Key > 0 not implemented");
            break;
        }
        if (cmd_struct.cmd_bits & 0x3F) {
            LOG_F(ERROR, "non-zero i/b/w not implemented");
            break;
        }
        this->dma_cb->dma_push(
            mmu_get_dma_mem(cmd_struct.address, cmd_struct.req_count),
            cmd_struct.req_count);
        this->cmd_ptr += 16;
        break;
    case 1:
        LOG_F(INFO, "Executing DMA Command OUTPUT_LAST");
        if (cmd_struct.cmd_key & 7) {
            LOG_F(ERROR, "Key > 0 not implemented");
            break;
        }
        if (cmd_struct.cmd_bits & 0x3F) {
            LOG_F(ERROR, "non-zero i/b/w not implemented");
            break;
        }
        this->dma_cb->dma_push(
            mmu_get_dma_mem(cmd_struct.address, cmd_struct.req_count),
            cmd_struct.req_count);
        this->cmd_ptr += 16;
        break;
    case 2:
        LOG_F(ERROR, "Unsupported DMA Command INPUT_MORE");
        break;
    case 3:
        LOG_F(ERROR, "Unsupported DMA Command INPUT_LAST");
        break;
    case 4:
        LOG_F(ERROR, "Unsupported DMA Command STORE_QUAD");
        break;
    case 5:
        LOG_F(ERROR, "Unsupported DMA Command LOAD_QUAD");
        break;
    case 6:
        LOG_F(INFO, "Unsupported DMA Command NOP");
        break;
    case 7:
        LOG_F(INFO, "DMA Command: 7 (STOP)");
        this->ch_stat &= ~CH_STAT_ACTIVE;
        break;
    default:
        LOG_F(ERROR, "Unsupported DMA command 0x%X", cmd_struct.cmd_key >> 4);
        this->ch_stat |= CH_STAT_DEAD;
        this->ch_stat &= ~CH_STAT_ACTIVE;
    }

    return (cmd_struct.cmd_key >> 4);
}


uint32_t DMAChannel::reg_read(uint32_t offset, int size)
{
    uint32_t res = 0;

    if (size != 4) {
        LOG_F(WARNING, "Unsupported non-DWORD read from DMA channel");
        return 0;
    }

    switch(offset) {
    case DMAReg::CH_CTRL:
        res = 0; /* ChannelControl reads as 0 (DBDMA spec 5.5.1, table 74) */
        break;
    case DMAReg::CH_STAT:
        res = BYTESWAP_32(this->ch_stat);
        break;
    default:
        LOG_F(WARNING, "Unsupported DMA channel register 0x%X", offset);
    }

    return res;
}

void DMAChannel::reg_write(uint32_t offset, uint32_t value, int size)
{
    uint16_t mask, old_stat, new_stat;

    if (size != 4) {
        LOG_F(WARNING, "Unsupported non-DWORD write to DMA channel");
        return;
    }

    value = BYTESWAP_32(value);
    old_stat = this->ch_stat;

    switch(offset) {
    case DMAReg::CH_CTRL:
        mask = value >> 16;
        new_stat = (value & mask & 0xF0FFU) | (old_stat & ~mask);
        LOG_F(INFO, "New ChannelStatus value = 0x%X", new_stat);

        if ((new_stat & CH_STAT_RUN) != (old_stat & CH_STAT_RUN)) {
            if (new_stat & CH_STAT_RUN) {
                new_stat |= CH_STAT_ACTIVE;
                this->ch_stat = new_stat;
                this->start();
            } else {
                new_stat &= ~CH_STAT_ACTIVE;
                new_stat &= ~CH_STAT_DEAD;
                this->ch_stat = new_stat;
                this->abort();
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
        if (new_stat & CH_STAT_FLUSH) {
            LOG_F(WARNING, "DMA flush not implemented!");
            new_stat &= ~CH_STAT_FLUSH;
            this->ch_stat = new_stat;
        }
        break;
    case DMAReg::CH_STAT:
        break; /* ingore writes to ChannelStatus */
    case DMAReg::CMD_PTR_LO:
        if (!(this->ch_stat & CH_STAT_RUN) && !(this->ch_stat & CH_STAT_ACTIVE)) {
            this->cmd_ptr = value;
            LOG_F(INFO, "CommandPtrLo set to 0x%X", this->cmd_ptr);
        }
        break;
    default:
        LOG_F(WARNING, "Unsupported DMA channel register 0x%X", offset);
    }
}

void DMAChannel::start()
{
    if (this->ch_stat & CH_STAT_PAUSE) {
        LOG_F(WARNING, "Cannot start DMA channel, PAUSE bit is set");
        return;
    }

    LOG_F(INFO, "Starting DMA channel, stat = 0x%X", this->ch_stat);

    this->dma_cb->dma_start();

    while (this->interpret_cmd() != 7) {
    }
}

void DMAChannel::resume()
{
    if (this->ch_stat & CH_STAT_PAUSE) {
        LOG_F(WARNING, "Cannot resume DMA channel, PAUSE bit is set");
        return;
    }

    LOG_F(INFO, "Resuming DMA channel");
}

void DMAChannel::abort()
{
    LOG_F(INFO, "Aborting DMA channel");
}

void DMAChannel::pause()
{
    LOG_F(INFO, "Pausing DMA channel");
    this->dma_cb->dma_end();
}
