/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-24 divingkatae and maximum
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

/** @file Taos video controller emulation. */

#include <core/bitops.h>
#include <devices/common/i2c/athens.h>
#include <devices/deviceregistry.h>
#include <devices/memctrl/memctrlbase.h>
#include <devices/video/taos.h>
#include <machines/machinebase.h>
#include <machines/machineproperties.h>
#include <loguru.hpp>
#include <memaccess.h>

#include <string>

TaosVideo::TaosVideo() : VideoCtrlBase(640, 480) {
    set_name("Taos");

    supports_types(HWCompType::MMIO_DEV);

    // initialize the video clock generator
    this->clk_gen = std::unique_ptr<AthensClocks>(new AthensClocks(0x29, 20000000.0f));

    // register the video clock generator with the I2C host
    I2CBus* i2c_bus = dynamic_cast<I2CBus*>(gMachineObj->get_comp_by_type(
        HWCompType::I2C_HOST));
    i2c_bus->register_device(0x29, this->clk_gen.get());

    // initialize video encoder
    this->vid_enc = std::unique_ptr<Bt856>(new Bt856(0x44));

    // register the video encoder with the I2C host
    i2c_bus->register_device(0x44, this->vid_enc.get());

    MemCtrlBase* mem_ctrl = dynamic_cast<MemCtrlBase*>(
        gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));

    // add MMIO region for VRAM
    mem_ctrl->add_ram_region(TAOS_VRAM_REGION_BASE, DRAM_CAP_1MB);
    this->vram_ptr = mem_ctrl->get_region_hostmem_ptr(TAOS_VRAM_REGION_BASE);

    // add MMIO region for the configuration and status registers
    mem_ctrl->add_mmio_region(TAOS_IOREG_BASE, 0x800, this);

    // add MMIO region for the CLUT
    mem_ctrl->add_mmio_region(TAOS_CLUT_BASE, 0x400, this);

    this->vbl_cb = [this](uint8_t irq_line_state) {
        this->vsync_active = irq_line_state;
    };

    std::string video_out = GET_STR_PROP("video_out");
    if (video_out == "VGA")
        this->mon_id = MON_ID_VGA;
    else if (video_out == "PAL")
        this->mon_id = MON_ID_PAL;
    else
        this->mon_id = MON_ID_NTSC; // NTSC is set by default in the driver
}

uint32_t TaosVideo::read(uint32_t rgn_start, uint32_t offset, int size) {
    if (rgn_start == TAOS_CLUT_BASE) {
        uint8_t r, g, b, a;
        get_palette_color(offset >> 2, r, g, b, a);
        return (r << 24) | (g << 16) | (b << 8);
    }

    int reg_num = offset >> 2;

    switch(reg_num) {
    case CRT_CTRL:
        return this->crt_ctrl;
    case GPIO_IN:
        return ((this->mon_id << 29) | (vsync_active << 25)) & ~this->gpio_cfg;
    case INT_ENABLES:
        return this->int_enables;
    case TAOS_VERSION:
        return TAOS_CHIP_VERSION;
    default:
        LOG_F(WARNING, "%s: reading register at 0x%X", this->name.c_str(), offset);
    }

    return 0;
}

void TaosVideo::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) {
    if (rgn_start == TAOS_CLUT_BASE) {
        set_palette_color(offset >> 2, value >> 24, (value >> 16) & 0xFF,
                         (value >> 8) & 0xFF, 0xFF);
        return;
    }

    int reg_num = offset >> 2;

    switch(reg_num) {
    case FB_BASE:
        this->fb_base = value;
        break;
    case ROW_WORDS:
        this->row_words = value;
        break;
    case COLOR_MODE:
        this->color_mode = value;
        break;
    case VIDEO_MODE:
        this->video_mode = value >> 30;
        break;
    case CRT_CTRL:
        if (bit_changed(this->crt_ctrl, value, ENABLE_VIDEO_OUT)) {
            if (bit_set(value, ENABLE_VIDEO_OUT))
                enable_display();
        }
        this->crt_ctrl = value;
        break;
    case HEQ:
    case HBWAY:
    case HAL:
    case HSERR:
    case HFP:
    case HPIX:
    case HSP:
    case HLFLN:
    case VBPEQ:
    case VBP:
    case VAL:
    case VFP:
    case VFPEQ:
    case VSYNC:
    case VHLINE:
        this->swatch_regs[REG_TO_INDEX(reg_num)] = value;
        break;
    case GPIO_CONFIG:
        this->gpio_cfg = value >> 24;
        break;
    case GPIO_OUT:
        LOG_F(INFO, "%s: value 0x%X written into GPIO pins, GPIO_CONFIG=0x%X",
              this->name.c_str(), value >> 24, this->gpio_cfg);
        break;
    case INT_ENABLES:
        this->int_enables = value;
        if (bit_set(value, 29))
            LOG_F(WARNING, "%s: VBL interrupt enabled", this->name.c_str());
        for (int gpio_pin = 0; gpio_pin < 8; gpio_pin++) {
            if (bit_set(value, 27 - gpio_pin))
                LOG_F(INFO, "%s: GPIO %d interrupt enabled", this->name.c_str(),
                      gpio_pin);
        }
        break;
    default:
        LOG_F(WARNING, "%s: register at 0x%X set to 0x%X", this->name.c_str(), offset, value);
    }
}

