/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-21 divingkatae and maximum
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

#include <devices/common/pci/pcidevice.h>
#include <devices/video/atirage.h>
#include <devices/video/displayid.h>
#include <endianswap.h>
#include <loguru.hpp>
#include <memaccess.h>

#include <chrono>
#include <cstdint>
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


ATIRage::ATIRage(uint16_t dev_id, uint32_t vmem_size_mb)
    : PCIDevice("ati-rage"), VideoCtrlBase()
{
    uint8_t asic_id;

    this->vram_size = vmem_size_mb << 20; // convert MBs to bytes

    /* allocate video RAM */
    this->vram_ptr = new uint8_t[this->vram_size];

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
    WRITE_DWORD_LE_A(&this->pci_cfg[0], (dev_id << 16) | ATI_PCI_VENDOR_ID);
    WRITE_DWORD_LE_A(&this->pci_cfg[8], (0x030000 << 8) | asic_id);
    WRITE_DWORD_LE_A(&this->pci_cfg[0x3C], 0x00080100);

    /* stuff default values into chip registers */
    WRITE_DWORD_LE_A(&this->block_io_regs[ATI_CONFIG_CHIP_ID],
                    (asic_id << 24) | dev_id);

    /* initialize display identification */
    this->disp_id = new DisplayID();
}

