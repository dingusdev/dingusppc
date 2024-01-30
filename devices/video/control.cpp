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

/** @file TNT on-board video output emulation. */

/** TNT on-board video comprises several components:
    - Chaos ASIC that provides data bus buffering between the video subsystem
      and the processor bus
    - Control ASIC that provides addressing and control for the video subsystem
    - RaDACal RAMDAC ASIC for generating RGB video stream to the monitor
    - Athens clock generator for generating pixel clock

    Some TNT boards can generate composite video output and thus include
    two additional components:
    - Sixty6 ASIC that converts RGB pixels stored in the VRAM to YUV color space
    - SAA7187 encoder that converts pixels from Sixty6 to composite video signal

    Kudos to joevt#3510 for his precious technical help and HW hacking.
 */

#include <devices/common/i2c/athens.h>
#include <devices/common/i2c/i2c.h>
#include <devices/deviceregistry.h>
#include <devices/ioctrl/macio.h>
#include <devices/video/control.h>
#include <endianswap.h>
#include <loguru.hpp>
#include <machines/machinebase.h>
#include <machines/machineproperties.h>
#include <memaccess.h>

#include <cinttypes>

namespace loguru {
    enum : Verbosity {
        Verbosity_RADACAL = loguru::Verbosity_INFO,
        Verbosity_CONTROL = loguru::Verbosity_INFO
    };
}

ControlVideo::ControlVideo()
    : PCIDevice("Control-Video"), VideoCtrlBase(640, 480)
{
    supports_types(HWCompType::PCI_HOST | HWCompType::PCI_DEV);

    // get VRAM size in MBs and convert it to bytes
    this->vram_size = GET_INT_PROP("gfxmem_size") << 20;

    // calculate number of VRAM banks from VRAM size
    this->num_banks = this->vram_size >> 21; // 2 MB => 1 bank, 4 MB >> 2 banks

    // allocate VRAM
    this->vram_ptr = std::unique_ptr<uint8_t[]> (new uint8_t[this->vram_size]);

    // set up PCI configuration space header
    this->vendor_id   = PCI_VENDOR_APPLE;
    this->device_id   = 3;
    this->class_rev   = 0;

    this->setup_bars({
        {0, 0xFFFFFFFFUL}, // I/O region (4 bytes but it's weird because bit 1 is set)
        {1, 0xFFFFF000UL}, // base address for the HW registers (4KB)
        {2, 0xFC000000UL}  // base address for the VRAM (64MB)
    });

    this->pci_notify_bar_change = [this](int bar_num) {
        this->notify_bar_change(bar_num);
    };

    // initialize the video clock generator
    this->clk_gen = std::unique_ptr<AthensClocks> (new AthensClocks(0x28));

    // register the video clock generator with the I2C host
    I2CBus* i2c_bus = dynamic_cast<I2CBus*>(gMachineObj->get_comp_by_type(HWCompType::I2C_HOST));
    i2c_bus->register_device(0x28, this->clk_gen.get());

    // attach RAMDAC
    this->radacal = std::unique_ptr<AppleRamdac>(new AppleRamdac(DacFlavour::RADACAL));
    this->radacal->get_clut_entry_cb = [this](uint8_t index, uint8_t *colors) {
        uint8_t a;
        this->get_palette_color(index, colors[0], colors[1], colors[2], a);
    };
    this->radacal->set_clut_entry_cb = [this](uint8_t index, uint8_t *colors) {
        this->set_palette_color(index, colors[0], colors[1], colors[2], 0xFF);
    };
    this->radacal->cursor_ctrl_cb = [this](bool cursor_on) {
        if (cursor_on) {
            this->radacal->measure_hw_cursor(this->fb_ptr - 16);
            this->cursor_ovl_cb = [this](uint8_t *dst_buf, int dst_pitch) {
                this->radacal->draw_hw_cursor(this->fb_ptr - 16,
                    dst_buf, dst_pitch);
            };
        } else {
            this->cursor_ovl_cb = nullptr;
        }
    };

    // attach IOBus Device #2 0xF301B000 ; register RaDACal with the I/O controller
    GrandCentral* gc_obj = dynamic_cast<GrandCentral*>(gMachineObj->get_comp_by_name("GrandCentral"));
    gc_obj->attach_iodevice(1, this->radacal.get());

    // initialize display identification
    this->display_id = std::unique_ptr<DisplayID> (new DisplayID());
}

