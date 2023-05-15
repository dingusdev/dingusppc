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

/** @file Basic ATAPI interface emulation. */

#include <devices/common/ata/atabasedevice.h>
#include <devices/common/ata/atapibasedevice.h>
#include <devices/common/ata/atadefs.h>
#include <devices/common/scsi/scsi.h> // ATAPI reuses SCSI commands (sic!)
#include <endianswap.h>
#include <loguru.hpp>
#include <memaccess.h>

#include <cinttypes>

using namespace ata_interface;

AtapiBaseDevice::AtapiBaseDevice(const std::string name)
    : AtaBaseDevice(name, DEVICE_TYPE_ATAPI) {
}

void AtapiBaseDevice::device_set_signature() {
    this->r_sect_count = 1;
    this->r_sect_num   = 1;
    this->r_dev_head   = 0;

    // set ATAPI protocol signature
    this->r_byte_count  = 0xEB14;
    this->r_status_save = this->r_status; // for restoring on the first command
    this->r_status      = 0;
}

uint16_t AtapiBaseDevice::read(const uint8_t reg_addr) {
    switch (reg_addr) {
    case ATA_Reg::DATA:
        if (this->data_ptr && this->xfer_cnt) {
            this->xfer_cnt -= 2;
            if (this->xfer_cnt <= 0) {
                this->r_status &= ~DRQ;
            }
            return *this->data_ptr++;
        } else {
            return 0xFFFFU;
        }
    case ATA_Reg::ERROR:
        return this->r_error;
    case ATAPI_Reg::INT_REASON:
        return this->r_int_reason;
    case ATAPI_Reg::BYTE_COUNT_LO:
        return this->r_byte_count & 0xFFU;
    case ATAPI_Reg::BYTE_COUNT_HI:
        return (this->r_byte_count >> 8) & 0xFFU;
    case ATA_Reg::DEVICE_HEAD:
        return this->r_dev_head;
    case ATA_Reg::STATUS:
        // TODO: clear pending interrupt
        return this->r_status;
    case ATA_Reg::ALT_STATUS:
        return this->r_status;
    default:
        LOG_F(WARNING, "Attempted to read unknown register: %x", reg_addr);
        return 0;
    }
}

void AtapiBaseDevice::write(const uint8_t reg_addr, const uint16_t value) {
    switch (reg_addr) {
    case ATA_Reg::DATA:
        if (this->data_ptr && this->xfer_cnt) {
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
    this->r_status |= BSY;

    switch (this->r_command) {
    case ATAPI_PACKET:
        this->data_ptr = (uint16_t *)this->cmd_pkt;
        this->xfer_cnt = sizeof(this->cmd_pkt);
        this->r_int_reason &= ~ATAPI_Int_Reason::IO; // host->device
        this->r_int_reason |= ATAPI_Int_Reason::CoD; // command packet
        this->r_status |= DRQ;
        this->r_status &= ~BSY;
        break;
    case ATAPI_IDFY_DEV:
        LOG_F(INFO, "ATAPI_IDENTIFY issued");
        this->data_buf[0] = 0x85;
        this->data_buf[1] = 0xC0;
        this->data_ptr = (uint16_t *)this->data_buf;
        this->data_pos = 0;
        this->xfer_cnt = 512;
        this->r_status |= DRQ;
        this->r_status &= ~BSY;
        break;
    case SET_FEATURES:
        LOG_F(INFO, "%s: SET_FEATURES #%d", this->name.c_str(), this->r_features);
        this->r_status &= ~BSY;
        break;
    default:
        LOG_F(ERROR, "%s: unsupported command 0x%X", this->name.c_str(), this->r_command);
        return -1;
    }
    return 0;
}

void AtapiBaseDevice::perform_packet_command() {
    uint32_t lba, xfer_len;

    this->r_status |= BSY;

    switch (this->cmd_pkt[0]) {
    case ScsiCommand::TEST_UNIT_READY:
        this->r_status &= ~BSY;
        break;
    case ScsiCommand::START_STOP_UNIT:
        this->r_status &= ~BSY;
        break;
    case ScsiCommand::READ_12:
        lba      = READ_DWORD_BE_U(&this->cmd_pkt[2]);
        xfer_len = READ_DWORD_BE_U(&this->cmd_pkt[6]);
        this->r_int_reason |= ~ATAPI_Int_Reason::IO; // device->host
        this->r_int_reason &= ATAPI_Int_Reason::CoD; // data
        this->xfer_cnt = xfer_len * 2048;
        this->r_status |= DRQ;
        this->r_status &= ~BSY;
        break;
    default:
        LOG_F(INFO, "%s: unsupported ATAPI command 0x%X", this->name.c_str(),
              this->cmd_pkt[0]);
    }
}