void TaosVideo::enable_display() {
    int new_width, new_height;

    // get pixel frequency from Athens
    this->pixel_clock = this->clk_gen->get_dot_freq();

    // calculate active_width and active_height from video timing parameters
    new_width  = (swatch_regs[REG_TO_INDEX(HFP)] >> 20) - (swatch_regs[REG_TO_INDEX(HAL)] >> 20);
    new_height = (swatch_regs[REG_TO_INDEX(VFP)] >> 20) - (swatch_regs[REG_TO_INDEX(VAL)] >> 20);

    LOG_F(INFO, "%s: width=%d, height=%d", this->name.c_str(), new_width, new_height);

    // calculate display refresh rate
    this->hori_blank = (swatch_regs[REG_TO_INDEX(HAL)] >> 20) +
        ((swatch_regs[REG_TO_INDEX(HSP)] >> 20) - (swatch_regs[REG_TO_INDEX(HFP)] >> 20));

    this->vert_blank = (swatch_regs[REG_TO_INDEX(VAL)] >> 20) +
        ((swatch_regs[REG_TO_INDEX(VSYNC)] >> 20) - (swatch_regs[REG_TO_INDEX(VFP)] >> 20));

    this->hori_total = this->hori_blank + new_width;
    this->vert_total = this->vert_blank + new_height;

    this->stop_refresh_task();

    this->refresh_rate = (double)(this->pixel_clock) / (this->hori_total * this->vert_total);

    LOG_F(INFO, "%s: refresh rate set to %f Hz", this->name.c_str(), this->refresh_rate);

    // set framebuffer parameters
    this->fb_ptr   = &this->vram_ptr[this->fb_base >> 20];
    this->fb_pitch = this->row_words >> 20;

    if (bit_set(this->color_mode, 31)) {
        this->pixel_depth = 16;
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_15bpp_indexed(dst_buf, dst_pitch);
        };
    } else {
        this->pixel_depth = 8;
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_8bpp_indexed(dst_buf, dst_pitch);
        };
    }

    this->start_refresh_task();

    this->blank_on = false;
    this->crtc_on = true;
}

void TaosVideo::convert_frame_15bpp_indexed(uint8_t *dst_buf, int dst_pitch) {
    uint8_t     *src_row, *dst_row;
    uint16_t    c;
    uint32_t    pix;
    int         src_pitch;

    src_row = this->fb_ptr;
    dst_row = dst_buf;

    src_pitch = this->fb_pitch - 2 * this->active_width;
    dst_pitch = dst_pitch - 4 * this->active_width;

    for (int h = this->active_height; h > 0; h--) {
        for (int x = this->active_width; x > 0; x--) {
            c = READ_WORD_BE_A(src_row);
            pix = (this->palette[(c >> 10) & 0x1F] & 0x00FF0000) |
                  (this->palette[(c >>  5) & 0x1F] & 0x0000FF00) |
                  (this->palette[ c        & 0x1F] & 0xFF0000FF);
            WRITE_DWORD_LE_A(dst_row, pix);
            src_row += 2;
            dst_row += 4;
        }
        src_row += src_pitch;
        dst_row += dst_pitch;
    }
}

// ========================== Device registry stuff ==========================
static const PropMap Taos_Properties = {
    {"video_out", new StrProperty("NTSC", {"PAL", "NTSC", "VGA"})},
};

static const DeviceDescription Taos_Descriptor = {
    TaosVideo::create, {}, Taos_Properties
};

REGISTER_DEVICE(TaosVideo, Taos_Descriptor);
