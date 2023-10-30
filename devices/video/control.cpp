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

/** @file TNT on-board video output emulation. */

/** TNT on-board video comprises several components:
    - Chaos ASIC that provides data bus buffering between the video subsystem
      and the processor bus
    - Control ASIC that provides addressing and control for the video subsystem
    - RaDACal RAMDAC ASIC for generating video stream to the monitor
    - Athens clock generator for generating pixel clock

    Kudos to joevt#3510 for his precious technical help and HW hacking.
 */

#include <debugger/backtrace.h>
#include <devices/common/i2c/athens.h>
#include <devices/common/i2c/i2c.h>
#include <devices/deviceregistry.h>
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

    // attach IOBus Device #2 0xF301B000 ; register RaDACal with the I/O controller
    GrandCentral* gc_obj = dynamic_cast<GrandCentral*>(gMachineObj->get_comp_by_name("GrandCentral"));
    gc_obj->attach_iodevice(1, this);

    // initialize display identification
    this->display_id = std::unique_ptr<DisplayID> (new DisplayID());
}

void ControlVideo::notify_bar_change(int bar_num)
{
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
    case ControlRegs::PIPED         : return "PIPED";
    case ControlRegs::HPIX          : return "HPIX";
    case ControlRegs::HFP           : return "HFP";
    case ControlRegs::HAL           : return "HAL";
    case ControlRegs::HBWAY         : return "HBWAY";
    case ControlRegs::HSP           : return "HSP";
    case ControlRegs::HEQ           : return "HEQ";
    case ControlRegs::HLFLN         : return "HLFLN";
    case ControlRegs::HSERR         : return "HSERR";
    case ControlRegs::CNTTST        : return "CNTTST";
    case ControlRegs::TEST          : return "TEST";
    case ControlRegs::GBASE         : return "GBASE";
    case ControlRegs::ROW_WORDS     : return "ROW_WORDS";
    case ControlRegs::MON_SENSE     : return "MON_SENSE";
    case ControlRegs::ENABLE        : return "ENABLE";
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
            return read_mem(&this->vram_ptr[offset - 0x800000], size);
        }
    
        LOG_F(INFO, "Control: little-endian access to VRAM not supported yet");
        return 0;
    }

    uint32_t value;

    if (rgn_start == this->regs_base) {
        switch (offset >> 4) {
        case ControlRegs::CUR_LINE:
            value = 0; // current active video line should relate this to refresh rate
            LOG_F(ERROR, "Control: read  CUR_LINE %03x.%c", offset, SIZE_ARG(size));
            break;
        case ControlRegs::VFPEQ:
        case ControlRegs::VFP:
        case ControlRegs::VAL:
        case ControlRegs::VBP:
        case ControlRegs::VBPEQ:
        case ControlRegs::VSYNC:
        case ControlRegs::VHLINE:
        case ControlRegs::PIPED:
        case ControlRegs::HPIX:
        case ControlRegs::HFP:
        case ControlRegs::HAL:
        case ControlRegs::HBWAY:
        case ControlRegs::HSP:
        case ControlRegs::HEQ:
        case ControlRegs::HLFLN:
        case ControlRegs::HSERR:
            value = this->swatch_params[(offset >> 4) - ControlRegs::VFPEQ];
            LOG_F(CONTROL, "Control: read  %s %03x.%c = %0*x", get_name_controlreg(offset), offset, SIZE_ARG(size), size * 2, value);
            break;
        case ControlRegs::CNTTST:
            value = 0;
            LOG_F(ERROR, "Control: read  CNTTST %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            break;
        case ControlRegs::TEST:
            value = this->test;
            LOG_F(CONTROL, "Control: read  TEST %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            break;
        case ControlRegs::GBASE:
            value = this->fb_base;
            LOG_F(CONTROL, "Control: read  GBASE %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            break;
        case ControlRegs::ROW_WORDS:
            value = this->row_words;
            LOG_F(CONTROL, "Control: read  ROW_WORDS %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            break;
        case ControlRegs::MON_SENSE:
            value = this->cur_mon_id << 6;
            LOG_F(CONTROL, "Control: read  MON_SENSE %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            break;
        case ControlRegs::ENABLE:
            value = this->flags;
            LOG_F(CONTROL, "Control: read  ENABLE %03x.%c", offset, SIZE_ARG(size));
            break;
        case ControlRegs::GSC_DIVIDE:
            value = this->clock_divider;
            LOG_F(CONTROL, "Control: read  GSC_DIVIDE %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            break;
        case ControlRegs::REFRESH_COUNT:
            value = 0;
            LOG_F(ERROR, "Control: read  CNTTST %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            break;
        case ControlRegs::INT_STATUS:
            value = this->int_status;
            if (value != this->last_int_status) {
                LOG_F(CONTROL, "Control: read  (previous %d times) INT_STATUS %03x.%c = %0*x", last_int_status_read_count, offset, SIZE_ARG(size), size * 2, value);
                this->last_int_status = value;
                this->last_int_status_read_count = 0;
            }
            else {
                this->last_int_status_read_count++;
            
            }
            break;
        case ControlRegs::INT_ENABLE:
            value = this->int_enable;
            LOG_F(CONTROL, "Control: read  INT_ENABLE %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            break;
        default:
            LOG_F(ERROR, "Control: read  %03x.%c", offset, SIZE_ARG(size));
            value = 0;
        }

        AccessDetails details;
        details.size = size;
        details.offset = offset & 3;
        uint32_t result = pci_conv_rd_data(value, value, details);
        if ((offset & 3) || (size != 4)) {
            LOG_F(WARNING, "Control: read  %03x.%c = %08x -> %0*x", offset, SIZE_ARG(size), value, size * 2, result);
            //dump_backtrace();
        }

        return result;
    }

    return 0;
}

void ControlVideo::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    if (rgn_start == this->vram_base) {
        if (offset >= 0x800000) {
            write_mem(&this->vram_ptr[offset - 0x800000], value, size);
        } else {
            LOG_F(INFO, "Control: little-endian access to VRAM not supported yet");
        }
        return;
    }

    if (rgn_start == this->regs_base) {
        value = BYTESWAP_32(value);

        switch (offset >> 4) {
        case ControlRegs::PIPED:
            this->swatch_params[(offset >> 4) - ControlRegs::VFPEQ] = value & 0x3ff;
            if (value & ~0x3ff)
                LOG_F(ERROR, "Control: write PIPED %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            else
                LOG_F(CONTROL, "Control: write PIPED %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            if (this->display_enabled) {
                this->enable_display();
            }
            break;
        case ControlRegs::HEQ:
            this->swatch_params[(offset >> 4) - ControlRegs::VFPEQ] = value & 0xff;
            if (value & ~0xff)
                LOG_F(ERROR, "Control: write HEQ %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            else
                LOG_F(CONTROL, "Control: write HEQ %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            if (this->display_enabled) {
                this->enable_display();
            }
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
            this->swatch_params[(offset >> 4) - ControlRegs::VFPEQ] = value & 0xfff;
            if (value & ~0xfff)
                LOG_F(ERROR, "Control: write %s %03x.%c = %0*x", get_name_controlreg(offset), offset, SIZE_ARG(size), size * 2, value);
            else
                LOG_F(CONTROL, "Control: write %s %03x.%c = %0*x", get_name_controlreg(offset), offset, SIZE_ARG(size), size * 2, value);
            if (this->display_enabled) {
                this->enable_display();
            }
            break;
        case ControlRegs::CNTTST:
            if (value != 0)
                LOG_F(ERROR, "Control: write CNTTST %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            else
                LOG_F(CONTROL, "Control: write CNTTST %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            break;
        case ControlRegs::TEST:
            if (value & ~0x7ff)
                LOG_F(ERROR, "Control: write TEST %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            else
                LOG_F(CONTROL, "Control: write TEST %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            value &= 0x7ff;
            if (this->test != value) {
                if ((this->test & ~TEST_STROBE & 0x400) != (value & ~TEST_STROBE & 0x400)) {
                    this->test = value;
                    this->test_shift = 0;
                    LOG_F(CONTROL, "New TEST value: 0x%08X", this->test);
                } else {
                    LOG_F(CONTROL, "TEST strobe bit flipped, new value: 0x%08X", value);
                    this->test = value;
                    if (++this->test_shift >= 3) {
                        LOG_F(CONTROL, "Received TEST reg value: 0x%08X", this->test & ~TEST_STROBE);
                        if ((this->test ^ this->prev_test) & 0x400) {
                            if ((this->display_enabled = !(this->test & 0x400))) {
                                this->enable_display();
                            } else {
                                this->disable_display();
                            }
                            this->prev_test = this->test;
                        }
                    }
                }
            }
            break;
        case ControlRegs::GBASE:
            if (value & ~0x3fffe0)
                LOG_F(ERROR, "Control: write GBASE %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            else
                LOG_F(CONTROL, "Control: write GBASE %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            this->fb_base = value & 0x3fffe0;
            if (this->display_enabled) {
                this->enable_display();
            }
            break;
        case ControlRegs::ROW_WORDS:
            if (value & ~0x7fe0)
                LOG_F(ERROR, "Control: write ROW_WORDS %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            else
                LOG_F(CONTROL, "Control: write ROW_WORDS %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            this->row_words = value & 0x7fe0;
            if (this->display_enabled) {
                this->enable_display();
            }
            break;
        case ControlRegs::MON_SENSE:
            if (value & ~0x1FF)
                LOG_F(ERROR, "Control: write MON_SENSE %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            else
                LOG_F(CONTROL, "Control: write MON_SENSE %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            value = (value >> 3) & 7;
            this->cur_mon_id = this->display_id->read_monitor_sense(value & 7, value ^ 7);
            break;
        case ControlRegs::ENABLE:
            if (value & ~0xfff)
                LOG_F(ERROR, "Control: write ENABLE %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            else
                LOG_F(CONTROL, "Control: write ENABLE %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            this->flags = value & -0xfff;
            break;
        case ControlRegs::GSC_DIVIDE:
            if (value & ~0x3)
                LOG_F(ERROR, "Control: write GSC_DIVIDE %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            else
                LOG_F(CONTROL, "Control: write GSC_DIVIDE %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            this->clock_divider = value & 3;
            if (this->display_enabled) {
                this->enable_display();
            }
            break;
        case ControlRegs::REFRESH_COUNT:
            if (value & ~0x3ff)
                LOG_F(ERROR, "Control: write REFRESH_COUNT %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            else
                LOG_F(CONTROL, "Control: write REFRESH_COUNT %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            break;
        case ControlRegs::INT_ENABLE:
            if (value & ~0xc)
                LOG_F(ERROR, "Control: write INT_ENABLE %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            else {
                //LOG_F(CONTROL, "Control: write INT_ENABLE %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
            }
            this->int_enable = value & 0xf; // alternates between 0x04 and 0x0c
            break;
        default:
            LOG_F(ERROR, "Control: write %03x.%c = %0*x", offset, SIZE_ARG(size), size * 2, value);
        }
    }
}

void ControlVideo::enable_display()
{
    int new_width, new_height, clk_divisor;

    // get pixel frequency from Athens
    this->pixel_clock = this->clk_gen->get_dot_freq();

    // get RaDACal clock divisor
    clk_divisor = 1 << ((rad_cr >> 6) + 1);

    // calculate active_width and active_height from video timing parameters
    new_width  = swatch_params[ControlRegs::HFP-1] - swatch_params[ControlRegs::HAL-1];
    new_height = swatch_params[ControlRegs::VFP-1] - swatch_params[ControlRegs::VAL-1];

    new_width *= clk_divisor;
    new_height >>= 1; // FIXME: assume non-interlaced mode for now

    this->active_width  = new_width;
    this->active_height = new_height;

    // set framebuffer parameters
    this->fb_ptr = &this->vram_ptr[this->fb_base];
    this->fb_pitch = this->row_words;

    // get pixel depth from RaDACal
    switch ((this->rad_cr >> 2) & 3) {
    case 0:
        this->pixel_depth = 8;
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_8bpp_indexed(dst_buf, dst_pitch);
        };
        break;
    case 1:
        this->pixel_depth = 16;
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_15bpp(dst_buf, dst_pitch);
        };
        break;
    case 2:
        this->pixel_depth = 32;
        this->fb_ptr += 16;
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

    this->vert_blank >>= 1;

    this->hori_total = this->hori_blank + new_width;
    this->vert_total = this->vert_blank + new_height;

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

void ControlVideo::draw_hw_cursor(uint8_t *dst_buf, int dst_pitch) {
    uint8_t *src_row = &this->vram_ptr[this->fb_base];
    uint8_t *dst_row = dst_buf;
    int cur_height = this->active_height;
    dst_pitch -= 32 * 4;
    int src_pitch = this->fb_pitch - 16;

    uint32_t color[16];
    for (int c = 0; c < 16; c++) {
        color[c] = (this->cursor_data[c*3] << 16) | (this->cursor_data[c*3 + 1] << 8) | (this->cursor_data[c*3 + 2]);
    }

    for (int h = 0; h < cur_height; h++) {
        for (int x = 0; x < 2; x++) {
            uint64_t px16 = READ_QWORD_BE_A(src_row);
            for (int p = 0; p < 16; p++) {
                int c = px16 >> 60;
                switch (c) {
                    case 0:
                        // transparent
                        break;
                    case 1:
                        // 1's complement
                        WRITE_DWORD_LE_A(dst_row, READ_DWORD_LE_A(dst_row) ^ 0xffffff);
                        break;
                    case 8:
                        WRITE_DWORD_LE_A(dst_row, color[0]);
                        break;
                    case 9:
                        WRITE_DWORD_LE_A(dst_row, color[1]);
                        break;
                    default:
                        WRITE_DWORD_LE_A(dst_row, (c << 16) | (c << 8) | c);
                        break;
                }
                px16 <<= 4;
                dst_row += 4;
            }
            src_row += 8;
        }
        dst_row += dst_pitch;
        src_row += src_pitch;
    }
}

// ========================== RaDACal related stuff ==========================

uint16_t ControlVideo::iodev_read(uint32_t address)
{
    uint16_t result;
    switch (address) {
    case RadacalRegs::MULTI:
        switch (this->rad_addr) {
        case RadacalRegs::MISC_CTRL:
            result = this->rad_cr;
            LOG_F(RADACAL, "RaDACal: read  MISC_CTRL = 0x%02x", result);
            break;
/*
        case RadacalRegs::CLOCK_SELECT:
            result = this->dac_clock_select;
            LOG_F(RADACAL, "RaDACal: read  CLOCK_SELECT = 0x%02x", result);
            break;
        case RadacalRegs::DAC_TYPE:
            result = this->dac_type;
            LOG_F(RADACAL, "RaDACal: read  DAC_TYPE = 0x%02x", result);
            break;
*/
        default:
            LOG_F(ERROR, "RaDACal: read  MULTI 0x%02x", this->rad_addr);
            result = 0;
        }
        break;
    case RadacalRegs::CLUT_DATA:
        LOG_F(ERROR, "RaDACal: read  CLUT_DATA 0x%02x", rad_addr);
        result = 0;
        break;
    default:
        LOG_F(ERROR, "RaDACal: read  0x%02x", address);
        result = 0;
    }

    return result;
}
void ControlVideo::iodev_write(uint32_t address, uint16_t value)
{
    switch (address) {
    case RadacalRegs::ADDRESS:
        LOG_F(RADACAL, "RaDACal: write ADDRESS = 0x%02x", value);
        this->rad_addr = value;
        this->comp_index = 0;
        break;
    case RadacalRegs::CURSOR_DATA:
        LOG_F(RADACAL, "RaDACal: write CURSOR_DATA 0x%02x = 0x%02x", this->rad_addr, value);
        this->cursor_data[(this->rad_addr++) % 24] = value;
        break;
    case RadacalRegs::MULTI:
        switch (this->rad_addr) {
        case RadacalRegs::CURSOR_POS_HI:
            LOG_F(RADACAL, "RaDACal: write CURSOR_POS_HI = 0x%02x", value);
            this->rad_cur_pos = (value << 8) | (this->rad_cur_pos & 0x00ff);
            break;
        case RadacalRegs::CURSOR_POS_LO:
            LOG_F(RADACAL, "RaDACal: write CURSOR_POS_LO = 0x%02x", value);
            this->rad_cur_pos = (this->rad_cur_pos & 0xff00) | (value & 0x00ff);
            break;
        case RadacalRegs::MISC_CTRL:
            LOG_F(RADACAL, "RaDACal: write MISC_CTRL = 0x%02x", value);
            this->rad_cr = value;
            break;
        case RadacalRegs::DBL_BUF_CTRL:
            LOG_F(RADACAL, "RaDACal: write DBL_BUF_CTRL = 0x%02x", value);
            this->rad_dbl_buf_cr = value;
            break;
        default:
            LOG_F(ERROR, "RaDACal: write MULTI 0x%02x = 0x%02x", this->rad_addr, value);
        }
        break;
    case RadacalRegs::CLUT_DATA:
        LOG_F(RADACAL, "RaDACal: write CLUT_DATA 0x%02x = 0x%02x", this->rad_addr, value);
        this->clut_color[this->comp_index++] = value;
        if (this->comp_index >= 3) {
            this->set_palette_color(this->rad_addr, clut_color[0],
                                    clut_color[1], clut_color[2], 0xFF);
            this->rad_addr++; // auto-increment CLUT address
            this->comp_index = 0;
        }
        break;
    default:
        LOG_F(ERROR, "RaDACal: write 0x%02x = 0x%02x", address, value);
    }
}

int ControlVideo::device_postinit()
{
    this->int_ctrl = dynamic_cast<InterruptCtrl*>(
        gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
    this->irq_id = this->int_ctrl->register_dev_int(IntSrc::CONTROL);

    this->vbl_cb = [this](uint8_t irq_line_state) {
        if (irq_line_state)
            this->int_status |= 0xc;
        else
            this->int_status &= ~0xc;

        if (this->crtc_on && (4 & this->int_enable)) {
            //this->pci_interrupt(irq_line_state);
            this->int_ctrl->ack_int(this->irq_id, irq_line_state);
        }
    };

    return 0;
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
