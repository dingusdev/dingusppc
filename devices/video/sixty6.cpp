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

/** @file TNT on-board NTSC/PAL video output emulation. */

#include <devices/video/sixty6.h>
#include <devices/video/control.h>
#include <devices/common/i2c/saa7187.h>
#include <devices/common/machineid.h>
#include <devices/deviceregistry.h>
#include <machines/machinebase.h>

namespace loguru {
    enum : Verbosity {
        Verbosity_SIXTY6 = loguru::Verbosity_9,
        Verbosity_SIXTY6_EXTRA = loguru::Verbosity_9,
        Verbosity_SIXTY6_INTERRUPT = loguru::Verbosity_9
    };
}

Sixty6Video::Sixty6Video()
{
    // initialize the video clock generator
    this->saa7187 = std::unique_ptr<Saa7187VideoEncoder> (new Saa7187VideoEncoder(0x44));

    // register the video clock generator with the I2C host
    I2CBus* i2c_bus = dynamic_cast<I2CBus*>(gMachineObj->get_comp_by_type(HWCompType::I2C_HOST));
    i2c_bus->register_device(0x44, this->saa7187.get());

    // get (raw) pointer to the I/O controller
    GrandCentral* gc_obj = dynamic_cast<GrandCentral*>(gMachineObj->get_comp_by_name("GrandCentral"));

    // attach IOBus Device #3 0xF301C000 ; sixty6
    gc_obj->attach_iodevice(2, this);

    // attach IOBus Device #5 0xF301E000 ; sixty6 composite/s-video
    gMachineObj->add_device("BoardReg66", std::unique_ptr<BoardRegister>(
        new BoardRegister("Board Register 66",
            ((GET_BIN_PROP("has_svideo") ^ 1) << 6)    | // S-Video connected (active low)
            ((GET_BIN_PROP("has_composite") ^ 1) << 7) | // Composite Video connected (active low)
            0xFF3FU                                      // pull up unused bits
    )));
    gc_obj->attach_iodevice(4, dynamic_cast<BoardRegister*>(gMachineObj->get_comp_by_name("BoardReg66")));
}

uint16_t Sixty6Video::iodev_read(uint32_t address)
{
    uint16_t value;
    switch (address & 3) {

        case Sixty6BaseReg::CLUT_DATA:
            if (this->comp_index == 0) {
                uint8_t a;
                get_palette_color(this->clut_addr, clut_color[0], clut_color[1], clut_color[2], a);
            }
            value = this->clut_color[this->comp_index];
            LOG_F(SIXTY6, "Sixty6: read  %d:CLUT_DATA 0x%02x.%c = %02x", address, this->clut_addr, "rgb"[comp_index], value);
            this->comp_index++;
            if (this->comp_index >= 3) {
                this->clut_addr++; // auto-increment CLUT address
                this->comp_index = 0;
            }
            break;

        case Sixty6BaseReg::CLUT_ADDR:
            value = this->clut_addr;
            LOG_F(SIXTY6, "Sixty6: read  %d:CLUT_ADDR = %02x", address, value);
            break;

        case Sixty6BaseReg::CONTROL_DATA:
            switch (this->control_addr) {
                case Sixty6Reg::CONTROL_1:
                    value = this->control_1;
                    if (last_control_1_value == value) {
                        last_control_1_count++;
                    } else {
                        if (last_control_1_count) {
                            LOG_F(SIXTY6_INTERRUPT, "Sixty6: read  %d:CONTROL_1 = %02x x %d", address, last_control_1_value, last_control_1_count);
                        }
                        last_control_1_value = value;
                        last_control_1_count = 0;
                        LOG_F(SIXTY6_INTERRUPT, "Sixty6: read  %d:CONTROL_1 = %02x x %d", address, last_control_1_value, last_control_1_count);
                    }
                    break;
                default:
                    value = 0;
                    LOG_F(ERROR, "Sixty6: read  %d:CONTROL_DATA 0x%02x = %02x", address, this->control_addr, value);
            }
            break;

        case Sixty6BaseReg::CONTROL_ADDR:
            value = this->control_addr;
            LOG_F(SIXTY6_INTERRUPT, "Sixty6: read  %d:CONTROL_ADDR = %02x", address, value);
            break;
    }
    return value;
}

