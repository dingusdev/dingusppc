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

/** ATI Mach64 GX emulation.
   It emulates an ATI88800GX controller with an IBM RGB514 style RAMDAC.
   Emulation is limited to a basic frame buffer for now.
 */

#include <core/bitops.h>
#include <core/timermanager.h>
#include <devices/deviceregistry.h>
#include <devices/video/atimach64defs.h>
#include <devices/video/atimach64gx.h>
#include <devices/video/displayid.h>
#include <devices/video/rgb514defs.h>
#include <loguru.hpp>
#include <memaccess.h>

#include <string>

AtiMach64Gx::AtiMach64Gx()
    : PCIDevice("ati-mach64-gx"), VideoCtrlBase(1024, 768)
{
    supports_types(HWCompType::MMIO_DEV | HWCompType::PCI_DEV);

    // set up PCI configuration space header
    this->vendor_id   = PCI_VENDOR_ATI;
    this->device_id   = ATI_MACH64_GX_DEV_ID;
    this->class_rev   = (0x030000 << 8) | 3;

    this->setup_bars({{0, 0xFF000000UL}}); // declare main aperture (16MB)

    this->pci_notify_bar_change = [this](int bar_num) {
        this->notify_bar_change(bar_num);
    };

    // declare expansion ROM containing FCode and Mac OS drivers
    if (this->attach_exp_rom_image(std::string("113-32900-004_Apple_MACH64.bin"))) {
        ABORT_F("%s: could not load ROM - this device won't work properly!",
                this->name.c_str());
    }

    // initialize display identification
    this->disp_id = std::unique_ptr<DisplayID> (new DisplayID(0x07, 0x3A));

    // allocate video RAM
    this->vram_size = 2 << 20;
    this->vram_ptr = std::unique_ptr<uint8_t[]> (new uint8_t[this->vram_size]);

    // set up RAMDAC identification
    this->regs[ATI_CONFIG_STAT0] = 1 << 9;
}

void AtiMach64Gx::notify_bar_change(int bar_num)
{
    if (bar_num) // only BAR0 is supported
        return;

    if (this->aperture_base != (this->bars[bar_num] & 0xFFFFFFF0UL)) {
        if (this->aperture_base) {
            LOG_F(WARNING, "AtiMach64Gx: deallocating I/O memory not implemented");
        }

        this->aperture_base = this->bars[0] & 0xFFFFFFF0UL;
        this->host_instance->pci_register_mmio_region(this->aperture_base,
                                                      APERTURE_SIZE, this);

        // copy aperture address to CONFIG_CNTL:CFG_MEM_AP_LOC
        this->config_cntl = (this->config_cntl & 0xFFFFC00FUL) |
                            ((this->aperture_base >> 18) & 0x3FF0U);

        LOG_F(INFO, "AtiMach64Gx: aperture address set to 0x%08X", this->aperture_base);
    }
}

// map I/O register index to MMIO register offset
static const uint32_t io_idx_to_reg_offset[32] = {
    ATI_CRTC_H_TOTAL_DISP,      ATI_CRTC_H_SYNC_STRT_WID,
    ATI_CRTC_V_TOTAL_DISP,      ATI_CRTC_V_SYNC_STRT_WID,
    ATI_CRTC_VLINE_CRNT_VLINE,  ATI_CRTC_OFF_PITCH,
    ATI_CRTC_INT_CNTL,          ATI_CRTC_GEN_CNTL,
    ATI_OVR_CLR,                ATI_OVR_WID_LEFT_RIGHT,
    ATI_OVR_WID_TOP_BOTTOM,     ATI_CUR_CLR0,
    ATI_CUR_CLR1,               ATI_CUR_OFFSET,
    ATI_CUR_HORZ_VERT_POSN,     ATI_CUR_HORZ_VERT_OFF,
    ATI_SCRATCH_REG0,           ATI_SCRATCH_REG1,
    ATI_CLOCK_CNTL,             ATI_BUS_CNTL,
    ATI_MEM_CNTL,               ATI_MEM_VGA_WP_SEL,
    ATI_MEM_VGA_RP_SEL,         ATI_DAC_REGS,
    ATI_DAC_CNTL,               ATI_GEN_TEST_CNTL,
    ATI_CONFIG_CNTL,            ATI_CONFIG_CHIP_ID,
    ATI_CONFIG_STAT0,           ATI_GX_CONFIG_STAT1,
    ATI_INVALID,                ATI_CRTC_H_TOTAL_DISP
};

bool AtiMach64Gx::pci_io_read(uint32_t offset, uint32_t size, uint32_t* res)
{
    *res = 0;

    // check for valid I/O base and I/O access permission
    if ((offset & 0x3FC) != 0x2EC || !(this->command & 1)) {
        return false;
    }

    // convert ISA-style I/O address to MMIO register offset
    offset = io_idx_to_reg_offset[(offset >> 10) & 0x1F] * 4 + (offset & 3);

    // CONFIG_CNTL is accessible from I/O space only
    if ((offset >> 2) == ATI_CONFIG_CNTL) {
        *res = read_mem(((uint8_t *)&this->config_cntl) + (offset & 3), size);
    } else {
        *res = BYTESWAP_SIZED(this->read_reg(offset, size), size);
    }

    return true;
}