ATIRage::~ATIRage()
{
    if (this->vram_ptr) {
        delete this->vram_ptr;
    }

    delete (this->disp_id);
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

    switch (offset & ~3) {
    case ATI_GP_IO:
        break;
    case ATI_CLOCK_CNTL:
        /* reading from internal PLL registers */
        if (offset == ATI_CLOCK_CNTL+2 && size == 1 &&
            !(this->block_io_regs[ATI_CLOCK_CNTL+1] & 0x2)) {
            return this->plls[this->block_io_regs[ATI_CLOCK_CNTL+1] >> 2];
        }
        break;
    case ATI_DAC_REGS:
        if (offset == ATI_DAC_DATA) {
            this->block_io_regs[ATI_DAC_DATA] =
                this->palette[this->block_io_regs[ATI_DAC_R_INDEX]][this->comp_index];
            this->comp_index++; /* move to next color component */
            if (this->comp_index >= 3) {
                /* autoincrement reading index - move to next palette entry */
                (this->block_io_regs[ATI_DAC_R_INDEX])++;
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
            read_mem(&this->block_io_regs[offset], size));
    }

    res = read_mem(&this->block_io_regs[offset], size);

    return res;
}

void ATIRage::write_reg(uint32_t offset, uint32_t value, uint32_t size) {
    uint32_t gpio_val;
    uint16_t gpio_dir;

    /* size-dependent endian conversion */
    write_mem(&this->block_io_regs[offset], value, size);

    switch (offset & ~3) {
    case ATI_CRTC_OFF_PITCH:
        LOG_F(INFO, "ATI Rage: CRTC_OFF_PITCH=0x%08X", READ_DWORD_LE_A(&this->block_io_regs[ATI_CRTC_OFF_PITCH]));
        break;
    case ATI_CRTC_GEN_CNTL:
        if (this->block_io_regs[ATI_CRTC_GEN_CNTL+3] & 2) {
            this->crtc_enable();
        } else {
            this->crtc_on = false;
        }
        LOG_F(INFO, "ATI Rage: CRTC_GEN_CNTL:CRTC_ENABLE=%d", !!(this->block_io_regs[ATI_CRTC_GEN_CNTL+3] & 2));
        LOG_F(INFO, "ATI Rage: CRTC_GEN_CNTL:CRTC_DISPLAY_DIS=%d", !!(this->block_io_regs[ATI_CRTC_GEN_CNTL] & 0x40));
        break;
    case ATI_CUR_OFFSET:
        LOG_F(INFO, "ATI Rage: CUR_OFFSET=0x%08X", READ_DWORD_LE_A(&this->block_io_regs[ATI_CUR_OFFSET]));
        break;
    case ATI_CUR_HORZ_VERT_POSN:
        LOG_F(INFO, "ATI Rage: CUR_HORZ_VERT_POSN=0x%08X", READ_DWORD_LE_A(&this->block_io_regs[ATI_CUR_HORZ_VERT_POSN]));
        break;
    case ATI_CUR_HORZ_VERT_OFF:
        LOG_F(INFO, "ATI Rage: CUR_HORZ_VERT_OFF=0x%08X", READ_DWORD_LE_A(&this->block_io_regs[ATI_CUR_HORZ_VERT_OFF]));
        break;
    case ATI_GP_IO:
        if (offset < (ATI_GP_IO + 2)) {
            gpio_val = READ_DWORD_LE_A(&this->block_io_regs[ATI_GP_IO]);
            gpio_dir = (gpio_val >> 16) & 0x3FFF;
            WRITE_WORD_LE_A(
                &this->block_io_regs[ATI_GP_IO],
                this->disp_id->read_monitor_sense(gpio_val, gpio_dir));
        }
        break;
    case ATI_CLOCK_CNTL:
        /* writing to internal PLL registers */
        if (offset == ATI_CLOCK_CNTL+2 && size == 1 &&
            (this->block_io_regs[ATI_CLOCK_CNTL+1] & 0x2)) {
            int pll_addr = this->block_io_regs[ATI_CLOCK_CNTL+1] >> 2;
            uint8_t pll_data = this->block_io_regs[ATI_CLOCK_CNTL+2];
            this->plls[pll_addr] = pll_data;
            LOG_F(INFO, "ATI Rage: PLL #%d set to 0x%02X", pll_addr, pll_data);
        } else if (offset == ATI_CLOCK_CNTL && size == 1) {
            LOG_F(INFO, "ATI Rage: CLOCK_SEL = 0x%02X",
                  this->block_io_regs[ATI_CLOCK_CNTL] & 3);
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
            this->palette[this->block_io_regs[ATI_DAC_W_INDEX]][this->comp_index] = value & 0xFF;
            this->comp_index++; /* move to next color component */
            if (this->comp_index >= 3) {
                LOG_F(
                    INFO,
                    "ATI DAC palette entry #%d set to R=%X, G=%X, B=%X",
                    this->block_io_regs[ATI_DAC_W_INDEX],
                    this->palette[this->block_io_regs[ATI_DAC_W_INDEX]][0],
                    this->palette[this->block_io_regs[ATI_DAC_W_INDEX]][1],
                    this->palette[this->block_io_regs[ATI_DAC_W_INDEX]][2]);
                /* autoincrement writing index - move to next palette entry */
                (this->block_io_regs[ATI_DAC_W_INDEX])++;
                this->comp_index = 0;
            }
        }
        break;
    case ATI_GEN_TEST_CNTL:
        LOG_F(INFO, "HW cursor: %s", this->block_io_regs[ATI_GEN_TEST_CNTL] & 0x80 ? "on" : "off");
        break;
    default:
        LOG_F(
            INFO,
            "ATI Rage: %s register at 0x%X set to 0x%X",
            get_reg_name(offset),
            offset & ~3,
            READ_DWORD_LE_A(&this->block_io_regs[offset & ~3]));
    }

    if ((this->block_io_regs[ATI_CRTC_GEN_CNTL+3] & 2) &&
        !(this->block_io_regs[ATI_CRTC_GEN_CNTL] & 0x40)) {
        int32_t src_offset = (READ_DWORD_LE_A(&this->block_io_regs[ATI_CRTC_OFF_PITCH]) & 0xFFFF) * 8;

        this->fb_pitch = ((READ_DWORD_LE_A(&this->block_io_regs[ATI_CRTC_OFF_PITCH])) >> 19) & 0x1FF8;

        this->fb_ptr = this->vram_ptr + src_offset;
        this->update_screen();
    }
}


uint32_t ATIRage::pci_cfg_read(uint32_t reg_offs, uint32_t size) {
    uint32_t res = 0;

    LOG_F(INFO, "Reading ATI Rage config space, offset = 0x%X, size=%d", reg_offs, size);

    res = read_mem(&this->pci_cfg[reg_offs], size);

    LOG_F(INFO, "Return value: 0x%X", res);
    return res;
}

void ATIRage::pci_cfg_write(uint32_t reg_offs, uint32_t value, uint32_t size) {
    LOG_F(
        INFO,
        "Writing into ATI Rage PCI config space, offset = 0x%X, val=0x%X size=%d",
        reg_offs,
        BYTESWAP_32(value),
        size);

    switch (reg_offs) {
    case 0x10: /* BAR 0 */
        if (value == 0xFFFFFFFFUL) {
            WRITE_DWORD_LE_A(&this->pci_cfg[CFG_REG_BAR0], 0xFF000000UL);
        }
        else {
            this->aperture_base = BYTESWAP_32(value);
            LOG_F(INFO, "ATI Rage aperture address set to 0x%08X", this->aperture_base);
            WRITE_DWORD_BE_A(&this->pci_cfg[CFG_REG_BAR0], value);
            this->host_instance->pci_register_mmio_region(this->aperture_base,
                APERTURE_SIZE, this);
        }
        break;
    case 0x14: /* BAR 1: I/O space base, 256 bytes wide */
        if (value == 0xFFFFFFFFUL) {
            WRITE_DWORD_BE_A(&this->pci_cfg[CFG_REG_BAR1], 0xFFFFFF01UL);
        }
        else {
            WRITE_DWORD_BE_A(&this->pci_cfg[CFG_REG_BAR1], value);
        }
        break;
    case 0x18: /* BAR 2 */
        if (value == 0xFFFFFFFFUL) {
            WRITE_DWORD_BE_A(&this->pci_cfg[CFG_REG_BAR2], 0xFFFFF000UL);
        }
        else {
            WRITE_DWORD_BE_A(&this->pci_cfg[CFG_REG_BAR2], value);
        }
        break;
    case CFG_REG_BAR3: /* unimplemented */
    case CFG_REG_BAR4: /* unimplemented */
    case CFG_REG_BAR5: /* unimplemented */
        WRITE_DWORD_BE_A(&this->pci_cfg[reg_offs], 0);
        break;
    case CFG_EXP_BASE: /* no expansion ROM */
        if (value == 0x00F8FFFFUL) {
            // return 0 (not implemented) when attempting to size the expansion ROM
            WRITE_DWORD_BE_A(&this->pci_cfg[reg_offs], 0);
        } else {
            WRITE_DWORD_BE_A(&this->pci_cfg[reg_offs], value);
        }
        break;
    default:
        write_mem(&this->pci_cfg[reg_offs], value, size);
    }
}


bool ATIRage::io_access_allowed(uint32_t offset, uint32_t* p_io_base) {
    if (!(this->pci_cfg[CFG_REG_CMD] & 1)) {
        LOG_F(WARNING, "ATI I/O space disabled in the command reg");
        return false;
    }

    uint32_t io_base = READ_DWORD_LE_A(&this->pci_cfg[CFG_REG_BAR1]) & ~3;

    if (offset < io_base || offset > (io_base + 0x100)) {
        LOG_F(WARNING, "Rage: I/O out of range, base=0x%X, offset=0x%X", io_base, offset);
        return false;
    }

    *p_io_base = io_base;

    return true;
}


bool ATIRage::pci_io_read(uint32_t offset, uint32_t size, uint32_t* res) {
    uint32_t io_base;

    if (!this->io_access_allowed(offset, &io_base)) {
        return false;
    }

    *res = this->read_reg(offset - io_base, size);
    return true;
}


bool ATIRage::pci_io_write(uint32_t offset, uint32_t value, uint32_t size) {
    uint32_t io_base;

    if (!this->io_access_allowed(offset, &io_base)) {
        return false;
    }

    this->write_reg(offset - io_base, value, size);
    return true;
}


uint32_t ATIRage::read(uint32_t reg_start, uint32_t offset, int size)
{
    LOG_F(8, "Reading ATI Rage PCI memory: region=%X, offset=%X, size %d", reg_start, offset, size);

    if (reg_start < this->aperture_base || offset > APERTURE_SIZE) {
        LOG_F(WARNING, "ATI Rage: attempt to read outside the aperture!");
        return 0;
    }

    if (offset < this->vram_size) {
        /* read from little-endian VRAM region */
        return read_mem(this->vram_ptr + offset, size);
    }
    else if (offset >= BE_FB_OFFSET) {
        /* read from big-endian VRAM region */
        return read_mem_rev(this->vram_ptr + (offset - BE_FB_OFFSET), size);
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

void ATIRage::write(uint32_t reg_start, uint32_t offset, uint32_t value, int size)
{
    LOG_F(8, "Writing reg=%X, offset=%X, value=%X, size %d", reg_start, offset, value, size);

    if (reg_start < this->aperture_base || offset > APERTURE_SIZE) {
        LOG_F(WARNING, "ATI Rage: attempt to write outside the aperture!");
        return;
    }

    if (offset < this->vram_size) {
        /* write to little-endian VRAM region */
        write_mem(this->vram_ptr + offset, value, size);
    }
    else if (offset >= BE_FB_OFFSET) {
        /* write to big-endian VRAM region */
        write_mem_rev(this->vram_ptr + (offset - BE_FB_OFFSET), value, size);
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

    switch (this->block_io_regs[ATI_CRTC_GEN_CNTL+1] & 7) {
    case 1:
        LOG_F(INFO, "%s 4 bpp with DAC palette", what);
        break;
    case 2:
        // check the undocumented DAC_DIRECT bit
        if (this->block_io_regs[ATI_DAC_CNTL+1] & 4) {
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
              this->block_io_regs[ATI_CRTC_GEN_CNTL+2] & 7);
    }
}

void ATIRage::crtc_enable() {
    /* active (visible) width is specified in characters (8 px) - 1 */
    this->active_width  = (this->block_io_regs[ATI_CRTC_H_TOTAL_DISP+2] + 1) * 8;

    /* active (visible) height is specified in lines - 1 */
    this->active_height =
        (READ_WORD_LE_A(&this->block_io_regs[ATI_CRTC_V_TOTAL_DISP+2]) & 0x7FFUL) + 1;

    if ((this->plls[PLL_VCLK_CNTL] & 3) == 3) {
        /* look up which VPLL ouput is requested */
        int clock_sel = this->block_io_regs[ATI_CLOCK_CNTL] & 3;

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
            ((READ_WORD_LE_A(&this->block_io_regs[ATI_CRTC_H_TOTAL_DISP])
              & 0x1FFUL) + 1) * 8;

        int vert_total =
            (READ_WORD_LE_A(&this->block_io_regs[ATI_CRTC_V_TOTAL_DISP]) & 0x7FFUL) + 1;

        this->refresh_rate = pixel_clock / hori_total / vert_total;

        LOG_F(INFO, "ATI Rage: primary CRT controller enabled:");
        LOG_F(INFO, "Video mode: %s",
             (this->block_io_regs[ATI_CRTC_GEN_CNTL+3] & 1) ? "extended" : "VGA");
        LOG_F(INFO, "Video width: %d px", this->active_width);
        LOG_F(INFO, "Video height: %d px", this->active_height);
        verbose_pixel_format(0);
        LOG_F(INFO, "VPLL frequency: %f MHz", vpll_freq * 1e-6);
        LOG_F(INFO, "Pixel (dot) clock: %f MHz", this->pixel_clock * 1e-6);
        LOG_F(INFO, "Refresh rate: %f Hz", this->refresh_rate);
    } else {
        LOG_F(WARNING, "ATI Rage: VLCK source != VPLL!");
    }

    this->crtc_on = true;
}

void ATIRage::draw_hw_cursor(uint8_t *dst_buf, int dst_pitch) {
    uint8_t *src_buf, *src_row, *dst_row, px4;

    int horz_offset = READ_DWORD_LE_A(&this->block_io_regs[ATI_CUR_HORZ_VERT_OFF]) & 0x3F;
    int vert_offset = (READ_DWORD_LE_A(&this->block_io_regs[ATI_CUR_HORZ_VERT_OFF]) >> 16) & 0x3F;

    src_buf = this->vram_ptr + (READ_DWORD_LE_A(&this->block_io_regs[ATI_CUR_OFFSET]) * 8);

    int cur_height = 64 - vert_offset;

    uint32_t color0 = READ_DWORD_LE_A(&this->block_io_regs[ATI_CUR_CLR0]) | 0x000000FFUL;
    uint32_t color1 = READ_DWORD_LE_A(&this->block_io_regs[ATI_CUR_CLR1]) | 0x000000FFUL;

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