void Sixty6Video::iodev_write(uint32_t address, uint16_t value)
{
    switch (address & 3) {

        case Sixty6BaseReg::CLUT_DATA:
            if (this->comp_index == 0) {
                uint8_t a;
                get_palette_color(this->clut_addr, clut_color[0], clut_color[1], clut_color[2], a);
            }
            LOG_F(SIXTY6, "Sixty6: write %d:CLUT_DATA 0x%02x.%c = %02x", address, this->clut_addr, "rgb"[comp_index], value);
            this->clut_color[this->comp_index++] = value;
            if (this->comp_index >= 3) {
                this->set_palette_color(this->clut_addr, clut_color[0],
                                        clut_color[1], clut_color[2], 0xFF);
                this->clut_addr++; // auto-increment CLUT address
                this->comp_index = 0;
            }
            break;

        case Sixty6BaseReg::CLUT_ADDR:
            LOG_F(SIXTY6, "Sixty6: write %d:CLUT_ADDR = %02x", address, value);
            this->clut_addr = value;
            this->comp_index = 0;
            break;

        case Sixty6BaseReg::CONTROL_DATA:
            switch (this->control_addr) {
                case Sixty6Reg::REG_V1O_L:
                    this->v1_odd = (this->v1_odd & 0xff00) | (value & 0xff);
                    this->changed = true;
                    LOG_F(SIXTY6, "Sixty6: write %d:REG_V1O_L = %02x V1_odd = %d", address, value, this->v1_odd);
                    break;
                case Sixty6Reg::REG_V1O_H:
                    this->v1_odd = (value << 8) | (this->v1_odd & 0xff);
                    this->changed = true;
                    LOG_F(SIXTY6, "Sixty6: write %d:REG_V1O_H = %02x V1_odd = %d", address, value, this->v1_odd);
                    break;
                case Sixty6Reg::REG_V2O_L:
                    this->v2_odd = (this->v2_odd & 0xff00) | (value & 0xff);
                    this->changed = true;
                    LOG_F(SIXTY6, "Sixty6: write %d:REG_V2O_L = %02x V2_odd = %d", address, value, this->v2_odd);
                    break;
                case Sixty6Reg::REG_V2O_H:
                    this->v2_odd = (value << 8) | (this->v2_odd & 0xff);
                    this->changed = true;
                    LOG_F(SIXTY6, "Sixty6: write %d:REG_V2O_H = %02x V2_odd = %d", address, value, this->v2_odd);
                    break;
                case Sixty6Reg::REG_V1E_L:
                    this->v1_even = (this->v1_even & 0xff00) | (value & 0xff);
                    this->changed = true;
                    LOG_F(SIXTY6, "Sixty6: write %d:REG_V1E_L = %02x v1_even = %d", address, value, this->v1_even);
                    break;
                case Sixty6Reg::REG_V1E_H:
                    this->v1_even = (value << 8) | (this->v1_even & 0xff);
                    this->changed = true;
                    LOG_F(SIXTY6, "Sixty6: write %d:REG_V1E_H = %02x v1_even = %d", address, value, this->v1_even);
                    break;
                case Sixty6Reg::REG_V2E_L:
                    this->v2_even = (this->v2_even & 0xff00) | (value & 0xff);
                    this->changed = true;
                    LOG_F(SIXTY6, "Sixty6: write %d:REG_V2E_L = %02x v2_even = %d", address, value, this->v2_even);
                    break;
                case Sixty6Reg::REG_V2E_H:
                    this->v2_even = (value << 8) | (this->v2_even & 0xff);
                    this->changed = true;
                    LOG_F(SIXTY6, "Sixty6: write %d:REG_V2E_H = %02x v2_even = %d", address, value, this->v2_even);
                    break;
                case Sixty6Reg::REG_H1_L:
                    this->h1 = (this->h1 & 0xff00) | (value & 0xff);
                    this->changed = true;
                    LOG_F(SIXTY6, "Sixty6: write %d:REG_H1_L = %02x h1 = %d", address, value, this->h1);
                    break;
                case Sixty6Reg::REG_H1_H:
                    this->h1 = (value << 8) | (this->h1 & 0xff);
                    this->changed = true;
                    LOG_F(SIXTY6, "Sixty6: write %d:REG_H1_H = %02x h1 = %d", address, value, this->h1);
                    break;
                case Sixty6Reg::REG_H2_L:
                    this->h2 = (this->h2 & 0xff00) | (value & 0xff);
                    this->changed = true;
                    LOG_F(SIXTY6, "Sixty6: write %d:REG_H2_L = %02x h2 = %d", address, value, this->h2);
                    break;
                case Sixty6Reg::REG_H2_H:
                    this->h2 = (value << 8) | (this->h2 & 0xff);
                    this->changed = true;
                    LOG_F(SIXTY6, "Sixty6: write %d:REG_H2_H = %02x h2 = %d", address, value, this->h2);
                    break;
                case Sixty6Reg::BASE_ADDR_L:
                    this->base_addr = (this->base_addr & 0xffff00) | (value & 0xff);
                    this->changed = true;
                    LOG_F(SIXTY6, "Sixty6: write %d:BASE_ADDR_L = %02x base_addr = %06x", address, value, this->base_addr);
                    break;
                case Sixty6Reg::BASE_ADDR_M:
                    this->base_addr = (value << 8) | (this->base_addr & 0xff00ff);
                    this->changed = true;
                    LOG_F(SIXTY6, "Sixty6: write %d:BASE_ADDR_M = %02x base_addr = %06x", address, value, this->base_addr);
                    break;
                case Sixty6Reg::BASE_ADDR_H:
                    this->base_addr = (value << 16) | (this->base_addr & 0x00ffff);
                    this->changed = true;
                    LOG_F(SIXTY6, "Sixty6: write %d:BASE_ADDR_H = %02x base_addr = %06x", address, value, this->base_addr);
                    break;
                case Sixty6Reg::PITCH_L:
                    this->pitch = (this->pitch & 0xff00) | (value & 0xff);
                    this->changed = true;
                    LOG_F(SIXTY6, "Sixty6: write %d:PITCH_L = %02x pitch = %d", address, value, this->pitch);
                    break;
                case Sixty6Reg::PITCH_H:
                    this->pitch = (value << 8) | (this->pitch & 0xff);
                    this->changed = true;
                    LOG_F(SIXTY6, "Sixty6: write %d:PITCH_H = %02x pitch = %d", address, value, this->pitch);
                    break;
                case Sixty6Reg::CURSOR_X_L:
                    this->cursor_x = (this->cursor_x & 0xff00) | (value & 0xff);
                    LOG_F(SIXTY6, "Sixty6: write %d:CURSOR_X_L = %02x cursor_x = %d", address, value, this->cursor_x);
                    break;
                case Sixty6Reg::CURSOR_X_H:
                    this->cursor_x = (value << 8) | (this->cursor_x & 0xff);
                    LOG_F(SIXTY6, "Sixty6: write %d:CURSOR_X_H = %02x cursor_x = %d", address, value, this->cursor_x);
                    break;
                case Sixty6Reg::CURSOR_Y_L:
                    this->cursor_y = (this->cursor_y & 0xff00) | (value & 0xff);
                    LOG_F(SIXTY6, "Sixty6: write %d:CURSOR_Y_L = %02x cursor_y = %d", address, value, this->cursor_y);
                    break;
                case Sixty6Reg::CURSOR_Y_H:
                    this->cursor_y = (value << 8) | (this->cursor_y & 0xff);
                    LOG_F(SIXTY6, "Sixty6: write %d:CURSOR_Y_H = %02x cursor_y = %d", address, value, this->cursor_y);
                    break;
                case Sixty6Reg::INT_COUNT_L:
                    this->int_count = (this->int_count & 0xff00) | (value & 0xff);
                    LOG_F(SIXTY6, "Sixty6: write %d:INT_COUNT_L = %02x int_count = %d", address, value, this->int_count);
                    break;
                case Sixty6Reg::INT_COUNT_H:
                    this->int_count = (value << 8) | (this->int_count & 0xff);
                    LOG_F(SIXTY6, "Sixty6: write %d:INT_COUNT_H = %02x int_count = %d", address, value, this->int_count);
                    break;
                case Sixty6Reg::CONTROL_1:
                {
                    if (this->changed) {
                        if (value & 2) {
                            LOG_F(SIXTY6, "Sixty6: write %d:CONTROL_1 = %02x -> %02x", address, value, value & ~2);
                        } else {
                            LOG_F(SIXTY6, "Sixty6: write %d:CONTROL_1 = %02x", address, value);
                        }
                    }
                    else {
                        if (value & 2) {
                            LOG_F(SIXTY6_INTERRUPT, "Sixty6: write %d:CONTROL_1 = %02x -> %02x", address, value, value & ~2);
                        } else {
                            LOG_F(SIXTY6_INTERRUPT, "Sixty6: write %d:CONTROL_1 = %02x", address, value);
                        }
                    }
                    this->control_1 = value & ~2; // we don't support hardware cursor

                    if (this->control_1 & 0x40) {
                        this->interrupt_enabled = true;
                    }
                    else {
                        /*
                            If you disable interrupts then startup may hang which means we
                            aren't doing interrupts properly.

                            We need interrupts to move mouse while it is positioned on the
                            Sixty6 display.
                        */
                        // this->interrupt_enabled = false;
                    }

                    int new_pixel_format = (this->control_1 >> 4) & 3;
                    if (new_pixel_format != this->pixel_format)
                        this->changed = true;

                    if (1 || (value & 0x49) == 0x49) {
                        if (this->changed) {
                            this->enable_display();
                            this->changed = false;
                        }
                    }
                    else {
                        //this->disable_display();
                    }
                    break;
                }
                case Sixty6Reg::CONTROL_2:
                    this->control_2 = value;
                    LOG_F(SIXTY6, "Sixty6: write %d:CONTROL_2 = %02x", address, value);
                    break;
                default:
                    LOG_F(ERROR, "Sixty6: write %d:CONTROL_DATA 0x%02x = %02x", address, this->control_addr, value);
            }
            break;

        case Sixty6BaseReg::CONTROL_ADDR:
            LOG_F(SIXTY6_EXTRA, "Sixty6: write %d:CONTROL_ADDR = %02x", address, value);
            this->control_addr = value;
            break;
    }
}