void ControlVideo::notify_bar_change(int bar_num) {
    switch (bar_num) {
    case 0:
        this->io_base = this->bars[bar_num] & ~3;
        LOG_F(INFO, "Control: I/O space address set to 0x%08X", this->io_base);
        break;
    case 1:
        if (this->regs_base != (this->bars[bar_num] & 0xFFFFFFF0UL)) {
            this->regs_base = this->bars[bar_num] & 0xFFFFFFF0UL;
            this->host_instance->pci_register_mmio_region(this->regs_base,
                0x1000, this);
            LOG_F(INFO, "Control: register aperture set to 0x%08X", this->regs_base);
        }
        break;
    case 2:
        if (this->vram_base != (this->bars[bar_num] & 0xFFFFFFF0UL)) {
            this->vram_base = this->bars[bar_num] & 0xFFFFFFF0UL;
            this->host_instance->pci_register_mmio_region(this->vram_base,
                0x04000000, this);
            LOG_F(INFO, "Control: VRAM aperture set to 0x%08X", this->vram_base);
        }
        break;
    }
}

int ControlVideo::device_postinit() {
    this->int_ctrl = dynamic_cast<InterruptCtrl*>(
        gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
    this->irq_id = this->int_ctrl->register_dev_int(IntSrc::CONTROL);

    this->vbl_cb = [this](uint8_t irq_line_state) {
        if (irq_line_state != !!(this->int_status & VBL_IRQ_STAT)) {
            if (irq_line_state)
                this->int_status |= VBL_IRQ_STAT;
            else
                this->int_status &= ~VBL_IRQ_STAT;

            if (this->int_enable & VBL_IRQ_EN)
                this->int_ctrl->ack_int(this->irq_id, irq_line_state);
        }
    };

    return 0;
}

static const char * get_name_controlreg(int offset) {
    switch (offset >> 4) {
    case ControlRegs::CUR_LINE      : return "CUR_LINE";
    case ControlRegs::VFPEQ         : return "VFPEQ";
    case ControlRegs::VFP           : return "VFP";
    case ControlRegs::VAL           : return "VAL";
    case ControlRegs::VBP           : return "VBP";
    case ControlRegs::VBPEQ         : return "VBPEQ";
    case ControlRegs::VSYNC         : return "VSYNC";
    case ControlRegs::VHLINE        : return "VHLINE";
    case ControlRegs::PIPE_DELAY    : return "PIPE_DELAY";
    case ControlRegs::HPIX          : return "HPIX";
    case ControlRegs::HFP           : return "HFP";
    case ControlRegs::HAL           : return "HAL";
    case ControlRegs::HBWAY         : return "HBWAY";
    case ControlRegs::HSP           : return "HSP";
    case ControlRegs::HEQ           : return "HEQ";
    case ControlRegs::HLFLN         : return "HLFLN";
    case ControlRegs::HSERR         : return "HSERR";
    case ControlRegs::CNTTST        : return "CNTTST";
    case ControlRegs::SWATCH_CTRL   : return "SWATCH_CTRL";
    case ControlRegs::GBASE         : return "GBASE";
    case ControlRegs::ROW_WORDS     : return "ROW_WORDS";
    case ControlRegs::MON_SENSE     : return "MON_SENSE";
    case ControlRegs::MISC_ENABLES  : return "MISC_ENABLES";
    case ControlRegs::GSC_DIVIDE    : return "GSC_DIVIDE";
    case ControlRegs::REFRESH_COUNT : return "REFRESH_COUNT";
    case ControlRegs::INT_ENABLE    : return "INT_ENABLE";
    case ControlRegs::INT_STATUS    : return "INT_STATUS";
    default                         : return "unknown";
    }
}

uint32_t ControlVideo::read(uint32_t rgn_start, uint32_t offset, int size)
{
    if (rgn_start == this->vram_base) {
        if (offset >= 0x800000) {
            // HACK: writing to VRAM in 128bit mode with only the standard
            // bank populated seems to replicate the first 64bit portion of data
            // in the second 64bit portion. This "feature" is used by
            // the Mac OS driver to detect how much physical VRAM is installed.
            // I handle this case here because reads from VRAM seem to happen
            // far less frequently than writes.
            if ((this->enables & VRAM_WIDE_MODE) && this->num_banks == 1)
                offset &= ~8UL;
            return read_mem(&this->vram_ptr[offset & 0x3FFFFF], size);
        }

        LOG_F(ERROR, "%s: read from unmapped aperture address 0x%X", this->name.c_str(),
              this->vram_base + offset);
        return 0;
    }

    uint32_t value;

    if (rgn_start == this->regs_base) {
        switch (offset >> 4) {
        case ControlRegs::CUR_LINE:
            value = 0; // current active video line should relate this to refresh rate
            LOG_F(ERROR, "Control: read  CUR_LINE %03x", offset);
            break;
        case ControlRegs::VFPEQ:
        case ControlRegs::VFP:
        case ControlRegs::VAL:
        case ControlRegs::VBP:
        case ControlRegs::VBPEQ:
        case ControlRegs::VSYNC:
        case ControlRegs::VHLINE:
        case ControlRegs::PIPE_DELAY:
        case ControlRegs::HPIX:
        case ControlRegs::HFP:
        case ControlRegs::HAL:
        case ControlRegs::HBWAY:
        case ControlRegs::HSP:
        case ControlRegs::HEQ:
        case ControlRegs::HLFLN:
        case ControlRegs::HSERR:
            value = this->swatch_params[(offset >> 4) - ControlRegs::VFPEQ];
            break;
        case ControlRegs::CNTTST:
            value = 0;
            break;
        case ControlRegs::SWATCH_CTRL:
            value = this->swatch_ctrl;
            break;
        case ControlRegs::GBASE:
            value = this->fb_base;
            break;
        case ControlRegs::ROW_WORDS:
            value = this->row_words;
            break;
        case ControlRegs::MON_SENSE:
            value = this->cur_mon_id << 6;
            break;
        case ControlRegs::MISC_ENABLES:
            value = this->enables;
            break;
        case ControlRegs::GSC_DIVIDE:
            value = this->clock_divider;
            break;
        case ControlRegs::REFRESH_COUNT:
            value = 0;
            break;
        case ControlRegs::INT_STATUS:
            value = this->int_status;
            break;
        case ControlRegs::INT_ENABLE:
            value = this->int_enable;
            break;
        default:
            LOG_F(ERROR, "Control: read  %03x", offset);
            value = 0;
        }

        AccessDetails details;
        details.size = size;
        details.offset = offset & 3;
        uint32_t result = pci_conv_rd_data(value, value, details);
        if ((offset & 3) || (size != 4)) {
            LOG_F(WARNING, "%s: read  %s %03x.%c = %08x -> %0*x", this->name.c_str(), get_name_controlreg(offset), offset, SIZE_ARG(size), value, size * 2, result);
        }

        return result;
    }

    return 0;
}

void ControlVideo::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    if (rgn_start == this->vram_base) {
        if (offset >= 0x800000) {
            write_mem(&this->vram_ptr[offset & 0x3FFFFF], value, size);
        } else {
            LOG_F(ERROR, "%s: write to unmapped aperture address 0x%X", this->name.c_str(),
                  this->vram_base + offset);
        }
        return;
    }

    if (rgn_start == this->regs_base) {
        value = BYTESWAP_32(value);

        switch (offset >> 4) {
        case ControlRegs::PIPE_DELAY:
            this->swatch_params[(offset >> 4) - ControlRegs::VFPEQ] = value & 0x3FF;
            break;
        case ControlRegs::HEQ:
            this->swatch_params[(offset >> 4) - ControlRegs::VFPEQ] = value & 0xFFU;
            break;
        case ControlRegs::VFPEQ:
        case ControlRegs::VFP:
        case ControlRegs::VAL:
        case ControlRegs::VBP:
        case ControlRegs::VBPEQ:
        case ControlRegs::VSYNC:
        case ControlRegs::VHLINE:
        case ControlRegs::HPIX:
        case ControlRegs::HFP:
        case ControlRegs::HAL:
        case ControlRegs::HBWAY:
        case ControlRegs::HSP:
        case ControlRegs::HLFLN:
        case ControlRegs::HSERR:
            this->swatch_params[(offset >> 4) - ControlRegs::VFPEQ] = value & 0xFFF;
            break;
        case ControlRegs::CNTTST:
            if (value)
                LOG_F(WARNING, "%s: CNTTST set to 0x%X", this->name.c_str(), value);
            break;
        case ControlRegs::SWATCH_CTRL:
            if ((this->swatch_ctrl ^ value) & DISABLE_TIMING) {
                this->swatch_ctrl = value;
                this->strobe_counter = 0;
            } else if ((this->swatch_ctrl ^ value) & RESET_TIMING) {
                this->swatch_ctrl = value;
                if (value & RESET_TIMING) { // count 0-to-1 transitions
                    this->strobe_counter++;
                    if (this->strobe_counter >= 2) {
                        if (value & DISABLE_TIMING)
                            disable_display();
                        else
                            enable_display();
                    }
                }
            } else
                this->swatch_ctrl = value;
            break;
        case ControlRegs::GBASE:
            this->fb_base = value & 0x3FFFE0;
            break;
        case ControlRegs::ROW_WORDS:
            this->row_words = value & 0x7FE0;
            break;
        case ControlRegs::MON_SENSE: {
                uint8_t dirs   = ((value >> 3) & 7) ^ 7;
                uint8_t levels = ((value & 7) & dirs) | (dirs ^ 7);
                this->cur_mon_id = this->display_id->read_monitor_sense(levels, dirs);
            }
            break;
        case ControlRegs::MISC_ENABLES:
            if ((this->enables ^ value) & BLANK_DISABLE) {
                if (value & BLANK_DISABLE)
                    this->blank_on = false;
                else {
                    this->blank_on = true;
                    this->blank_display();
                }
            }
            this->enables = value;
            if (this->enables & FB_ENDIAN_LITTLE)
                ABORT_F("%s: little-endian framebuffer is not implemented yet",
                        this->name.c_str());
            break;
        case ControlRegs::GSC_DIVIDE:
            this->clock_divider = value & 3;
            break;
        case ControlRegs::REFRESH_COUNT:
            LOG_F(9, "Control: VRAM refresh count set to %d", value);
            break;
        case ControlRegs::INT_ENABLE:
            if ((this->int_enable ^ value) & VBL_IRQ_CLR) {
                // clear VBL IRQ on a 1-to-0 transition of INT_ENABLE[VBL_IRQ_CLR]
                if (!(value & VBL_IRQ_CLR))
                    this->vbl_cb(0);
            }
            this->int_enable = value & 0x0F;
            break;
        default:
            LOG_F(ERROR, "Control: write %03x = %0*x", offset, size * 2, value);
        }
    }
}

