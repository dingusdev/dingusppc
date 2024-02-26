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

#include <core/bitops.h>
#include <devices/common/hwcomponent.h>
#include <devices/common/pci/pcidevice.h>
#include <devices/deviceregistry.h>
#include <devices/video/atirage.h>
#include <devices/video/displayid.h>
#include <endianswap.h>
#include <loguru.hpp>
#include <memaccess.h>

#include <map>

/* Mach64 post dividers. */
static const int mach64_post_div[8] = {
    1, 2, 4, 8, // standard post dividers
    3, 5, 6, 12 // alternate post dividers
};

/* Human readable Mach64 HW register names for easier debugging. */
static const std::map<uint16_t, std::string> mach64_reg_names = {
    {0x0000, "CRTC_H_TOTAL_DISP"},
    {0x0004, "CRTC_H_SYNC_STRT_WID"},
    {0x0008, "CRTC_V_TOTAL_DISP"},
    {0x000C, "CRTC_V_SYNC_STRT_WID"},
    {0x0010, "CRTC_VLINE_CRNT_VLINE"},
    {0x0014, "CRTC_OFF_PITCH"},
    {0x0018, "CRTC_INT_CNTL"},
    {0x001C, "CRTC_GEN_CNTL"},
    {0x0020, "DSP_CONFIG"},
    {0x0024, "DSP_ON_OFF"},
    {0x002C, "MEM_BUF_CNTL"},
    {0x0034, "MEM_ADDR_CFG"},
    {0x0040, "OVR_CLR"},
    {0x0044, "OVR_WID_LEFT_RIGHT"},
    {0x0048, "OVR_WID_TOP_BOTTOM"},
    {0x0060, "CUR_CLR0"},
    {0x0064, "CUR_CLR1"},
    {0x0068, "CUR_OFFSET"},
    {0x006C, "CUR_HORZ_VERT_POSN"},
    {0x0078, "GP_IO"},
    {0x007C, "HW_DEBUG"},
    {0x0080, "SCRATCH_REG0"},
    {0x0084, "SCRATCH_REG1"},
    {0x0088, "SCRATCH_REG2"},
    {0x008C, "SCRATCH_REG3"},
    {0x0090, "CLOCK_CNTL"},
    {0x00A0, "BUS_CNTL"},
    {0x00AC, "EXT_MEM_CNTL"},
    {0x00B0, "MEM_CNTL"},
    {0x00C0, "DAC_REGS"},
    {0x00C4, "DAC_CNTL"},
    {0x00D0, "GEN_TEST_CNTL"},
    {0x00D4, "CUSTOM_MACRO_CNTL"},
    {0x00E0, "CONFIG_CHIP_ID"},
    {0x00E4, "CONFIG_STAT0"},
    {0x01B4, "SRC_CNTL"},
    {0x01FC, "SCALE_3D_CNTL"},
    {0x0310, "FIFO_STAT"},
    {0x0338, "GUI_STAT"},
    {0x04C0, "MPP_CONFIG"},
    {0x04C4, "MPP_STROBE_SEQ"},
    {0x04C8, "MPP_ADDR"},
    {0x04CC, "MPP_DATA"},
    {0x0500, "TVO_CNTL"},
    {0x0704, "SETUP_CNTL"},
};

