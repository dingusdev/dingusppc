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

/** @file Base class for ATA devices. */

#ifndef ATA_BASE_DEVICE_H
#define ATA_BASE_DEVICE_H

#include <devices/common/ata/atadefs.h>
#include <devices/common/hwcomponent.h>
#include "endianswap.h"

#include <cinttypes>
#include <string>
#include <functional>

class IdeChannel;

class AtaBaseDevice : public HWComponent, public AtaInterface
{
public:
    AtaBaseDevice(const std::string name, uint8_t type);
    ~AtaBaseDevice() = default;

    void set_host(IdeChannel* host, uint8_t dev_id) {
        this->host_obj = host;
        this->my_dev_id = dev_id;
    };

    uint16_t read(const uint8_t reg_addr) override;
    void write(const uint8_t reg_addr, const uint16_t value) override;

    virtual int perform_command() = 0;

    int get_device_id() override { return this->my_dev_id; };

    void pdiag_callback() override {
        this->r_error &= 0x7F;
    };

    virtual void device_reset(bool is_soft_reset);
    virtual void device_set_signature();
    void device_control(const uint8_t new_ctrl);
    void update_intrq(uint8_t new_intrq_state);
    void signal_data_ready();

    bool has_data() {
        return data_ptr && xfer_cnt;
    }

    uint16_t get_data() {
        return BYTESWAP_16(*this->data_ptr++);
    }

protected:
    bool is_selected() { return ((this->r_dev_head >> 4) & 1) == this->my_dev_id; };

    void prepare_xfer(int xfer_size, int block_size);

    uint8_t my_dev_id = 0; // my IDE device ID configured by the host
    uint8_t device_type = ata_interface::DEVICE_TYPE_UNKNOWN;
    uint8_t intrq_state = 0; // INTRQ deasserted

    IdeChannel* host_obj = nullptr;

    // IDE aka task file registers
    uint8_t r_error;
    uint8_t r_features;
    uint8_t r_sect_count;
    uint8_t r_sect_num;
    uint8_t r_cylinder_lo;
    uint8_t r_cylinder_hi;
    uint8_t r_dev_head;
    uint8_t r_command;
    uint8_t r_status;
    uint8_t r_status_save;
    uint8_t r_dev_ctrl = 0x08;

    uint16_t    *data_ptr       = nullptr;
    uint16_t    *cur_data_ptr   = nullptr;
    uint8_t     data_buf[512]   = {};
    int         xfer_cnt        = 0;
    int         chunk_cnt       = 0;
    int         chunk_size      = 0;

    std::function<void()> post_xfer_action = nullptr;
};

#endif // ATA_BASE_DEVICE_H
