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

/** @file Descriptor-based direct memory access emulation.

    Official documentation can be found in the fifth chapter of the book
    "Macintosh Technology in the Common Hardware Reference Platform"
    by Apple Computer, Inc.
 */

#ifndef DB_DMA_H
#define DB_DMA_H

#include <cinttypes>

/** DBDMA Channel registers offsets */
enum DMAReg : uint32_t {
    CH_CTRL    = 0,
    CH_STAT    = 4,
    CMD_PTR_LO = 12,
};

/** Channel Status bits (DBDMA spec, 5.5.3) */
enum {
    CH_STAT_ACTIVE = 0x400,
    CH_STAT_DEAD   = 0x800,
    CH_STAT_WAKE   = 0x1000,
    CH_STAT_FLUSH  = 0x2000,
    CH_STAT_PAUSE  = 0x4000,
    CH_STAT_RUN    = 0x8000
};

/** DBDMA command (DBDMA spec, 5.6.1) - all fields are little-endian! */
typedef struct DMACmd {
    uint16_t req_count;
    uint8_t cmd_bits;
    uint8_t cmd_key;
    uint32_t address;
    uint32_t cmd_arg;
    uint16_t res_count;
    uint16_t xfer_stat;
} DMACmd;

class DMACallback {
public:
    virtual void dma_start(void)                  = 0;
    virtual void dma_end(void)                    = 0;
    virtual void dma_push(uint8_t* buf, int size) = 0;
    virtual void dma_pull(uint8_t* buf, int size) = 0;
};

class DMAChannel {
public:
    DMAChannel(DMACallback* cb) {
        this->dma_cb = cb;
    };
    ~DMAChannel() = default;

    uint32_t reg_read(uint32_t offset, int size);
    void reg_write(uint32_t offset, uint32_t value, int size);

    int get_data(uint32_t req_len, uint32_t* avail_len, uint8_t** p_data);

protected:
    void get_next_cmd(uint32_t cmd_addr, DMACmd* p_cmd);
    uint8_t interpret_cmd(void);

    void start(void);
    void resume(void);
    void abort(void);
    void pause(void);

private:
    DMACallback* dma_cb = 0;
    uint16_t ch_stat    = 0;
    uint32_t cmd_ptr    = 0;

    uint32_t queue_len;
    uint8_t* queue_data;
};

#endif /* DB_DMA_H */