void Sixty6Video::enable_display()
{
    int new_width, new_height, clk_divisor;

    // get pixel frequency from Saa7187
    this->pixel_clock = 25000000; // this->saa7187->get_dot_freq();

    // calculate active_width and active_height from video timing parameters
    new_width  = this->h2 - this->h1;
    new_height = (this->v2_odd - this->v1_odd + this->v2_even - this->v1_even + 1) & ~1;

    if (this->control_1 & 4) {
        // zoom mode
        new_width >>= 1;
        new_height >>= 1;
    }

    this->active_width  = new_width;
    this->active_height = new_height;

    // set framebuffer parameters
    this->fb_ptr = &control_video->GetVram()[(this->base_addr << 3) & 0x3fffff]; // why does (this->base_addr << 3) == 0x4C00C20?
    this->fb_pitch = this->pitch << 3;
    this->pixel_format = (this->control_1 >> 4) & 3;

    // get pixel depth
    switch (this->pixel_format) {
    case 1:
        this->pixel_depth = 8;
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_8bpp_indexed(dst_buf, dst_pitch);
        };
        break;
    case 2:
        this->pixel_depth = 16;
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_15bpp_BE(dst_buf, dst_pitch);
        };
        break;
    default:
        LOG_F(ERROR, "Sixty6: Invalid pixel format %d!", this->pixel_format);
        // fallthrough
    case 3:
        this->pixel_depth = 32;
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_32bpp_BE(dst_buf, dst_pitch);
        };
        break;
    }

    // calculate display refresh rate
    if (this->v2_odd > 256)
        this->refresh_rate = 50.0;
    else
        this->refresh_rate = 60.0;

    this->hori_blank = this->h1;
    this->vert_blank = this->v1_odd + this->v1_even;

    this->hori_total = this->h2;
    this->vert_total = this->v2_odd + v2_even;

    this->stop_refresh_task();

    // set up periodic timer for display updates
    if (this->active_width > 0 && this->active_height > 0 && this->pixel_clock > 0) {
        LOG_F(INFO, "Sixty6: refresh rate set to %f Hz", this->refresh_rate);

        this->start_refresh_task();

        this->blank_on = false;

        LOG_F(INFO, "Sixty6: display enabled");
        this->crtc_on = true;
    }
    else {
        LOG_F(INFO, "Sixty6: display not enabled");
        this->blank_on = true;
        this->crtc_on = false;
    }
}