ATIRage::ATIRage(uint16_t dev_id)
    : PCIDevice("ati-rage"), VideoCtrlBase(640, 480)
{
    uint8_t asic_id;

    supports_types(HWCompType::MMIO_DEV | HWCompType::PCI_DEV);

    this->vram_size = GET_INT_PROP("gfxmem_size") << 20; // convert MBs to bytes

    // allocate video RAM
    this->vram_ptr = std::unique_ptr<uint8_t[]> (new uint8_t[this->vram_size]);

    // ATI Rage driver needs to know ASIC ID (manufacturer's internal chip code)
    // to operate properly
    switch (dev_id) {
    case ATI_RAGE_GT_DEV_ID:
        asic_id = 0x9A; // GT-B2U3 fabricated by UMC
        this->cmd_fifo_size = 48;
        break;
    case ATI_RAGE_PRO_DEV_ID:
        asic_id = 0x5C; // R3B/D/P-A4 fabricated by UMC
        this->cmd_fifo_size = 128;
        break;
    default:
        asic_id = 0xDD;
        LOG_F(WARNING, "ATI Rage: bogus ASIC ID assigned!");
    }

    // set up PCI configuration space header
    this->vendor_id   = PCI_VENDOR_ATI;
    this->device_id   = dev_id;
    this->subsys_vndr = PCI_VENDOR_ATI;
    this->subsys_id   = 0x6987; // adapter ID
    this->class_rev   = (0x030000 << 8) | asic_id;
    this->min_gnt     = 8;
    this->irq_pin     = 1;

    this->setup_bars({
        {0, 0xFF000000UL}, // declare main aperture (16MB)
        {1, 0xFFFFFF01UL}, // declare I/O region (256 bytes)
        {2, 0xFFFFF000UL}  // declare register aperture (4KB)
    });

    this->pci_notify_bar_change = [this](int bar_num) {
        this->notify_bar_change(bar_num);
    };

    // stuff default values into chip registers
    this->regs[ATI_CONFIG_CHIP_ID] = (asic_id << 24) | dev_id;

    // initialize display identification
    this->disp_id = std::unique_ptr<DisplayID> (new DisplayID());

    uint8_t mon_code = this->disp_id->read_monitor_sense(0, 0);

    this->regs[ATI_GP_IO] = ((mon_code & 6) << 11) | ((mon_code & 1) << 8);
}

void ATIRage::notify_bar_change(int bar_num)
{
    switch (bar_num) {
    case 0:
        if (this->aperture_base != (this->bars[bar_num] & 0xFFFFFFF0UL)) {
            this->aperture_base = this->bars[0] & 0xFFFFFFF0UL;
            this->host_instance->pci_register_mmio_region(this->aperture_base,
                APERTURE_SIZE - this->vram_size, this);
            LOG_F(INFO, "ATIRage: aperture address set to 0x%08X", this->aperture_base);
        }
        break;
    case 1:
        this->io_base = this->bars[1] & ~3;
        LOG_F(INFO, "ATIRage: I/O space address set to 0x%08X", this->io_base);
        break;
    case 2:
        LOG_F(INFO, "ATIRage: register aperture address set to 0x%08X", this->bars[2]);
        break;
    }
}

uint32_t ATIRage::pci_cfg_read(uint32_t reg_offs, AccessDetails &details)
{
    if (reg_offs < 64) {
        return PCIDevice::pci_cfg_read(reg_offs, details);
    }

    switch (reg_offs) {
    case 0x40:
        return this->user_cfg;
    default:
        LOG_READ_UNIMPLEMENTED_CONFIG_REGISTER();
    }

    return 0;
}

void ATIRage::pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details)
{
    if (reg_offs < 64) {
        PCIDevice::pci_cfg_write(reg_offs, value, details);
        return;
    }

    switch (reg_offs) {
    case 0x40:
        this->user_cfg = value;
        break;
    default:
        LOG_WRITE_UNIMPLEMENTED_CONFIG_REGISTER();
    }
}

const char* ATIRage::get_reg_name(uint32_t reg_offset) {
    auto iter = mach64_reg_names.find(reg_offset & ~3);
    if (iter != mach64_reg_names.end()) {
        return iter->second.c_str();
    } else {
        return "unknown Mach64 register";
    }
}

uint32_t ATIRage::read_reg(uint32_t reg_offset, uint32_t size) {
    uint64_t result;
    uint32_t offset = reg_offset & 3;

    switch (reg_offset >> 2) {
    case ATI_CLOCK_CNTL:
        result = this->regs[ATI_CLOCK_CNTL];
        if ((offset + size - 1) >= 2) {
            uint8_t pll_addr = (this->regs[ATI_CLOCK_CNTL] >> 10) & 0x3F;
            insert_bits<uint64_t>(result, this->plls[pll_addr], 16, 8);
        }
        break;
    case ATI_DAC_REGS:
        result = this->regs[ATI_DAC_REGS];
        switch (reg_offset) {
        case ATI_DAC_W_INDEX:
            insert_bits<uint64_t>(result, this->dac_wr_index, 0, 8);
            break;
        case ATI_DAC_MASK:
            insert_bits<uint64_t>(result, this->dac_mask, 16, 8);
            break;
        case ATI_DAC_R_INDEX:
            insert_bits<uint64_t>(result, this->dac_rd_index, 24, 8);
            break;
        case ATI_DAC_DATA:
            if (!this->comp_index) {
                uint8_t alpha; // temp variable for unused alpha
                get_palette_colors(this->dac_rd_index, color_buf[0],
                                   color_buf[1], color_buf[2], alpha);
            }
            insert_bits<uint64_t>(result, color_buf[this->comp_index], 8, 8);
            if (++this->comp_index >= 3) {
                this->dac_rd_index++; // auto-increment reading index
                this->comp_index = 0; // reset color component index
            }
        }
        break;
    case ATI_GUI_STAT:
        result = this->cmd_fifo_size << 16; // HACK: tell the guest the command FIFO is empty
        break;
    default:
        result = this->regs[reg_offset >> 2];
    }

    if (!offset && size == 4) { // fast path
        return result;
    } else { // slow path
        if ((offset + size) > 4) {
            result |= (uint64_t)(this->regs[(reg_offset >> 2) + 1]) << 32;
        }
        return extract_bits<uint64_t>(result, offset * 8, size * 8);
    }
}

