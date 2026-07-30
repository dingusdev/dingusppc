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

/** @file Base class for ATAPI devices. */

#ifndef ATAPI_BASE_DEVICE_H
#define ATAPI_BASE_DEVICE_H

#include <devices/common/ata/atabasedevice.h>
#include <devices/common/scsi/scsi.h>
#include <devices/common/scsi/scsiphysinterface.h>

#include <cinttypes>
#include <string>

class AtapiBaseDevice : public AtaBaseDevice, public ScsiPhysInterface
{
public:
    AtapiBaseDevice(const std::string name);
    ~AtapiBaseDevice() = default;

    void device_set_signature() override;

    uint16_t read(const uint8_t reg_addr) override;
    void write(const uint8_t reg_addr, const uint16_t value) override;

    // ScsiPhysInterface methods
    void set_eject_state(bool eject_allowed) override {};

    void set_xfer_len(uint64_t len) override {
        this->xfer_cnt      = len;
        this->r_byte_count  = len;
    }

    void set_buffer(uint8_t *buf_ptr) override {
        this->data_ptr = (uint16_t*)buf_ptr;
    }

    void set_status(uint8_t status_code, uint8_t sense_key) override;
    void switch_phase(const int new_phase) override;
    void set_read_more_data_cb(more_data_cb_t cb) override {};
    void set_write_more_data_cb(more_data_cb_t cb) override {};
    void set_post_xfer_action(action_callback cb) override {};

    // methods to be implemented in the particular device
    int  perform_command() override;
    virtual void perform_packet_command() = 0;
    virtual int get_config(uint8_t* pkt, uint8_t* buf) = 0;
    virtual int request_data() = 0;
    virtual bool data_available() = 0;

    // methods with default implementation
    virtual void data_in_phase();
    virtual void present_status();

protected:
    uint8_t     r_int_reason;
    uint16_t    r_byte_count;
    bool        status_expected = false;

    alignas(uint16_t)
    uint8_t     cmd_pkt[12] = {};
};

#endif // ATAPI_BASE_DEVICE_H
