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

    // register RaDACal with the I/O controller
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

uint32_t ControlVideo::read(uint32_t rgn_start, uint32_t offset, int size)
{
    uint32_t result = 0;

    if (rgn_start == this->vram_base) {
        if (offset >= 0x800000) {
            return read_mem_rev(&this->vram_ptr[offset - 0x800000], size);
        } else {
            LOG_F(INFO, "Control: little-endian access to VRAM not supported yet");
            return 0;
        }
    }

    if (rgn_start == this->regs_base) {
        switch (offset >> 4) {
        case ControlRegs::TEST:
            result = this->test;
            break;
        case ControlRegs::MON_SENSE:
            result = this->cur_mon_id << 6;
            break;
        default:
            LOG_F(INFO, "read from 0x%08X:0x%08X", rgn_start, offset);
        }

        return BYTESWAP_32(result);
    }

    return 0;
}

void ControlVideo::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    if (rgn_start == this->vram_base) {
        if (offset >= 0x800000) {
            write_mem_rev(&this->vram_ptr[offset - 0x800000], value, size);
        } else {
            LOG_F(INFO, "Control: little-endian access to VRAM not supported yet");
        }
        return;
    }

    if (rgn_start == this->regs_base) {
        value = BYTESWAP_32(value);

        switch (offset >> 4) {
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
            this->swatch_params[(offset >> 4) - 1] = value;
            break;
        case ControlRegs::TEST:
            if (this->test != value) {
                if ((this->test & ~TEST_STROBE) != (value & ~TEST_STROBE)) {
                    this->test = value;
                    this->test_shift = 0;
                    LOG_F(9, "New TEST value: 0x%08X", this->test);
                } else {
                    LOG_F(9, "TEST strobe bit flipped, new value: 0x%08X", value);
                    this->test = value;
                    if (++this->test_shift >= 3) {
                        LOG_F(9, "Received TEST reg value: 0x%08X", this->test & ~TEST_STROBE);
                        if ((this->test ^ this->prev_test) & 0x400) {
                            if (this->test & 0x400) {
                                this->disable_display();
                            } else {
                                this->enable_display();
                            }
                            this->prev_test = this->test;
                        }
                    }
                }
            }
            break;
        case ControlRegs::GBASE:
            this->fb_base = value;
            break;
        case ControlRegs::ROW_WORDS:
            this->row_words = value;
            break;
        case ControlRegs::MON_SENSE:
            LOG_F(9, "Control: monitor sense written with 0x%X", value);
            value = (value >> 3) & 7;
            this->cur_mon_id = this->display_id->read_monitor_sense(value & 7, value ^ 7);
            break;
        case ControlRegs::ENABLE:
            this->flags = value;
            break;
        case ControlRegs::GSC_DIVIDE:
            this->clock_divider = value;
            break;
        case ControlRegs::REFRESH_COUNT:
            LOG_F(INFO, "Control: refresh count set to 0x%08X", value);
            break;
        case ControlRegs::INT_ENABLE:
            this->int_enable = value;
            break;
        default:
            LOG_F(INFO, "write 0x%08X to 0x%08X:0x%08X", value, rgn_start, offset);
        }
    }
}

void ControlVideo::enable_display()
{
    int new_width, new_height, hori_blank, vert_blank, clk_divisor;

    // get pixel frequency from Athens
    this->pixel_clock = this->clk_gen->get_dot_freq();

    // get RaDACal clock divisor
    clk_divisor = 1 << ((rad_cr >> 6) + 1);

    // calculate active_width and active_height from video timing parameters
    new_width  = swatch_params[ControlRegs::HFP-1] - swatch_params[ControlRegs::HAL-1];
    new_height = swatch_params[ControlRegs::VFP-1] - swatch_params[ControlRegs::VAL-1];

    new_width *= clk_divisor;
    new_height >>= 1; // FIXME: assume non-interlaced mode for now

    if (new_width != this->active_width || new_height != this->active_height) {
        LOG_F(WARNING, "Display window resizing not implemented yet!");
    }

    this->active_width  = new_width;
    this->active_height = new_height;

    // get pixel depth from RaDACal
    switch ((this->rad_cr >> 2) & 3) {
    case 0:
        this->pixel_depth = 8;
        break;
    case 1:
        this->pixel_depth = 16;
        break;
    case 2:
        this->pixel_depth = 32;
        break;
    default:
        ABORT_F("Invalid RaDACal pixel depth code!");
    }

    if (pixel_depth == 8) {
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_8bpp_indexed(dst_buf, dst_pitch);
        };
    } else {
        ABORT_F("Control: 16bpp and 32bpp formats not supported yet!");
    }

    // set framebuffer parameters
    this->fb_ptr = &this->vram_ptr[this->fb_base];
    this->fb_pitch = this->row_words;

    // calculate display refresh rate
    hori_blank = swatch_params[ControlRegs::HAL-1] +
        (swatch_params[ControlRegs::HSP-1] - swatch_params[ControlRegs::HFP-1]);

    hori_blank *= clk_divisor;

    vert_blank = swatch_params[ControlRegs::VAL-1] +
        (swatch_params[ControlRegs::VSYNC-1] - swatch_params[ControlRegs::VFP-1]);

    vert_blank >>= 1;

    this->refresh_rate = (double)(this->pixel_clock) / (new_width + hori_blank)
        / (new_height + vert_blank);
    LOG_F(INFO, "Control: refresh rate set to %f Hz", this->refresh_rate);

    this->stop_refresh_task();

    // set up periodic timer for display updates
    this->start_refresh_task();

    this->blank_on = false;

    LOG_F(INFO, "Control: display enabled");
    this->crtc_on = true;
}

void ControlVideo::disable_display()
{
    this->crtc_on = false;
    LOG_F(INFO, "Control: display disabled");
}

// ========================== RaDACal related stuff ==========================

uint16_t ControlVideo::iodev_read(uint32_t address)
{
    LOG_F(INFO, "RaDACal: read from 0x%08X", address);
    return 0;
}

void ControlVideo::iodev_write(uint32_t address, uint16_t value)
{
    switch (address) {
    case RadacalRegs::ADDRESS:
        this->rad_addr = value;
        this->comp_index = 0;
        break;
    case RadacalRegs::CURSOR_DATA:
        break;
    case RadacalRegs::MULTI:
        switch (this->rad_addr) {
        case RadacalRegs::CURSOR_POS_HI:
            this->rad_cur_pos_hi = value;
            break;
        case RadacalRegs::CURSOR_POS_LO:
            this->rad_cur_pos_lo = value;
            break;
        case RadacalRegs::MISC_CTRL:
            this->rad_cr = value;
            break;
        case RadacalRegs::DBL_BUF_CTRL:
            this->rad_dbl_buf_cr = value;
            break;
        default:
            LOG_F(ERROR, "Unsupported RaDACal register %d", this->rad_addr);
        }
        break;
    case RadacalRegs::CLUT_DATA:
        this->clut_color[this->comp_index++] = value;
        if (this->comp_index >= 3) {
            this->set_palette_color(this->rad_addr, clut_color[0],
                                    clut_color[1], clut_color[2], 0xFF);
            this->rad_addr++; // auto-increment CLUT address
            this->comp_index = 0;
        }
        break;
    default:
        LOG_F(INFO, "RaDACal: write to non-existent register at 0x%08X", address);
    }
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