void ATIRage::write_reg(uint32_t reg_offset, uint32_t value, uint32_t size) {
    uint32_t offset = reg_offset & 3;
    reg_offset >>= 2;

    if (offset || size != 4) { // slow path
        if ((offset + size) > 4) {
            ABORT_F("%s: unaligned DWORD writes not implemented", this->name.c_str());
        }
        uint64_t old_val = this->regs[reg_offset];
        insert_bits<uint64_t>(old_val, value, offset * 8, size * 8);
        value = old_val;
    }

    switch (reg_offset) {
    case ATI_CRTC_H_TOTAL_DISP:
        LOG_F(9, "%s: ATI_CRTC_H_TOTAL_DISP set to 0x%08X", this->name.c_str(), value);
        break;
    case ATI_CRTC_OFF_PITCH:
        if (this->regs[reg_offset] != value) {
            this->regs[reg_offset] = value;
            this->crtc_update();
        }
        break;
    case ATI_CRTC_GEN_CNTL:
        if (bit_changed(this->regs[reg_offset], value, 6)) {
            if (value & 0x40) {
                this->regs[reg_offset] |= (1 << 6);
                this->blank_on = true;
                this->blank_display();
            } else {
                this->regs[reg_offset] &= ~(1 << 6);
                this->blank_on = false;
            }
        }

        if (bit_changed(this->regs[reg_offset], value, 25)) {
            this->regs[reg_offset] = value;
            if (bit_set(this->regs[reg_offset], 25) &&
                !bit_set(this->regs[reg_offset], 6)) {
                this->crtc_update();
            }
            return;
        }
        break;
    case ATI_GP_IO:
        this->regs[reg_offset] = value;
        if (offset < 2 && (offset + size - 1) >= 1) {
            uint8_t gpio_levels = (this->regs[ATI_GP_IO] >> 8) & 0xFFU;
            gpio_levels = ((gpio_levels & 0x30) >> 3) | (gpio_levels & 1);
            uint8_t gpio_dirs = (this->regs[ATI_GP_IO] >> 24) & 0xFFU;
            gpio_dirs = ((gpio_dirs & 0x30) >> 3) | (gpio_dirs & 1);
            gpio_levels = this->disp_id->read_monitor_sense(gpio_levels, gpio_dirs);
            insert_bits<uint32_t>(this->regs[ATI_GP_IO],
                                ((gpio_levels & 6) << 3) | (gpio_levels & 1), 8, 8);
        }
        return;
    case ATI_CLOCK_CNTL:
        this->regs[reg_offset] = value;
        if ((offset + size - 1) >= 2 && bit_set(this->regs[ATI_CLOCK_CNTL], 9)) {
            uint8_t pll_addr = (this->regs[ATI_CLOCK_CNTL] >> 10) & 0x3F;
            uint8_t pll_data = (value >> 16) & 0xFF;
            this->plls[pll_addr] = pll_data;
            LOG_F(9, "%s: PLL #%d set to 0x%02X", this->name.c_str(), pll_addr, pll_data);
        }
        return;
    case ATI_DAC_REGS:
        switch (reg_offset * 4 + offset) {
        case ATI_DAC_W_INDEX:
            this->dac_wr_index = value & 0xFFU;
            this->comp_index = 0;
            break;
        case ATI_DAC_MASK:
            this->dac_mask = (value >> 16) & 0xFFU;
            break;
        case ATI_DAC_R_INDEX:
            this->dac_rd_index = (value >> 24) & 0xFFU;
            this->comp_index = 0;
            break;
        case ATI_DAC_DATA:
            this->color_buf[this->comp_index] = (value >> 8) & this->dac_mask;
            if (++this->comp_index >= 3) {
                this->set_palette_color(this->dac_wr_index, color_buf[0],
                                        color_buf[1], color_buf[2], 0xFF);
                this->dac_wr_index++; // auto-increment color index
                this->comp_index = 0; // reset color component index
            }
        }
        return;
    case ATI_GEN_TEST_CNTL:
        if (bit_changed(this->regs[reg_offset], value, 7)) {
            if (bit_set(value, 7))
                this->setup_hw_cursor();
            else
                this->cursor_on = false;
        }
        if (bit_changed(this->regs[reg_offset], value, 8)) {
            if (!bit_set(value, 8))
                LOG_F(9, "%s: reset GUI engine", this->name.c_str());
        }
        if (bit_changed(this->regs[reg_offset], value, 9)) {
            if (bit_set(value, 9))
                LOG_F(9, "%s: reset memory controller", this->name.c_str());
        }
        if (value & 0xFFFFFC00) {
            LOG_F(WARNING, "%s: unhandled GEN_TEST_CNTL state=0x%X",
                  this->name.c_str(), value);
        }
        break;
    case ATI_CONFIG_CHIP_ID:
        return; // prevent writes to this read-only register
    }

    this->regs[reg_offset] = value;
}

