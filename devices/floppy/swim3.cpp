/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-22 divingkatae and maximum
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

/** @file Sander-Wozniak Machine 3 (SWIM3) emulation. */

#include <core/timermanager.h>
#include <devices/deviceregistry.h>
#include <devices/common/dmacore.h>
#include <devices/common/hwinterrupt.h>
#include <devices/floppy/superdrive.h>
#include <devices/floppy/swim3.h>
#include <loguru.hpp>
#include <machines/machinebase.h>
#include <machines/machineproperties.h>

#include <cinttypes>
#include <string>
#include <vector>

using namespace Swim3;

Swim3Ctrl::Swim3Ctrl()
{
    this->name = "SWIM3";
    this->supported_types = HWCompType::FLOPPY_CTRL;

    this->setup_reg  = 0;
    this->mode_reg   = 0;
    this->int_reg    = 0;
    this->int_flags  = 0;
    this->int_mask   = 0;
    this->error      = 0;
    this->step_count = 0;
    this->xfer_cnt   = 0;
    this->first_sec  = 0xFF;

    this->cur_state = SWIM3_IDLE;

    // Attach virtual Superdrive to the internal drive connector
    // TODO: make SWIM3/drive wiring user selectable
    this->int_drive = std::unique_ptr<MacSuperdrive::MacSuperDrive>
        (new MacSuperdrive::MacSuperDrive());
}

