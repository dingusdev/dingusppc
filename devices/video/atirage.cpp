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

#include <core/timermanager.h>
#include <devices/common/hwcomponent.h>
#include <devices/common/pci/pcidevice.h>
#include <devices/deviceregistry.h>
#include <devices/video/atirage.h>
#include <devices/video/displayid.h>
#include <endianswap.h>
#include <loguru.hpp>
#include <memaccess.h>

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>

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
    : PCIDevice("ati-rage"), VideoCtrlBase(1024, 768)
{
    uint8_t asic_id;

    supports_types(HWCompType::MMIO_DEV | HWCompType::PCI_DEV);

    this->vram_size = GET_INT_PROP("gfxmem_size") << 20; // convert MBs to bytes

    /* allocate video RAM */
    this->vram_ptr = std::unique_ptr<uint8_t[]> (new uint8_t[this->vram_size]);

    /* ATI Rage driver needs to know ASIC ID (manufacturer's internal chip code)
       to operate properly */
    switch (dev_id) {
    case ATI_RAGE_GT_DEV_ID:
        asic_id = 0x9A; // GT-B2U3 fabricated by UMC
        break;
    case ATI_RAGE_PRO_DEV_ID:
        asic_id = 0x5C; // R3B/D/P-A4 fabricated by UMC
        break;
    default:
        asic_id = 0xDD;
        LOG_F(WARNING, "ATI Rage: bogus ASIC ID assigned!");
    }

    /* set up PCI configuration space header */
    this->vendor_id   = PCI_VENDOR_ATI;
    this->device_id   = dev_id;
    this->subsys_vndr = PCI_VENDOR_ATI;
    this->subsys_id   = 0x6987; // adapter ID
    this->class_rev   = (0x030000 << 8) | asic_id;
    this->min_gnt     = 8;
    this->irq_pin     = 1;
    this->bars_cfg[0] = 0xFF000000UL; // declare main aperture (16MB)
    this->bars_cfg[1] = 0xFFFFFF01UL; // declare I/O region (256 bytes)
    this->bars_cfg[2] = 0xFFFFF000UL; // declare register aperture (4KB)
    this->finish_config_bars();

    this->pci_notify_bar_change = [this](int bar_num) {
        this->notify_bar_change(bar_num);
    };

    /* stuff default values into chip registers */
    WRITE_DWORD_LE_A(&this->mm_regs[ATI_CONFIG_CHIP_ID],
                    (asic_id << 24) | dev_id);

    /* initialize display identification */
    this->disp_id = std::unique_ptr<DisplayID> (new DisplayID());
}