bool AtiMach64Gx::pci_io_write(uint32_t offset, uint32_t value, uint32_t size)
{
    // check for valid I/O base and I/O access permission
    if ((offset & 0x3FC) != 0x2EC || !(this->command & 1)) {
        return false;
    }

    // convert ISA-style I/O address to MMIO register offset
    offset = io_idx_to_reg_offset[(offset >> 10) & 0x1F] * 4 + (offset & 3);

    // CONFIG_CNTL is accessible from I/O space only
    if ((offset >> 2) == ATI_CONFIG_CNTL) {
        write_mem(((uint8_t *)&this->config_cntl) + (offset & 3), value, size);
        switch (this->config_cntl & 3) {
        case 0:
            LOG_F(WARNING, "%s: linear aperture disabled!", this->name.c_str());
            break;
        case 1:
            LOG_F(INFO, "%s: aperture size set to 4MB", this->name.c_str());
            this->mm_regs_offset = MM_REGS_2_OFF;
            break;
        case 2:
            LOG_F(INFO, "%s: aperture size set to 8MB", this->name.c_str());
            this->mm_regs_offset = MM_REGS_0_OFF;
            break;
        default:
            LOG_F(ERROR, "%s: invalid aperture size in CONFIG_CNTL", this->name.c_str());
        }
    } else {
        this->write_reg(offset, BYTESWAP_SIZED(value, size), size);
    }

    return true;
}

uint32_t AtiMach64Gx::read_reg(uint32_t reg_offset, uint32_t size)
{
    uint32_t offset = reg_offset & 3;

    uint64_t result = this->regs[reg_offset >> 2];

    if (!offset && size == 4) { // fast path
        return static_cast<uint32_t>(result);
    } else { // slow path
        if ((offset + size) > 4) {
            result |= (uint64_t)(this->regs[(reg_offset >> 2) + 1]) << 32;
        }
        return static_cast<uint32_t>(extract_bits<uint64_t>(result, offset * 8, size * 8));
    }
}

void AtiMach64Gx::write_reg(uint32_t reg_offset, uint32_t value, uint32_t size)
{
    uint8_t gpio_dirs, gpio_levels;
    int crtc_en;

    uint32_t offset = reg_offset & 3;
    reg_offset >>= 2;

    if (offset || size != 4) { // slow path
        if ((offset + size) > 4) {
            ABORT_F("%s: unaligned DWORD writes not implemented", this->name.c_str());
        }
        uint64_t old_val = this->regs[reg_offset];
        insert_bits<uint64_t>(old_val, value, offset * 8, size * 8);
        value = static_cast<uint32_t>(old_val);
    }

    switch (reg_offset) {
    case ATI_CRTC_OFF_PITCH:
        this->fb_pitch = extract_bits<uint32_t>(value, 22, 10) * 8;
        this->fb_ptr = &this->vram_ptr[extract_bits<uint32_t>(value, 0, 20) * 8];
        break;
    case ATI_CRTC_GEN_CNTL:
        if (bit_changed(this->regs[reg_offset], value, 6)) {
            if (value & 0x40) {
                this->blank_on = true;
                this->blank_display();
            } else {
                this->blank_on = false;
            }
        }

        if (bit_changed(this->regs[reg_offset], value, 25)) {
            if (!bit_set(value, 25)) {
                this->disable_crtc_internal();
            } else {
                this->blank_on = false;
            }
        }
        break;
    case ATI_DAC_REGS:
        if (size == 1) { // only byte accesses are allowed for DAC registers
            int dac_reg_addr = ((this->regs[ATI_DAC_CNTL] & 1) << 2) | offset;
            rgb514_write_reg(dac_reg_addr, extract_bits<uint32_t>(value, offset * 8, 8));
        }
        break;
    case ATI_DAC_CNTL:
        // monitor ID is usually accessed using 8bit writes
        if (offset == 3) {
            gpio_dirs   = extract_bits<uint32_t>(value, 27, 3);
            gpio_levels = extract_bits<uint32_t>(value, 24, 3);
            gpio_levels = this->disp_id->read_monitor_sense(gpio_levels, gpio_dirs);
            insert_bits<uint32_t>(value, gpio_levels, 24, 3);
        }
        break;
    case ATI_CONFIG_STAT0:
        return; // prevent writes to this read-only register
    }

    this->regs[reg_offset] = value;
}

uint32_t AtiMach64Gx::read(uint32_t rgn_start, uint32_t offset, int size)
{
    if (rgn_start == this->aperture_base) {
        if (offset < this->vram_size) {
            return read_mem(&this->vram_ptr[offset], size);
        } else if (offset >= this->mm_regs_offset) {
            return BYTESWAP_SIZED(read_reg(offset - this->mm_regs_offset, size), size);
        }
    }

    // memory mapped expansion ROM region
    if (rgn_start == this->exp_rom_addr && offset < this->exp_rom_size) {
        return read_mem(&this->exp_rom_data[offset], size);
    }

    return 0;
}