int Swim3Ctrl::device_postinit()
{
    this->int_ctrl = dynamic_cast<InterruptCtrl*>(
        gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
    this->irq_id = this->int_ctrl->register_dev_int(IntSrc::SWIM3);

    // if a floppy image was given "insert" it into the virtual superdrive
    std::string fd_image_path = GET_STR_PROP("fdd_img");
    int fd_write_prot = GET_BIN_PROP("fdd_wr_prot");
    if (!fd_image_path.empty()) {
        this->int_drive->insert_disk(fd_image_path, fd_write_prot);
    }

    return 0;
};

uint8_t Swim3Ctrl::read(uint8_t reg_offset)
{
    uint8_t status_addr, rddata_val, old_int_flags, old_error;

    switch(reg_offset) {
    case Swim3Reg::Timer:
        return this->calc_timer_val();
    case Swim3Reg::Error:
        old_error = this->error;
        this->error = 0;
        return old_error;
    case Swim3Reg::Phase:
        return this->phase_lines;
    case Swim3Reg::Setup:
        return this->setup_reg;
    case Swim3Reg::Handshake_Mode1:
        if (this->mode_reg & 2) { // internal drive?
            status_addr = ((this->mode_reg & 0x20) >> 2) | (this->phase_lines & 7);
            rddata_val  = this->int_drive->status(status_addr) & 1;

            // transfer rddata_val to both bit 2 (RDDATA) and bit 3 (SENSE)
            // because those signals seem to be historically wired together
            return (rddata_val << 2) | (rddata_val << 3);
        }
        return 0xC; // report both RdData & Sense high
    case Swim3Reg::Interrupt_Flags:
        old_int_flags = this->int_flags;
        this->int_flags = 0; // read from this register clears all flags
        update_irq();
        return old_int_flags;
    case Swim3Reg::Step:
        return this->step_count;
    case Swim3Reg::Current_Track:
        return this->cur_track;
    case Swim3Reg::Current_Sector:
        return this->cur_sector;
    case Swim3Reg::Gap_Format:
        return this->format;
    case Swim3Reg::First_Sector:
        return this->first_sec;
    case Swim3Reg::Sectors_To_Xfer:
        return this->xfer_cnt;
    case Swim3Reg::Interrupt_Mask:
        return this->int_mask;
    default:
        LOG_F(INFO, "SWIM3: reading from 0x%X register", reg_offset);
    }
    return 0;
}

void Swim3Ctrl::write(uint8_t reg_offset, uint8_t value)
{
    uint8_t status_addr;

    switch(reg_offset) {
    case Swim3Reg::Timer:
        this->init_timer(value);
        break;
    case Swim3Reg::Param_Data:
        this->pram = value;
        break;
    case Swim3Reg::Phase:
        this->phase_lines = value & 0xF;
        if (this->phase_lines & 8) { // CA3 aka LSTRB high -> sending a command to the drive
            if (this->mode_reg & 2) { // if internal drive is selected
                this->int_drive->command(
                    ((this->mode_reg & 0x20) >> 3) | (this->phase_lines & 3),
                    (value >> 2) & 1
                );
            }
        } else if (this->phase_lines == 4 && (this->mode_reg & 2)) {
            status_addr = ((this->mode_reg & 0x20) >> 2) | (this->phase_lines & 7);
            this->rd_line = this->int_drive->status(status_addr) & 1;
        }
        break;
    case Swim3Reg::Setup:
        this->setup_reg = value;
        break;
    case Swim3Reg::Status_Mode0:
        // ones in value clear the corresponding bits in the mode register
        if ((this->mode_reg & value) & (SWIM3_GO | SWIM3_GO_STEP)) {
            if (value & SWIM3_GO_STEP) {
                stop_stepping();
            } else {
                stop_disk_access();
            }
        }
        this->mode_reg &= ~value;
        break;
    case Swim3Reg::Handshake_Mode1:
        // ones in value set the corresponding bits in the mode register
        if ((this->mode_reg ^ value) & (SWIM3_GO | SWIM3_GO_STEP)) {
            if (value & SWIM3_GO_STEP) {
                start_stepping();
            } else {
                start_disk_access();
            }
        }
        this->mode_reg |= value;
        break;
    case Swim3Reg::Step:
        this->step_count = value;
        break;
    case Swim3Reg::Gap_Format:
        this->gap_size = value;
        break;
    case Swim3Reg::First_Sector:
        this->first_sec = value;
        break;
    case Swim3Reg::Sectors_To_Xfer:
        this->xfer_cnt = value;
        break;
    case Swim3Reg::Interrupt_Mask:
        this->int_mask = value;
        break;
    default:
        LOG_F(INFO, "SWIM3: writing 0x%X to register 0x%X", value, reg_offset);
    }
}

void Swim3Ctrl::update_irq()
{
    if (this->mode_reg & SWIM3_INT_ENA) {
        uint8_t new_irq = !!(this->int_flags & this->int_mask);
        if (new_irq != this->irq) {
            this->irq = new_irq;
            this->int_ctrl->ack_int(this->irq_id, new_irq);
        }
    }
}

void Swim3Ctrl::do_step()
{
    if (this->mode_reg & SWIM3_GO_STEP && this->step_count) { // are we still stepping?
        // instruct the drive to perform single step in current direction
        this->int_drive->command(MacSuperdrive::CommandAddr::Do_Step, 0);
        if (--this->step_count == 0) {
            if (this->step_timer_id) {
                this->stop_stepping();
            }
            this->int_flags |= INT_STEP_DONE;
            update_irq();
        }
    }
}

void Swim3Ctrl::start_stepping()
{
    if (!this->step_count) {
        LOG_F(WARNING, "SWIM3: step_count is zero while go_step is active!");
        return;
    }

    if (this->mode_reg & SWIM3_GO_STEP || this->step_timer_id) {
        LOG_F(ERROR, "SWIM3: another stepping action is running!");
        return;
    }

    if (this->mode_reg & SWIM3_GO || this->access_timer_id) {
        LOG_F(ERROR, "SWIM3: stepping attempt while disk access is in progress!");
        return;
    }

    if ((((this->mode_reg & 0x20) >> 3) | (this->phase_lines & 3))
        != MacSuperdrive::CommandAddr::Do_Step) {
        LOG_F(WARNING, "SWIM3: invalid command address on the phase lines!");
        return;
    }

    this->mode_reg |= SWIM3_GO_STEP;

    // step count > 1 requires periodic task
    if (this->step_count > 1) {
        this->step_timer_id = TimerManager::get_instance()->add_cyclic_timer(
            USECS_TO_NSECS(80),
            [this]() {
                this->do_step();
            }
        );
    }

    // perform the first step immediately
    do_step();
}

void Swim3Ctrl::stop_stepping()
{
    // cancel stepping task
    if (this->step_timer_id) {
        TimerManager::get_instance()->cancel_timer(this->step_timer_id);
    }
    this->step_timer_id = 0;
    this->step_count = 0; // not sure this one is required
}

void Swim3Ctrl::start_disk_access()
{
    if (this->mode_reg & SWIM3_GO || this->access_timer_id) {
        LOG_F(ERROR, "SWIM3: another disk access is running!");
        return;
    }

    if (this->mode_reg & SWIM3_GO_STEP || this->step_timer_id) {
        LOG_F(ERROR, "SWIM3: disk access attempt while stepping is in progress!");
        return;
    }

    if (this->mode_reg & SWIM3_WR_MODE) {
        LOG_F(ERROR, "SWIM3: writing not implemented yet");
        return;
    }

    this->mode_reg |= SWIM3_GO;
    LOG_F(9, "SWIM3: disk access started!");

    this->target_sect = this->first_sec;

    this->access_timer_id = TimerManager::get_instance()->add_oneshot_timer(
        this->int_drive->sync_to_disk(),
        [this]() {
            this->cur_state = SWIM3_ADDR_MARK_SEARCH;
            this->disk_access();
        }
    );
}

void Swim3Ctrl::disk_access()
{
    MacSuperdrive::SectorHdr hdr;
    uint64_t delay;

    switch(this->cur_state) {
    case SWIM3_ADDR_MARK_SEARCH:
        hdr = this->int_drive->current_sector_header();
        // update the corresponding SWIM3 registers
        this->cur_track  = ((hdr.side & 1) << 7) | (hdr.track & 0x7F);
        this->cur_sector = 0x80 /* CRC/checksum valid */ | (hdr.sector & 0x7F);
        this->format = hdr.format;
        // generate ID_read interrupt
        this->int_flags |= INT_ID_READ;
        update_irq();
        if ((this->cur_sector & 0x7F) == this->target_sect) {
            // sector matches -> transfer its data
            this->cur_state = SWIM3_DATA_XFER;
            delay = this->int_drive->sector_data_delay();
        } else {
            // move to next address mark
            this->cur_state = SWIM3_ADDR_MARK_SEARCH;
            delay = this->int_drive->next_sector_delay();
        }
        break;
    case SWIM3_DATA_XFER:
        // transfer sector data over DMA
        this->dma_ch->push_data(this->int_drive->get_sector_data_ptr(this->cur_sector & 0x7F), 512);
        if (--this->xfer_cnt == 0) {
            this->stop_disk_access();
            // generate sector_done interrupt
            this->int_flags |= INT_SECT_DONE;
            update_irq();
            return;
        }
        this->cur_state = SWIM3_ADDR_MARK_SEARCH;
        delay = this->int_drive->next_addr_mark_delay(&this->target_sect);
        break;
    default:
        LOG_F(ERROR, "SWIM3: unknown disk access phase 0x%X", this->cur_state);
        return;
    }

    this->access_timer_id = TimerManager::get_instance()->add_oneshot_timer(
        delay,
        [this]() {
            this->disk_access();
        }
    );
}

void Swim3Ctrl::stop_disk_access()
{
    // cancel disk access timer
    if (this->access_timer_id) {
        TimerManager::get_instance()->cancel_timer(this->access_timer_id);
    }
    this->access_timer_id = 0;
}

void Swim3Ctrl::init_timer(const uint8_t start_val)
{
    if (this->timer_val) {
        LOG_F(WARNING, "SWIM3: attempt to re-arm the timer");
    }
    this->timer_val = start_val;
    if (!this->timer_val) {
        this->one_us_timer_start = 0;
        return;
    }

    this->one_us_timer_start = TimerManager::get_instance()->current_time_ns();

    this->one_us_timer_id = TimerManager::get_instance()->add_oneshot_timer(
        this->timer_val * NS_PER_USEC,
        [this]() {
            this->timer_val = 0;
            this->int_flags |= INT_TIMER_DONE;
            update_irq();
        }
    );
}

uint8_t Swim3Ctrl::calc_timer_val()
{
    if (!this->timer_val) {
        return 0;
    }

    uint64_t time_now   = TimerManager::get_instance()->current_time_ns();
    uint64_t us_elapsed = (time_now - this->one_us_timer_start) / NS_PER_USEC;
    if (us_elapsed > this->timer_val) {
        return 0;
    } else {
        return (this->timer_val - us_elapsed) & 0xFFU;
    }
}

// floppy disk formats properties for the cases
// where disk format needs to be specified manually
static const std::vector<std::string> FloppyFormats = {
    "", "GCR_400K", "GCR_800K", "MFM_720K", "MFM_1440K"
};

static const PropMap Swim3_Properties = {
    {"fdd_img",
        new StrProperty("")},
    {"fdd_wr_prot",
        new BinProperty(0)},
    {"fdd_fmt",
        new StrProperty("", FloppyFormats)},
};

static const DeviceDescription Swim3_Descriptor = {
    Swim3Ctrl::create, {}, Swim3_Properties
};

REGISTER_DEVICE(Swim3, Swim3_Descriptor);
