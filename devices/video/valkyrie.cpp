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

/** @file Valkyrie video controller emulation. */

#include <core/timermanager.h>
#include <devices/common/i2c/i2c.h>
#include <devices/deviceregistry.h>
#include <devices/memctrl/memctrlbase.h>
#include <devices/video/pdmonboard.h>
#include <devices/video/valkyrie.h>
#include <loguru.hpp>
#include <machines/machinebase.h>
#include <machines/machineproperties.h>

ValkyrieVideo::ValkyrieVideo(const uint32_t base_addr) : VideoCtrlBase() {
    set_name("Valkyrie");

    supports_types(HWCompType::MMIO_DEV);

    this->reg_shift = (base_addr == Valkyrie::REGBASE_CORDYCEPS) ? 2 : 3;

    MemCtrlBase* mem_ctrl = dynamic_cast<MemCtrlBase*>(
        gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));

    // add MMIO region for VRAM
    mem_ctrl->add_ram_region(Valkyrie::VRAM_BASE, DRAM_CAP_1MB);
    this->vram_ptr = mem_ctrl->get_region_hostmem_ptr(Valkyrie::VRAM_BASE);

    // add MMIO region for the configuration and status registers
    mem_ctrl->add_mmio_region(base_addr + Valkyrie::CONTROL_OFFSET, 0x1000, this);

    // add MMIO region for the CLUT
    mem_ctrl->add_mmio_region(base_addr + Valkyrie::CLUT_OFFSET, 0x1000, this);

    this->disp_id = std::unique_ptr<DisplayID> (new DisplayID());

    // initialize the video clock generator
    this->clk_gen = std::unique_ptr<AthensClocks>(new AthensClocks(0x28));

    // register the video clock generator with the I2C host
    I2CBus* i2c_bus = dynamic_cast<I2CBus*>(gMachineObj->get_comp_by_type(
        HWCompType::I2C_HOST));
    i2c_bus->register_device(0x28, this->clk_gen.get());
}

int ValkyrieVideo::device_postinit() {
    this->int_ctrl = dynamic_cast<InterruptCtrl*>(
        gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
    this->irq_id = this->int_ctrl->register_dev_int(IntSrc::VALKYRIE);

    this->vbl_cb = [this](uint8_t irq_line_state) {
        if (!(this->int_latch & Valkyrie::VBL_IRQ) && irq_line_state) {
            this->int_latch |= Valkyrie::VBL_IRQ;
            if (this->int_en & Valkyrie::VBL_IRQ)
                this->int_ctrl->ack_int(this->irq_id, irq_line_state);
        }
    };

    return 0;
}

uint32_t ValkyrieVideo::read(uint32_t rgn_start, uint32_t offset, int size) {
    if ((rgn_start & 0xFFFF) == Valkyrie::CLUT_OFFSET) {
        LOG_F(WARNING, "%s: CLUT is read", this->name.c_str());
        return 0;
    }

    switch((offset >> this->reg_shift) & 0xFF) {
    case Valkyrie::csr_mode:
        return this->mode;
    case Valkyrie::csr_id_reset:
        return this->chip_id;
    case Valkyrie::csr_int_stat:
        return this->int_latch;
    case Valkyrie::csr_monid:
        return this->mon_id;
    default:
        LOG_F(WARNING, "%s: unknown reg at 0x%X is read", this->name.c_str(),
              rgn_start + offset);
    }

    return 0;
}

void ValkyrieVideo::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) {
    if ((rgn_start & 0xFFFF) == Valkyrie::CLUT_OFFSET) {
        switch((offset >> this->reg_shift) & 3) {
        case Valkyrie::clut_index:
            this->clut_index = value;
            this->comp_index = 0;
            break;
        case Valkyrie::clut_color:
            this->clut_color[this->comp_index++] = value;
            if (this->comp_index >= 3) {
                this->set_palette_color(this->clut_index, clut_color[0],
                                        clut_color[1], clut_color[2], 0xFF);
                this->clut_index++;
                this->comp_index = 0;
            }
            break;
        default:
            LOG_F(WARNING, "%s: write to unknown CLUT reg at 0x%X", this->name.c_str(),
                  rgn_start + offset);
        }
        return;
    }

    switch((offset >> this->reg_shift) & 0xFF) {
    case Valkyrie::csr_mode:
        if (value != this->mode) {
            auto old_mode = this->mode;
            this->mode = value;
            if ((value ^ old_mode) & 0x80) {
                if (value & 0x80)
                    this->disable_video_internal();
                else
                    this->schedule_mode_switch();
            }
        }
        LOG_F(INFO, "%s: mode reg set to 0x%X", this->name.c_str(), this->mode);
        break;
    case Valkyrie::csr_depth:
        if (value != this->depth)
            this->schedule_mode_switch();
        this->depth = value;
        break;
    case Valkyrie::csr_int_stat:
        if (this->int_latch) {
            this->int_latch &= ~value;
            if (!this->int_latch)
                this->int_ctrl->ack_int(this->irq_id, 0);
        }
        break;
    case Valkyrie::csr_int_en:
        if (value & 0x80)
            this->int_en |= value & 0x7F;
        else
            this->int_en &= ~value;
        if (!int_en)
            LOG_F(INFO, "%s: all ints disabled", this->name.c_str());
        else
            LOG_F(INFO, "%s: ints enabled = 0x%X", this->name.c_str(), this->int_en);
        break;
    case Valkyrie::csr_monid: {
            // extract and convert pin directions (0 - input, 1 - output)
            uint8_t dirs = value & 7;
            if (dirs == 7 && !(value & 8)) {
                LOG_F(INFO, "%s: monitor sense lines tristated", this->name.c_str());
            }
            // propagate bit 3 to all pins configured as output
            // set levels for all input pins to "1"
            uint8_t levels = (7 ^ dirs) | (((value & 8) ? 7 : 0) & dirs);
            // read monitor sense lines and store the result in the bits 4-6
            this->mon_id = (this->mon_id & 0xF) |
                (this->disp_id->read_monitor_sense(levels, dirs) << 4);
        }
        break;
    default:
        LOG_F(WARNING, "%s: unknown reg at 0x%X is written", this->name.c_str(),
              rgn_start + offset);
    }
}