uint8_t* ControlVideo::GetVram()
{
    return &this->vram_ptr[0];
}

void ControlVideo::enable_display()
{
    int new_width, new_height, clk_divisor;

    // get pixel frequency from Athens
    this->pixel_clock = this->clk_gen->get_dot_freq();

    // get RaDACal clock divisor
    clk_divisor = this->radacal->get_clock_div();

    // calculate active_width and active_height from video timing parameters
    new_width  = swatch_params[ControlRegs::HFP-1] - swatch_params[ControlRegs::HAL-1];
    new_height = swatch_params[ControlRegs::VFP-1] - swatch_params[ControlRegs::VAL-1];

    new_width *= clk_divisor;
    if (this->enables & SCAN_CONTROL) {
        new_height >>= 1;
    }

    this->active_width  = new_width;
    this->active_height = new_height;

    // set framebuffer parameters
    this->fb_ptr   = &this->vram_ptr[this->fb_base];
    this->fb_pitch = this->row_words;

    this->pixel_depth = this->radacal->get_pix_width();
    if (swatch_params[ControlRegs::HAL-1] != swatch_params[ControlRegs::PIPE_DELAY-1] + 1 || this->pixel_depth == 32) {
        // don't know how to calculate offset from GBASE (fb_base); it is always hard coded as + 16 in the ndrv.
        this->fb_ptr += 16; // first 16 bytes are for 4 bpp HW cursor
    }
    else {
        /*
         Open Firmware frame buffer has these properties:
         - GBASE == 0 // no offset from vram_ptr
         - fb_ptr == vram_ptr // no offset from GBASE
         - active_width == ROW_WORDS (row_words) // no offset between rows
         - HAL == PIPE_DELAY + 1
         - depth_mode = 0 // 8 bit indexed
         */
    }

    // get pixel depth from RaDACal
    switch (this->pixel_depth) {
    case 8:
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_8bpp_indexed(dst_buf, dst_pitch);
        };
        break;
    case 16:
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_15bpp_BE(dst_buf, dst_pitch);
        };
        break;
    case 32:
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_32bpp_BE(dst_buf, dst_pitch);
        };
        break;
    default:
        LOG_F(ERROR, "RaDACal: Invalid pixel depth code!");
    }

    // calculate display refresh rate
    this->hori_blank = swatch_params[ControlRegs::HAL-1] +
        (swatch_params[ControlRegs::HSP-1] - swatch_params[ControlRegs::HFP-1]);

    this->hori_blank *= clk_divisor;

    this->vert_blank = swatch_params[ControlRegs::VAL-1] +
        (swatch_params[ControlRegs::VSYNC-1] - swatch_params[ControlRegs::VFP-1]);

    if (this->enables & SCAN_CONTROL) {
        this->vert_blank >>= 1;
    }

    this->hori_total = this->hori_blank + new_width;
    this->vert_total = this->vert_blank + new_height;

    this->radacal->set_fb_parameters(active_width, active_height, this->fb_pitch);

    this->stop_refresh_task();

    // set up periodic timer for display updates
    if (this->active_width > 0 && this->active_height > 0 && this->pixel_clock > 0) {
        this->refresh_rate = (double)(this->pixel_clock) / (this->hori_total * this->vert_total);
        LOG_F(INFO, "Control: refresh rate set to %f Hz", this->refresh_rate);

        this->start_refresh_task();

        this->blank_on = false;

        LOG_F(CONTROL, "Control: display enabled");
        this->crtc_on = true;
    }
    else {
        LOG_F(CONTROL, "Control: display not enabled");
        this->blank_on = true;
        this->crtc_on = false;
    }
}

void ControlVideo::disable_display()
{
    this->crtc_on = false;
    LOG_F(INFO, "Control: display disabled");
}

// ========================== Device registry stuff ==========================

static const PropMap Control_Properties = {
    {"gfxmem_size",
        new IntProperty(  2, vector<uint32_t>({2, 4}))},
    {"mon_id",
        new StrProperty("AppleVision1710")},
};

static const DeviceDescription Control_Descriptor = {
    ControlVideo::create, {}, Control_Properties
};

REGISTER_DEVICE(ControlVideo, Control_Descriptor);