void Sixty6Video::disable_display()
{
    this->crtc_on = false;
    LOG_F(INFO, "Sixty6: display disabled");
}

int Sixty6Video::device_postinit()
{
    this->int_ctrl = dynamic_cast<InterruptCtrl*>(
        gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
    this->irq_id = this->int_ctrl->register_dev_int(IntSrc::SIXTY6);

    this->vbl_cb = [this](uint8_t irq_line_state) {
        if (irq_line_state)
            this->control_1 |= 0x80;
        else
            this->control_1 &= ~0x80;
        LOG_F(SIXTY6_INTERRUPT, "Sixty6: interrupt state:%d doit:%d", irq_line_state, ((this->control_1 & 0x40) && this->crtc_on));

        if (this->interrupt_enabled && this->crtc_on) {
            //this->pci_interrupt(irq_line_state);
            this->int_ctrl->ack_int(this->irq_id, irq_line_state);
        }
    };

    this->control_video = dynamic_cast<ControlVideo*>(
        gMachineObj->get_comp_by_name("ControlVideo"));

    return 0;
}

// ========================== Device registry stuff ==========================

static const PropMap Sixty6_Properties = {
    {"has_composite",
        new BinProperty(0)},
    {"has_svideo",
        new BinProperty(0)},
};

static const DeviceDescription Sixty6_Descriptor = {
    Sixty6Video::create, {}, Sixty6_Properties
};

REGISTER_DEVICE(Sixty6Video, Sixty6_Descriptor);