bool ATIRage::io_access_allowed(uint32_t offset) {
    if (offset >= this->io_base && offset < (this->io_base + 0x100)) {
        if (this->command & 1) {
            return true;
        }
        LOG_F(WARNING, "ATI I/O space disabled in the command reg");
    }
    return false;
}


bool ATIRage::pci_io_read(uint32_t offset, uint32_t size, uint32_t* res) {
    if (!this->io_access_allowed(offset)) {
        return false;
    }

    *res = BYTESWAP_SIZED(this->read_reg(offset - this->io_base, size), size);
    return true;
}


bool ATIRage::pci_io_write(uint32_t offset, uint32_t value, uint32_t size) {
    if (!this->io_access_allowed(offset)) {
        return false;
    }

    this->write_reg(offset - this->io_base, BYTESWAP_SIZED(value, size), size);
    return true;
}


uint32_t ATIRage::read(uint32_t rgn_start, uint32_t offset, int size)
{
    if (offset < this->vram_size) { // little-endian VRAM region
        return read_mem(&this->vram_ptr[offset], size);
    }
    else if (offset >= BE_FB_OFFSET) { // big-endian VRAM region
        return read_mem(&this->vram_ptr[offset - BE_FB_OFFSET], size);
    }
    else if (offset >= MM_REGS_0_OFF) { // memory-mapped registers, block 0
        return BYTESWAP_SIZED(this->read_reg(offset & 0x3FF, size), size);
    }
    else if (offset >= MM_REGS_1_OFF) { // memory-mapped registers, block 1
        return BYTESWAP_SIZED(this->read_reg((offset & 0x3FF) + 0x400, size), size);
    }
    else {
        LOG_F(WARNING, "ATI Rage: read attempt from unmapped aperture region at 0x%08X", offset);
    }

    return 0;
}

void ATIRage::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    if (offset < this->vram_size) { // little-endian VRAM region
        write_mem(&this->vram_ptr[offset], value, size);
    }
    else if (offset >= BE_FB_OFFSET) { // big-endian VRAM region
        write_mem(&this->vram_ptr[offset & (BE_FB_OFFSET - 1)], value, size);
    }
    else if (offset >= MM_REGS_0_OFF) { // memory-mapped registers, block 0
        this->write_reg(offset & 0x3FF, BYTESWAP_SIZED(value, size), size);
    }
    else if (offset >= MM_REGS_1_OFF) { // memory-mapped registers, block 1
        this->write_reg((offset & 0x3FF) + 0x400, BYTESWAP_SIZED(value, size), size);
    }
    else {
        LOG_F(WARNING, "ATI Rage: write attempt to unmapped aperture region at 0x%08X", offset);
    }
}