void ATIRage::notify_bar_change(int bar_num)
{
    switch (bar_num) {
    case 0:
        if (this->aperture_base != (this->bars[bar_num] & 0xFFFFFFF0UL)) {
            this->aperture_base = this->bars[0] & 0xFFFFFFF0UL;
            this->host_instance->pci_register_mmio_region(this->aperture_base,
                APERTURE_SIZE, this);
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

uint32_t ATIRage::read_reg(uint32_t offset, uint32_t size) {
    uint32_t res;

    // perform register-specific pre-read action
    switch (offset & ~3) {
    case ATI_GP_IO:
        break;
    case ATI_CLOCK_CNTL:
        /* reading from internal PLL registers */
        if (offset == ATI_CLOCK_CNTL+2 && size == 1 &&
            !(this->mm_regs[ATI_CLOCK_CNTL+1] & 0x2)) {
            return this->plls[this->mm_regs[ATI_CLOCK_CNTL+1] >> 2];
        }
        break;
    case ATI_DAC_REGS:
        if (offset == ATI_DAC_DATA) {
            this->mm_regs[ATI_DAC_DATA] =
                this->palette[this->mm_regs[ATI_DAC_R_INDEX]][this->comp_index];
            this->comp_index++; /* move to next color component */
            if (this->comp_index >= 3) {
                /* autoincrement reading index - move to next palette entry */
                (this->mm_regs[ATI_DAC_R_INDEX])++;
                this->comp_index = 0;
            }
        }
        break;
    default:
        LOG_F(
            INFO,
            "ATI Rage: read I/O reg %s at 0x%X, size=%d, val=0x%X",
            get_reg_name(offset),
            offset,
            size,
            read_mem(&this->mm_regs[offset], size));
    }

    // reading internal registers with necessary endian conversion
    res = read_mem(&this->mm_regs[offset], size);

    return res;
}

void ATIRage::write_reg(uint32_t offset, uint32_t value, uint32_t size)
{
    uint8_t gpio_levels, gpio_dirs;

    // writing internal registers with necessary endian conversion
    write_mem(&this->mm_regs[offset], value, size);

    // perform register-specific post-write action
    switch (offset & ~3) {
    case ATI_CRTC_OFF_PITCH:
        LOG_F(INFO, "ATI Rage: CRTC_OFF_PITCH=0x%08X", READ_DWORD_LE_A(&this->mm_regs[ATI_CRTC_OFF_PITCH]));
        break;
    case ATI_CRTC_GEN_CNTL:
        if (this->mm_regs[ATI_CRTC_GEN_CNTL+3] & 2) {
            this->crtc_enable();
        } else {
            this->crtc_on = false;
        }
        LOG_F(INFO, "ATI Rage: CRTC_GEN_CNTL:CRTC_ENABLE=%d", !!(this->mm_regs[ATI_CRTC_GEN_CNTL+3] & 2));
        LOG_F(INFO, "ATI Rage: CRTC_GEN_CNTL:CRTC_DISPLAY_DIS=%d", !!(this->mm_regs[ATI_CRTC_GEN_CNTL] & 0x40));
        break;
    case ATI_CUR_OFFSET:
        LOG_F(INFO, "ATI Rage: CUR_OFFSET=0x%08X", READ_DWORD_LE_A(&this->mm_regs[ATI_CUR_OFFSET]));
        break;
    case ATI_CUR_HORZ_VERT_POSN:
        LOG_F(INFO, "ATI Rage: CUR_HORZ_VERT_POSN=0x%08X", READ_DWORD_LE_A(&this->mm_regs[ATI_CUR_HORZ_VERT_POSN]));
        break;
    case ATI_CUR_HORZ_VERT_OFF:
        LOG_F(INFO, "ATI Rage: CUR_HORZ_VERT_OFF=0x%08X", READ_DWORD_LE_A(&this->mm_regs[ATI_CUR_HORZ_VERT_OFF]));
        break;
    case ATI_GP_IO:
        if (offset < (ATI_GP_IO + 2)) {
            gpio_levels = this->mm_regs[ATI_GP_IO+1];
            gpio_levels = ((gpio_levels & 0x30) >> 3) | (gpio_levels & 1);
            gpio_dirs   = this->mm_regs[ATI_GP_IO+3];
            gpio_dirs   = ((gpio_dirs & 0x30) >> 3) | (gpio_dirs & 1);
            gpio_levels = this->disp_id->read_monitor_sense(gpio_levels, gpio_dirs);
            this->mm_regs[ATI_GP_IO+1] = ((gpio_levels & 6) << 3) | (gpio_levels & 1);
        }
        break;
    case ATI_CLOCK_CNTL:
        /* writing to internal PLL registers */
        if (offset == ATI_CLOCK_CNTL+2 && size == 1 &&
            (this->mm_regs[ATI_CLOCK_CNTL+1] & 0x2)) {
            int pll_addr = this->mm_regs[ATI_CLOCK_CNTL+1] >> 2;
            uint8_t pll_data = this->mm_regs[ATI_CLOCK_CNTL+2];
            this->plls[pll_addr] = pll_data;
            LOG_F(INFO, "ATI Rage: PLL #%d set to 0x%02X", pll_addr, pll_data);
        } else if (offset == ATI_CLOCK_CNTL && size == 1) {
            LOG_F(INFO, "ATI Rage: CLOCK_SEL = 0x%02X",
                  this->mm_regs[ATI_CLOCK_CNTL] & 3);
        }
        break;
    case ATI_DAC_REGS:
        switch (offset) {
        /* writing to read/write index registers resets color component index */
        case ATI_DAC_W_INDEX:
        case ATI_DAC_R_INDEX:
            this->comp_index = 0;
            break;
        case ATI_DAC_DATA:
            this->palette[this->mm_regs[ATI_DAC_W_INDEX]][this->comp_index] = value & 0xFF;
            this->comp_index++; /* move to next color component */
            if (this->comp_index >= 3) {
                LOG_F(
                    INFO,
                    "ATI DAC palette entry #%d set to R=%X, G=%X, B=%X",
                    this->mm_regs[ATI_DAC_W_INDEX],
                    this->palette[this->mm_regs[ATI_DAC_W_INDEX]][0],
                    this->palette[this->mm_regs[ATI_DAC_W_INDEX]][1],
                    this->palette[this->mm_regs[ATI_DAC_W_INDEX]][2]);
                /* autoincrement writing index - move to next palette entry */
                (this->mm_regs[ATI_DAC_W_INDEX])++;
                this->comp_index = 0;
            }
        }
        break;
    case ATI_GEN_TEST_CNTL:
        LOG_F(INFO, "HW cursor: %s", this->mm_regs[ATI_GEN_TEST_CNTL] & 0x80 ? "on" : "off");
        break;
    default:
        LOG_F(
            INFO,
            "ATI Rage: %s register at 0x%X set to 0x%X",
            get_reg_name(offset),
            offset & ~3,
            READ_DWORD_LE_A(&this->mm_regs[offset & ~3]));
    }

    //if ((this->mm_regs[ATI_CRTC_GEN_CNTL+3] & 2) &&
    //    !(this->mm_regs[ATI_CRTC_GEN_CNTL] & 0x40)) {
    //    int32_t src_offset = (READ_DWORD_LE_A(&this->mm_regs[ATI_CRTC_OFF_PITCH]) & 0xFFFF) * 8;

    //    this->fb_pitch = ((READ_DWORD_LE_A(&this->mm_regs[ATI_CRTC_OFF_PITCH])) >> 19) & 0x1FF8;

    //    this->fb_ptr = &this->vram_ptr[src_offset];
    //    this->update_screen();
    //}
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

    *res = this->read_reg(offset - this->io_base, size);
    return true;
}


bool ATIRage::pci_io_write(uint32_t offset, uint32_t value, uint32_t size) {
    if (!this->io_access_allowed(offset)) {
        return false;
    }

    this->write_reg(offset - this->io_base, value, size);
    return true;
}


uint32_t ATIRage::read(uint32_t rgn_start, uint32_t offset, int size)
{
    LOG_F(8, "Reading ATI Rage PCI memory: region=%X, offset=%X, size %d", rgn_start, offset, size);

    if (rgn_start < this->aperture_base || offset > APERTURE_SIZE) {
        LOG_F(WARNING, "ATI Rage: attempt to read outside the aperture!");
        return 0;
    }

    if (offset < this->vram_size) {
        /* read from little-endian VRAM region */
        return read_mem(&this->vram_ptr[offset], size);
    }
    else if (offset >= BE_FB_OFFSET) {
        /* read from big-endian VRAM region */
        return read_mem_rev(&this->vram_ptr[offset - BE_FB_OFFSET], size);
    }
    else if (offset >= MM_REGS_0_OFF) {
        /* read from memory-mapped registers, block 0 */
        return this->read_reg(offset - MM_REGS_0_OFF, size);
    }
    else if (offset >= MM_REGS_1_OFF) {
        /* read from memory-mapped registers, block 1 */
        return this->read_reg(offset - MM_REGS_1_OFF + 0x400, size);
    }
    else {
        LOG_F(WARNING, "ATI Rage: read attempt from unmapped aperture region at 0x%08X", offset);
    }

    return 0;
}

void ATIRage::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    LOG_F(8, "Writing reg=%X, offset=%X, value=%X, size %d", rgn_start, offset, value, size);

    if (rgn_start < this->aperture_base || offset > APERTURE_SIZE) {
        LOG_F(WARNING, "ATI Rage: attempt to write outside the aperture!");
        return;
    }

    if (offset < this->vram_size) {
        /* write to little-endian VRAM region */
        write_mem(&this->vram_ptr[offset], value, size);
    }
    else if (offset >= BE_FB_OFFSET) {
        /* write to big-endian VRAM region */
        write_mem_rev(&this->vram_ptr[offset - BE_FB_OFFSET], value, size);
    }
    else if (offset >= MM_REGS_0_OFF) {
        /* write to memory-mapped registers, block 0 */
        this->write_reg(offset - MM_REGS_0_OFF, value, size);
    }
    else if (offset >= MM_REGS_1_OFF) {
        /* write to memory-mapped registers, block 1 */
        this->write_reg(offset - MM_REGS_1_OFF + 0x400, value, size);
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

    const char* what = "Pixel format:";

    switch (this->mm_regs[ATI_CRTC_GEN_CNTL+1] & 7) {
    case 1:
        LOG_F(INFO, "%s 4 bpp with DAC palette", what);
        break;
    case 2:
        // check the undocumented DAC_DIRECT bit
        if (this->mm_regs[ATI_DAC_CNTL+1] & 4) {
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
        LOG_F(ERROR, "ATI Rage: CRTC pixel format %d not supported",
              this->mm_regs[ATI_CRTC_GEN_CNTL+2] & 7);
    }
}

void ATIRage::crtc_enable() {
    /* active (visible) width is specified in characters (8 px) - 1 */
    this->active_width  = (this->mm_regs[ATI_CRTC_H_TOTAL_DISP+2] + 1) * 8;

    /* active (visible) height is specified in lines - 1 */
    this->active_height =
        (READ_WORD_LE_A(&this->mm_regs[ATI_CRTC_V_TOTAL_DISP+2]) & 0x7FFUL) + 1;

    if ((this->plls[PLL_VCLK_CNTL] & 3) == 3) {
        /* look up which VPLL ouput is requested */
        int clock_sel = this->mm_regs[ATI_CLOCK_CNTL] & 3;

        /* calculate VPLL output frequency */
        float vpll_freq = calc_pll_freq(2, this->plls[VCLK0_FB_DIV + clock_sel]);

        /* calculate post divider's index */
        /* NOTE: post divider's index has been extended by an additional
                 bit in Rage Pro. This bit is resided in PLL_EXT_CNTL register.
        */
        int post_div_idx = ((this->plls[PLL_EXT_CNTL] >> (clock_sel + 2)) & 4) |
                           ((this->plls[VCLK_POST_DIV] >> (clock_sel * 2)) & 3);

        /* pixel clock = source_freq / post_div */
        this->pixel_clock = vpll_freq / mach64_post_div[post_div_idx];

        /* calculate display refresh rate */
        int hori_total =
            ((READ_WORD_LE_A(&this->mm_regs[ATI_CRTC_H_TOTAL_DISP])
              & 0x1FFUL) + 1) * 8;

        int vert_total =
            (READ_WORD_LE_A(&this->mm_regs[ATI_CRTC_V_TOTAL_DISP]) & 0x7FFUL) + 1;

        this->refresh_rate = pixel_clock / hori_total / vert_total;

        int32_t src_offset = (READ_DWORD_LE_A(&this->mm_regs[ATI_CRTC_OFF_PITCH]) & 0xFFFF) * 8;

        this->fb_pitch = ((READ_DWORD_LE_A(&this->mm_regs[ATI_CRTC_OFF_PITCH])) >> 19) & 0x1FF8;

        this->fb_ptr = &this->vram_ptr[src_offset];

        // specify framebuffer converter (hardcoded for now)
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_8bpp(dst_buf, dst_pitch);
        };

        LOG_F(INFO, "ATI Rage: primary CRT controller enabled:");
        LOG_F(INFO, "Video mode: %s",
             (this->mm_regs[ATI_CRTC_GEN_CNTL+3] & 1) ? "extended" : "VGA");
        LOG_F(INFO, "Video width: %d px", this->active_width);
        LOG_F(INFO, "Video height: %d px", this->active_height);
        verbose_pixel_format(0);
        LOG_F(INFO, "VPLL frequency: %f MHz", vpll_freq * 1e-6);
        LOG_F(INFO, "Pixel (dot) clock: %f MHz", this->pixel_clock * 1e-6);
        LOG_F(INFO, "Refresh rate: %f Hz", this->refresh_rate);

        uint64_t refresh_interval = static_cast<uint64_t>(1.0f / this->refresh_rate * NS_PER_SEC + 0.5);
        TimerManager::get_instance()->add_cyclic_timer(
            refresh_interval,
            [this]() {
                this->update_screen();
            }
        );

        this->update_screen();
    } else {
        LOG_F(WARNING, "ATI Rage: VLCK source != VPLL!");
    }

    this->crtc_on = true;
}

void ATIRage::draw_hw_cursor(uint8_t *dst_buf, int dst_pitch) {
    uint8_t *src_buf, *src_row, *dst_row, px4;

    // int horz_offset = READ_DWORD_LE_A(&this->mm_regs[ATI_CUR_HORZ_VERT_OFF]) & 0x3F;
    int vert_offset = (READ_DWORD_LE_A(&this->mm_regs[ATI_CUR_HORZ_VERT_OFF]) >> 16) & 0x3F;

    src_buf = &this->vram_ptr[(READ_DWORD_LE_A(&this->mm_regs[ATI_CUR_OFFSET]) * 8)];

    int cur_height = 64 - vert_offset;

    uint32_t color0 = READ_DWORD_LE_A(&this->mm_regs[ATI_CUR_CLR0]) | 0x000000FFUL;
    uint32_t color1 = READ_DWORD_LE_A(&this->mm_regs[ATI_CUR_CLR1]) | 0x000000FFUL;

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
                    break;
                case 3: // 1's complement of display pixel
                    break;
                }
            }
        }
    }
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