void AtiMach64Gx::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    if (rgn_start == this->aperture_base) {
        if (offset < this->vram_size) {
            write_mem(&this->vram_ptr[offset], value, size);
        } else if (offset >= this->mm_regs_offset) {
            write_reg(offset - this->mm_regs_offset, BYTESWAP_SIZED(value, size), size);
        }
    }
}

void AtiMach64Gx::enable_crtc_internal()
{
    uint32_t new_width, new_height;

    // check for unsupported modes and fail early
    if (!bit_set(this->regs[ATI_CRTC_GEN_CNTL], 24))
        ABORT_F("%s: VGA not supported", this->name.c_str());

    new_width  = (extract_bits<uint32_t>(this->regs[ATI_CRTC_H_TOTAL_DISP], 16,  8) + 1) * 8;
    new_height =  extract_bits<uint32_t>(this->regs[ATI_CRTC_V_TOTAL_DISP], 16, 11) + 1;

    if (new_width != this->active_width || new_height != this->active_height) {
        this->create_display_window(new_width, new_height);
    }

    // calculate display refresh rate
    this->hori_total = (extract_bits<uint32_t>(this->regs[ATI_CRTC_H_TOTAL_DISP], 0, 9) + 1) * 8;
    this->vert_total = extract_bits<uint32_t>(this->regs[ATI_CRTC_V_TOTAL_DISP], 0, 11) + 1;

    this->refresh_rate = this->pixel_clock / this->hori_total / this->vert_total;

    // specify framebuffer converter
    switch (this->pixel_depth) {
    case 8:
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_8bpp(dst_buf, dst_pitch);
        };
        break;
    default:
        ABORT_F("%s: unsupported pixel depth %d", this->name.c_str(), this->pixel_depth);
    }

    if (this->refresh_task_id) {
        TimerManager::get_instance()->cancel_timer(this->refresh_task_id);
    }

    uint64_t refresh_interval = static_cast<uint64_t>(1.0f / this->refresh_rate * NS_PER_SEC + 0.5);
    this->refresh_task_id = TimerManager::get_instance()->add_cyclic_timer(
        refresh_interval,
        [this]() {
            this->update_screen();
        }
    );

    this->crtc_on = true;

    this->crtc_enable = 1;
}

void AtiMach64Gx::disable_crtc_internal()
{
    this->blank_on = true;
    this->blank_display();
    this->crtc_enable = 0;
}

// ========================== IBM RGB514 related code ==========================
void AtiMach64Gx::rgb514_write_reg(uint8_t reg_addr, uint8_t value)
{
    switch (reg_addr) {
    case Rgb514::CLUT_ADDR_WR:
        this->clut_index = value;
        break;
    case Rgb514::CLUT_DATA:
        this->clut_color[this->comp_index++] = value;
        if (this->comp_index >= 3) {
            this->set_palette_color(this->clut_index, clut_color[0],
                                    clut_color[1], clut_color[2], 0xFF);
            this->clut_index++;
            this->comp_index = 0;
        }
        break;
    case Rgb514::CLUT_MASK:
        if (value != 0xFF) {
            LOG_F(WARNING, "RGB514: pixel mask set to 0x%X", value);
        }
        break;
    case Rgb514::INDEX_LOW:
        this->dac_idx_lo = value;
        break;
    case Rgb514::INDEX_HIGH:
        this->dac_idx_hi = value;
        break;
    case Rgb514::INDEX_DATA:
        this->rgb514_write_ind_reg((this->dac_idx_hi << 8) + this->dac_idx_lo, value);
        break;
    default:
        ABORT_F("RGB514: access to unimplemented register at 0x%X", reg_addr);
    }
}

void AtiMach64Gx::rgb514_write_ind_reg(uint8_t reg_addr, uint8_t value)
{
    this->dac_regs[reg_addr] = value;

    switch (reg_addr) {
    case Rgb514::MISC_CLK_CNTL:
        if (value & PLL_ENAB) {
            if ((this->dac_regs[Rgb514::PLL_CTL_1] & 3) != 1)
                ABORT_F("RGB514: unsupported PLL source");

            int m = 8 >> (this->dac_regs[Rgb514::F0_M0] >> 6);
            int vco_div = (this->dac_regs[Rgb514::F0_M0] & 0x3F) + 65;
            int ref_div = (this->dac_regs[Rgb514::F1_N0] & 0x1F) * m;
            this->pixel_clock = ATI_XTAL * vco_div / ref_div;
            LOG_F(INFO, "RGB514: dot clock set to %f Hz", this->pixel_clock);
        }
        break;
    case Rgb514::PIX_FORMAT:
        if (value == 3) {
            this->pixel_depth = 8;
            // HACK: not the best place for enabling display output!
            this->enable_crtc_internal();
        } else {
            ABORT_F("RGB514: unimplemented pixel format %d", value);
        }
        break;
    }
}

static const DeviceDescription AtiMach64Gx_Descriptor = {
    AtiMach64Gx::create, {}, {}
};

REGISTER_DEVICE(AtiMach64Gx, AtiMach64Gx_Descriptor);