float ATIRage::calc_pll_freq(int scale, int fb_div) {
    return (ATI_XTAL * scale * fb_div) / this->plls[PLL_REF_DIV];
}

void ATIRage::verbose_pixel_format(int crtc_index) {
    if (crtc_index) {
        LOG_F(ERROR, "CRTC2 not supported yet");
        return;
    }

    uint32_t fmt = extract_bits<uint32_t>(this->regs[ATI_CRTC_GEN_CNTL], 8, 3);

    const char* what = "Pixel format:";

    switch (fmt) {
    case 1:
        LOG_F(INFO, "%s 4 bpp with DAC palette", what);
        break;
    case 2:
        // check the undocumented DAC_DIRECT bit
        if (bit_set(this->regs[ATI_DAC_CNTL], 10)) {
            LOG_F(INFO, "%s 8 bpp direct color (RGB322)", what);
        } else {
            LOG_F(INFO, "%s 8 bpp with DAC palette", what);
        }
        break;
    case 3:
        LOG_F(INFO, "%s 15 bpp direct color (RGB555)", what);
        break;
    case 4:
        LOG_F(INFO, "%s 16 bpp direct color (RGB565)", what);
        break;
    case 5:
        LOG_F(INFO, "%s 24 bpp direct color (RGB888)", what);
        break;
    case 6:
        LOG_F(INFO, "%s 32 bpp direct color (ARGB8888)", what);
        break;
    default:
        LOG_F(ERROR, "%s: CRTC pixel format %d not supported", this->name.c_str(), fmt);
    }
}

void ATIRage::crtc_update() {
    uint32_t new_width, new_height, new_htotal, new_vtotal;

    // check for unsupported modes and fail early
    if (!bit_set(this->regs[ATI_CRTC_GEN_CNTL], 24))
        ABORT_F("%s: VGA not supported", this->name.c_str());

    if ((this->plls[PLL_VCLK_CNTL] & 3) != 3)
        ABORT_F("%s: VLCK source != VPLL", this->name.c_str());

    bool need_recalc = false;

    new_width  = (extract_bits<uint32_t>(this->regs[ATI_CRTC_H_TOTAL_DISP], 16, 8) + 1) * 8;
    new_height = extract_bits<uint32_t>(this->regs[ATI_CRTC_V_TOTAL_DISP], 16, 11) + 1;

    if (new_width != this->active_width || new_height != this->active_height) {
        this->create_display_window(new_width, new_height);
        need_recalc = true;
    }

    new_htotal = (extract_bits<uint32_t>(this->regs[ATI_CRTC_H_TOTAL_DISP], 0, 9) + 1) * 8;
    new_vtotal = extract_bits<uint32_t>(this->regs[ATI_CRTC_V_TOTAL_DISP], 0, 11) + 1;

    if (new_htotal != this->hori_total || new_vtotal != this->vert_total) {
        this->hori_total = new_htotal;
        this->vert_total = new_vtotal;
        need_recalc = true;
    }

    if (!need_recalc)
        return;

    // look up which VPLL ouput is requested
    int clock_sel = this->regs[ATI_CLOCK_CNTL] & 3;

    // calculate VPLL output frequency
    float vpll_freq = calc_pll_freq(2, this->plls[VCLK0_FB_DIV + clock_sel]);

    // calculate post divider's index
    // NOTE: post divider's index has been extended by an additional
    // bit in Rage Pro. This bit is resided in PLL_EXT_CNTL register.
    int post_div_idx = ((this->plls[PLL_EXT_CNTL] >> (clock_sel + 2)) & 4) |
                       ((this->plls[VCLK_POST_DIV] >> (clock_sel * 2)) & 3);

    // pixel clock = source_freq / post_div
    this->pixel_clock = vpll_freq / mach64_post_div[post_div_idx];

    // calculate display refresh rate
    this->refresh_rate = pixel_clock / this->hori_total / this->vert_total;

    // set up frame buffer converter
    int pix_fmt = extract_bits<uint32_t>(this->regs[ATI_CRTC_GEN_CNTL], 8, 3);

    switch (pix_fmt) {
        case 1:
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_4bpp_indexed(dst_buf, dst_pitch);
        };
        break;
    case 2:
        if (bit_set(this->regs[ATI_DAC_CNTL], 10)) {
            this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
                this->convert_frame_8bpp(dst_buf, dst_pitch);
            };
        }
        else {
            this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
                this->convert_frame_8bpp_indexed(dst_buf, dst_pitch);
            };
        }
        break;
    case 3:
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_15bpp_BE(dst_buf, dst_pitch);
        };
        break;
    case 4:
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_16bpp(dst_buf, dst_pitch);
        };
        break;
    case 5:
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_24bpp(dst_buf, dst_pitch);
        };
        break;
    case 6:
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_32bpp_BE(dst_buf, dst_pitch);
        };
        break;
    default:
        LOG_F(ERROR, "%s: unsupported pixel format %d", this->name.c_str(), pix_fmt);
    }

    static uint8_t bits_per_pixel[8] = {0, 4, 8, 16, 16, 24, 32, 0};

    this->fb_pitch = extract_bits<uint32_t>(this->regs[ATI_CRTC_OFF_PITCH], 22, 10) *
        (bits_per_pixel[pix_fmt & 7] * 8) >> 3;

    this->fb_ptr = &this->vram_ptr[extract_bits<uint32_t>(this->regs[ATI_CRTC_OFF_PITCH], 0, 20) * 8];

    LOG_F(INFO, "%s: primary CRT controller enabled:", this->name.c_str());
    LOG_F(INFO, "Video mode: %s",
         bit_set(this->regs[ATI_CRTC_GEN_CNTL], 24) ? "extended" : "VGA");
    LOG_F(INFO, "Video width: %d px", this->active_width);
    LOG_F(INFO, "Video height: %d px", this->active_height);
    verbose_pixel_format(0);
    LOG_F(INFO, "VPLL frequency: %f MHz", vpll_freq * 1e-6);
    LOG_F(INFO, "Pixel (dot) clock: %f MHz", this->pixel_clock * 1e-6);
    LOG_F(INFO, "Refresh rate: %f Hz", this->refresh_rate);

    this->stop_refresh_task();
    this->start_refresh_task();

    this->crtc_on = true;
}

