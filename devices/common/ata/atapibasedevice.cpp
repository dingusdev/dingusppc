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

/** @file Basic ATAPI interface emulation. */

#include <devices/common/ata/atabasedevice.h>
#include <devices/common/ata/atapibasedevice.h>
#include <devices/common/ata/atadefs.h>
#include <endianswap.h>
#include <loguru.hpp>

#include <cinttypes>
#include <cstring>

using namespace ata_interface;

AtapiBaseDevice::AtapiBaseDevice(const std::string name)
    : AtaBaseDevice(name, DEVICE_TYPE_ATAPI) {
}

void AtapiBaseDevice::device_set_signature() {
    this->r_int_reason = 1; // shadows ATA r_sect_count
    this->r_sect_num   = 1; // required for protocol identification in OF 2.x
    this->r_dev_head   = 0;

    // set ATAPI protocol signature
    this->r_byte_count  = 0xEB14;
    this->r_status_save = this->r_status; // for restoring on the first command
    this->r_status      = 0;
}

uint16_t AtapiBaseDevice::read(const uint8_t reg_addr) {
    switch (reg_addr) {
    case ATA_Reg::DATA:
        if (has_data()) {
            uint16_t ret_data = get_data();
            this->xfer_cnt -= 2;
            if (this->xfer_cnt <= 0) {
                this->r_status &= ~DRQ;

                if ((this->r_int_reason & ATAPI_Int_Reason::IO) &&
                    !(this->r_int_reason & ATAPI_Int_Reason::CoD)
                ) {
                    if (this->data_available()) {
                        this->r_status &= ~DRQ;
                        this->r_status |= BSY;
                        this->update_intrq(1); // Is this going to work here? The interrupt happens before returning ret_data.
                    }
                    else if (this->status_expected) {
                        this->present_status();
                    }
                }
            }
            return ret_data;
        } else {
            return 0xFFFFU;
        }
    case ATA_Reg::ERROR:
        return this->r_error;
    case ATAPI_Reg::INT_REASON:
        return this->r_int_reason;
    case ATA_Reg::SEC_NUM:
        return this->r_sect_num;
    case ATAPI_Reg::BYTE_COUNT_LO:
        return this->r_byte_count & 0xFFU;
    case ATAPI_Reg::BYTE_COUNT_HI:
        return (this->r_byte_count >> 8) & 0xFFU;
    case ATA_Reg::DEVICE_HEAD:
        return this->r_dev_head;
    case ATA_Reg::STATUS:
        this->update_intrq(0);
    case ATA_Reg::ALT_STATUS:
        if (this->r_status & BSY && this->data_available()) {
            this->r_byte_count = this->request_data();
            this->data_out_phase();
        }
        return this->r_status;
    default:
        LOG_F(WARNING, "Attempted to read unknown register: %x", reg_addr);
        return 0;
    }
}

void AtapiBaseDevice::write(const uint8_t reg_addr, const uint16_t value) {
    switch (reg_addr) {
    case ATA_Reg::DATA:
        if (has_data()) {
            *this->data_ptr++ = BYTESWAP_16(value);
            this->xfer_cnt -= 2;
            if (this->xfer_cnt <= 0) {
                this->r_status &= ~DRQ;
                if (this->r_int_reason & CoD) {
                    this->perform_packet_command();
                }
            }
        }
        break;
    case ATA_Reg::FEATURES:
        this->r_features = value;
        break;
    case ATA_Reg::SEC_COUNT:
    case ATA_Reg::SEC_NUM:
        break; // unused in ATAPI, NoOp them here for compatibility
    case ATAPI_Reg::BYTE_COUNT_LO:
        this->r_byte_count = (this->r_byte_count & 0xFF00U) | (value & 0xFFU);
        break;
    case ATAPI_Reg::BYTE_COUNT_HI:
        this->r_byte_count = (this->r_byte_count & 0xFFU) | ((value & 0xFFU) << 8);
        break;
    case ATA_Reg::DEVICE_HEAD:
        this->r_dev_head = value;
        break;
    case ATA_Reg::COMMAND:
        this->r_command = value;
        if (is_selected() || this->r_command == DIAGNOSTICS) {
            perform_command();
        }
        break;
    case ATA_Reg::DEV_CTRL:
        this->device_control(value);
        break;
    default:
        LOG_F(WARNING, "Attempted to write unknown IDE register: %x", reg_addr);
    }
}

int AtapiBaseDevice::perform_command() {
    this->r_error  &= ~ATA_Error::ABRT;
    this->r_status &= ~ATA_Status::ERR;
    this->r_status |= BSY;

    switch (this->r_command) {
    case ATAPI_SOFT_RESET:
        this->device_reset(true);
        this->device_set_signature();
        this->r_status &= ~BSY;
        break;
    case ATAPI_PACKET:
        this->data_ptr = (uint16_t *)this->cmd_pkt;
        this->xfer_cnt = sizeof(this->cmd_pkt);
        this->r_int_reason &= ~ATAPI_Int_Reason::IO; // host->device
        this->r_int_reason |= ATAPI_Int_Reason::CoD; // command packet
        this->r_status |= DRQ;
        this->r_status &= ~BSY;
        break;
    case ATAPI_IDFY_DEV:
        std::memset(this->data_buf, 0, 512);
        this->data_buf[0] = 0xC0;
        this->data_buf[1] = 0x85;
        this->data_ptr = (uint16_t *)this->data_buf;
        this->xfer_cnt = 512;
        this->status_expected = false;
        this->data_out_phase();
        break;
    case SET_FEATURES:
        switch (this->r_features) {
        case 3: // set transfer mode
            LOG_F(INFO, "%s: xfer_type=0x%X, mode=0x%X", this->name.c_str(),
                this->r_sect_count >> 3, this->r_sect_count & 7);
            break;
        default:
            LOG_F(ERROR, "%s: unsupported subcommand 0x%X in SET_FEATURES",
                this->name.c_str(), this->r_features);
            this->r_error  |= ATA_Error::ABRT;
            this->r_status |= ATA_Status::ERR;
        }
        this->r_status &= ~BSY;
        this->update_intrq(1);
        break;
    default:
        LOG_F(ERROR, "%s: unsupported command 0x%X", this->name.c_str(), this->r_command);
        this->r_error  |= ATA_Error::ABRT;
        this->r_status |= ATA_Status::ERR;
    }
    return 0;
}

void AtapiBaseDevice::data_out_phase() {
    this->r_int_reason |= ATAPI_Int_Reason::IO; // device->host
    this->r_int_reason &= ~ATAPI_Int_Reason::CoD; // data
    this->signal_data_ready();
}

void AtapiBaseDevice::present_status() {
    this->r_int_reason |= ATAPI_Int_Reason::IO;
    this->r_int_reason |= ATAPI_Int_Reason::CoD;
    this->r_status &= ~DRQ;
    this->r_status &= ~BSY;
    this->update_intrq(1);
}
