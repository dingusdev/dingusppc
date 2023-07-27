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

/** @file Descriptor-based direct memory access emulation.

    Official documentation can be found in the fifth chapter of the book
    "Macintosh Technology in the Common Hardware Reference Platform"
    by Apple Computer, Inc.
 */

#ifndef DB_DMA_H
#define DB_DMA_H

#include <devices/common/dmacore.h>
#include <devices/common/hwinterrupt.h>

#include <string>
#include <functional>

/** DBDMA Channel registers offsets */
enum DMAReg : uint32_t {
    CH_CTRL         = 0,
    CH_STAT         = 4,
    CMD_PTR_HI      = 8, // not implemented
    CMD_PTR_LO      = 12,
    INT_SELECT      = 16,
    BRANCH_SELECT   = 20,
    WAIT_SELECT     = 24,
//  TANSFER_MODES   = 28,
//  DATA_2_PTR_HI   = 32, // not implemented
//  DATA_2_PTR_LO   = 36,
//  RESERVED_1      = 40,
//  ADDRESS_HI      = 44,
//  RESERVED_2_0    = 48,
//  RESERVED_2_1    = 52,
//  RESERVED_2_2    = 56,
//  RESERVED_2_3    = 60,
//  UNIMPLEMENTED   = 64,
//  UNDEFINED       = 128,
};

/** Channel Status bits (DBDMA spec, 5.5.3) */
enum : uint16_t {
//  CH_STAT_S0     = 0x0001
//  CH_STAT_S1     = 0x0002,
//  CH_STAT_S2     = 0x0004,
//  CH_STAT_S3     = 0x0008,
//  CH_STAT_S4     = 0x0010,
//  CH_STAT_S5     = 0x0020,
//  CH_STAT_S6     = 0x0040,
//  CH_STAT_S7     = 0x0080,
//  CH_STAT_BT     = 0x0100,
    CH_STAT_ACTIVE = 0x0400,
    CH_STAT_DEAD   = 0x0800,
    CH_STAT_WAKE   = 0x1000,
    CH_STAT_FLUSH  = 0x2000,
    CH_STAT_PAUSE  = 0x4000,
    CH_STAT_RUN    = 0x8000
};

/** DBDMA command (DBDMA spec, 5.6.1) - all fields are little-endian! */
typedef struct DMACmd {
    uint16_t    req_count;
    uint8_t     cmd_bits; // wait: & 3, branch: & 0xC, interrupt: & 0x30, reserved: & 0xc0
    uint8_t     cmd_key; // key: & 7, reserved: & 8, cmd: >> 4
    uint32_t    address;
    uint32_t    cmd_arg;
    uint16_t    res_count;
    uint16_t    xfer_stat;
} DMACmd;

namespace DBDMA_Cmd {
    enum : uint8_t {
        OUTPUT_MORE = 0,
        OUTPUT_LAST = 1,
        INPUT_MORE  = 2,
        INPUT_LAST  = 3,
        STORE_QUAD  = 4,
        LOAD_QUAD   = 5,
        NOP         = 6,
        STOP        = 7
    };
};

typedef std::function<void(void)> DbdmaCallback;

class DMAChannel : public DmaBidirChannel {
public:
    DMAChannel(std::string name) : DmaBidirChannel(name) {}
    ~DMAChannel() = default;

    void set_callbacks(DbdmaCallback start_cb, DbdmaCallback stop_cb);
    uint32_t reg_read(uint32_t offset, int size);
    void reg_write(uint32_t offset, uint32_t value, int size);

    bool            is_active();
    DmaPullResult   pull_data(uint32_t req_len, uint32_t *avail_len, uint8_t **p_data);
    int             push_data(const char* src_ptr, int len);

    void register_dma_int(InterruptCtrl* int_ctrl_obj, uint32_t irq_id) {
        this->int_ctrl = int_ctrl_obj;
        this->irq_id   = irq_id;
    };

protected:
    void fetch_cmd(uint32_t cmd_addr, DMACmd* p_cmd);
    uint8_t interpret_cmd(void);
    void update_irq();

    void start(void);
    void resume(void);
    void abort(void);
    void pause(void);

private:
    std::function<void(void)> start_cb = nullptr; // DMA channel start callback
    std::function<void(void)> stop_cb  = nullptr; // DMA channel stop callback

    uint16_t ch_stat        = 0;
    uint32_t cmd_ptr        = 0;
    uint32_t queue_len      = 0;
    uint8_t* queue_data     = 0;
    uint32_t int_select     = 0;
    uint32_t branch_select  = 0;
    uint32_t wait_select    = 0;

    bool     cmd_in_progress = false;
    uint8_t  cur_cmd;

    // Interrupt related stuff
    InterruptCtrl* int_ctrl = nullptr;
    uint32_t       irq_id   = 0;
};

#endif /* DB_DMA_H */