void ATIRage::draw_hw_cursor(uint8_t *dst_buf, int dst_pitch) {
    uint8_t *src_buf, *src_row, *dst_row, px4;

    int vert_offset = extract_bits<uint32_t>(this->regs[ATI_CUR_HORZ_VERT_OFF], 16, 5);

    src_buf = &this->vram_ptr[this->regs[ATI_CUR_OFFSET] * 8];

    int cur_height = 64 - vert_offset;

    uint32_t color0 = this->regs[ATI_CUR_CLR0] | 0x000000FFUL;
    uint32_t color1 = this->regs[ATI_CUR_CLR1] | 0x000000FFUL;

    for (int h = 0; h < cur_height; h++) {
        dst_row = &dst_buf[h * dst_pitch];
        src_row = &src_buf[h * 16];

        for (int x = 0; x < 16; x++) {
            px4 = src_row[x];

            for (int p = 0; p < 4; p++, px4 >>= 2, dst_row += 4) {
                switch(px4 & 3) {
                case 0: // cursor color 0
                    WRITE_DWORD_BE_A(dst_row, color0);
                    break;
                case 1: // cursor color 1
                    WRITE_DWORD_BE_A(dst_row, color1);
                    break;
                case 2: // transparent
                    WRITE_DWORD_BE_A(dst_row, 0);
                    break;
                case 3: // 1's complement of display pixel
                    break;
                }
            }
        }
    }
}

void ATIRage::get_cursor_position(int& x, int& y) {
    x =  this->regs[ATI_CUR_HORZ_VERT_POSN] & 0xFFFFU;
    y = (this->regs[ATI_CUR_HORZ_VERT_POSN] >> 16) & 0xFFFFU;
}

static const PropMap AtiRage_Properties = {
    {"gfxmem_size",
        new IntProperty(  2, vector<uint32_t>({2, 4, 6}))},
    {"mon_id",
        new StrProperty("")},
};

static const DeviceDescription AtiRageGT_Descriptor = {
    ATIRage::create_gt, {}, AtiRage_Properties
};

static const DeviceDescription AtiRagePro_Descriptor = {
    ATIRage::create_pro, {}, AtiRage_Properties
};

REGISTER_DEVICE(AtiRageGT, AtiRageGT_Descriptor);
REGISTER_DEVICE(AtiRagePro, AtiRagePro_Descriptor);