void ValkyrieVideo::disable_video_internal() {
    this->stop_refresh_task();
    this->blank_on = true;
    this->blank_display();
    this->crtc_on = false;
    LOG_F(INFO, "%s: video disabled", this->name.c_str());
}

void ValkyrieVideo::schedule_mode_switch() {
    if (this->mode_timer_id)
        TimerManager::get_instance()->cancel_timer(this->mode_timer_id);

    this->mode_timer_id = TimerManager::get_instance()->add_oneshot_timer(
        MSECS_TO_NSECS(3), [this] {
        this->enable_video_internal();
    });
}

void ValkyrieVideo::enable_video_internal() {
    int new_width, new_height, hori_blank, vert_blank;

    // get pixel frequency from Athens
    this->pixel_clock = this->clk_gen->get_dot_freq();

    // set video mode parameters
    switch(this->mode & 0x7F) {
    case PdmVideoMode::Rgb13in:
        new_width  = 640;
        new_height = 480;
        hori_blank = 224;
        vert_blank =  45;
        break;
    case PdmVideoMode::Rgb16in:
        new_width  = 832;
        new_height = 624;
        hori_blank = 320;
        vert_blank =  43;
        break;
    case PdmVideoMode::VGA:
        new_width  = 640;
        new_height = 480;
        hori_blank = 160;
        vert_blank =  45;
        break;
    default:
        LOG_F(ERROR, "%s: invalid video mode 0x%X", this->name.c_str(), this->mode & 0x7F);
        return;
    }

    static uint8_t pix_depths[8] = {1, 2, 4, 8, 16, 0xFF, 0xFF, 0xFF};

    uint8_t new_pix_depth = pix_depths[this->depth];
    if (new_pix_depth == 0xFF) {
        LOG_F(ERROR, "%s: invalid pixel depth code %d specified", this->name.c_str(),
              this->depth);
        return;
    }

    this->stop_refresh_task();

    // set CRTC parameters
    this->active_width  = new_width;
    this->active_height = new_height;
    this->hori_blank    = hori_blank;
    this->vert_blank    = vert_blank;
    this->hori_total    = new_width  + hori_blank;
    this->vert_total    = new_height + vert_blank;

    this->fb_ptr = &this->vram_ptr[0x1000];

    this->pixel_depth = new_pix_depth;

    LOG_F(INFO, "%s: pixel depth set to %d", this->name.c_str(), this->pixel_depth);

    switch (this->pixel_depth) {
    case 1:
        this->convert_fb_cb = [this](uint8_t* dst_buf, int dst_pitch) {
            this->convert_frame_1bpp_indexed(dst_buf, dst_pitch);
        };
        this->fb_pitch = this->active_width >> 3; // one byte contains 8 pixels
        break;
    case 8:
        this->convert_fb_cb = [this](uint8_t* dst_buf, int dst_pitch) {
            this->convert_frame_8bpp_indexed(dst_buf, dst_pitch);
        };
        this->fb_pitch = this->active_width; // one byte contains 1 pixel
        break;
    case 16:
        this->convert_fb_cb = [this](uint8_t* dst_buf, int dst_pitch) {
            this->convert_frame_15bpp<BE>(dst_buf, dst_pitch);
        };
        this->fb_pitch = this->active_width << 1; // 1 pixel is 2 bytes
        break;
    default:
        ABORT_F("%s: pixel depth %d not implemented yet!", this->name.c_str(),
                this->pixel_depth);
    }

    // set up video refresh timer
    this->refresh_rate = (double)(this->pixel_clock) / hori_total / vert_total;
    LOG_F(INFO, "%s: refresh rate set to %f Hz", this->name.c_str(), this->refresh_rate);
    this->start_refresh_task();

    this->blank_on = false;
    this->crtc_on = true;
    LOG_F(INFO, "%s: video enabled", this->name.c_str());
}

// ========================== Device registry stuff ==========================
static const PropMap Valkyrie_Properties = {
    {"mon_id",
        new StrProperty("HiRes12-14in")},
};

static const DeviceDescription Valkyrie_Cordyceps_Descriptor = {
    ValkyrieVideo::create_for_cordyceps, {}, Valkyrie_Properties, HWCompType::MMIO_DEV
};

static const DeviceDescription Valkyrie_Alchemy_Descriptor = {
    ValkyrieVideo::create_for_alchemy, {}, Valkyrie_Properties, HWCompType::MMIO_DEV
};

REGISTER_DEVICE(ValkyrieCordyceps, Valkyrie_Cordyceps_Descriptor);
REGISTER_DEVICE(ValkyrieAlchemy,   Valkyrie_Alchemy_Descriptor);
