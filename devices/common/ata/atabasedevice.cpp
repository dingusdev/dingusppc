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

/** @file Basic ATA device emulation. */

#include <devices/common/ata/atabasedevice.h>
#include <devices/common/ata/atadefs.h>
#include <devices/common/ata/idechannel.h>
#include <loguru.hpp>

#include <cinttypes>
#include <core/timermanager.h>

using namespace ata_interface;

AtaBaseDevice::AtaBaseDevice(const std::string name, uint8_t type) {
    this->set_name(name);
    supports_types(HWCompType::IDE_DEV);

    this->device_type = type;

    device_reset(false);
    device_set_signature();
}

void AtaBaseDevice::device_reset(bool is_soft_reset) {
    LOG_F(INFO, "%s: %s-reset triggered", this->name.c_str(),
          is_soft_reset ? "soft" : "hard");

    // Diagnostic code
    this->r_error = 1; // device 0 passed, device 1 passed or not present

    // disable interrupts
    this->r_dev_ctrl |= ATA_CTRL::IEN;
}

void AtaBaseDevice::device_set_signature() {
    this->r_sect_count = 1;
    this->r_sect_num   = 1;
    this->r_dev_head   = 0;

    // set ATA protocol signature
    this->r_cylinder_lo = 0;
    this->r_cylinder_hi = 0;
    this->r_status = DRDY | DSC; // DSC=1 is required for ATA devices
}

uint16_t AtaBaseDevice::read(const uint8_t reg_addr) {
    switch (reg_addr) {
    case ATA_Reg::DATA:
        if (this->has_data()) {
            uint16_t ret_data = this->get_data();
            this->chunk_cnt -= 2;
            if (this->chunk_cnt <= 0) {
                this->xfer_cnt -= this->chunk_size;
                if (this->xfer_cnt <= 0) {
                    this->xfer_cnt = 0;
                    this->r_status &= ~DRQ;
                } else {
                    this->chunk_cnt = std::min(this->xfer_cnt, this->chunk_size);
                    this->update_intrq(1);
                }
            }
            return ret_data;
        } else {
            return 0xFFFFU;
        }
    case ATA_Reg::ERROR:
        return this->r_error;
    case ATA_Reg::SEC_COUNT:
        return this->r_sect_count;
    case ATA_Reg::SEC_NUM:
        return this->r_sect_num;
    case ATA_Reg::CYL_LOW:
        return this->r_cylinder_lo;
    case ATA_Reg::CYL_HIGH:
        return this->r_cylinder_hi;
    case ATA_Reg::DEVICE_HEAD:
        return this->r_dev_head;
    case ATA_Reg::STATUS:
        this->update_intrq(0);
        return this->r_status;
    case ATA_Reg::ALT_STATUS:
        return this->r_status;
    default:
        LOG_F(WARNING, "Attempted to read unknown IDE register: %x", reg_addr);
        return 0;
    }
}

void AtaBaseDevice::write(const uint8_t reg_addr, const uint16_t value) {
    switch (reg_addr) {
    case ATA_Reg::DATA:
        if (is_selected() && this->has_data()) {
            *this->cur_data_ptr++ = BYTESWAP_16(value);
            this->chunk_cnt -= 2;
            if (this->chunk_cnt <= 0) {
                this->post_xfer_action();
                this->xfer_cnt -= this->chunk_size;
                if (this->xfer_cnt <= 0) { // transfer complete?
                    this->xfer_cnt = 0;
                    this->r_status &= ~DRQ;
                    this->r_status &= ~BSY;
                    this->update_intrq(1);
                } else {
                    this->cur_data_ptr = this->data_ptr;
                    this->chunk_cnt = std::min(this->xfer_cnt, this->chunk_size);
                    this->signal_data_ready();
                }
            }
        }
        break;
    case ATA_Reg::FEATURES:
        this->r_features = value;
        break;
    case ATA_Reg::SEC_COUNT:
        this->r_sect_count = value;
        break;
    case ATA_Reg::SEC_NUM:
        this->r_sect_num = value;
        break;
    case ATA_Reg::CYL_LOW:
        this->r_cylinder_lo = value;
        break;
    case ATA_Reg::CYL_HIGH:
        this->r_cylinder_hi = value;
        break;
    case ATA_Reg::DEVICE_HEAD:
        this->r_dev_head = value;
        break;
    case ATA_Reg::COMMAND:
        if ((this->r_status & BSY) != 0) {
            LOG_F(ERROR, "%s: tried to perform command %x when busy", name.c_str(), value);
            break;
        }
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

void AtaBaseDevice::device_control(const uint8_t new_ctrl) {
    // perform ATA Soft Reset if requested
    if ((this->r_dev_ctrl ^ new_ctrl) & SRST) {
        if (new_ctrl & SRST) { // SRST set -> phase 0 aka self-test
            this->r_status |= BSY;
            this->device_reset(true);
        } else { // SRST cleared -> phase 1 aka signature and error report
            if (!this->my_dev_id && this->host_obj->is_device1_present())
                this->r_error |= 0x80;
            this->device_set_signature();
            this->r_status &= ~BSY;

            if (this->my_dev_id && this->r_error == 1) {
                this->host_obj->assert_pdiag();
            }
        }
    }
    this->r_dev_ctrl = new_ctrl;
}

void AtaBaseDevice::update_intrq(uint8_t new_intrq_state) {
    if (!this->is_selected() || (this->r_dev_ctrl & IEN))
        return;

    this->intrq_state = new_intrq_state;
    this->host_obj->report_intrq(new_intrq_state);
}

void AtaBaseDevice::prepare_xfer(int xfer_size, int block_size) {
    this->chunk_cnt  = std::min(xfer_size, block_size);
    this->xfer_cnt   = xfer_size;
    this->chunk_size = block_size;
}

void AtaBaseDevice::signal_data_ready() {
    this->r_status |= DRQ;
    this->r_status &= ~BSY;
    this->update_intrq(1);
}
