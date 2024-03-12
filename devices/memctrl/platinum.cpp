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

/** Platinum Memory/Display Controller emulation. */

#include <core/timermanager.h>
#include <devices/deviceregistry.h>
#include <devices/ioctrl/macio.h>
#include <devices/memctrl/platinum.h>
#include <devices/video/displayid.h>
#include <loguru.hpp>
#include <machines/machinebase.h>
#include <memaccess.h>

#include <cinttypes>

using namespace Platinum;

PlatinumCtrl::PlatinumCtrl() : MemCtrlBase(), VideoCtrlBase(640, 480) {
    set_name("Platinum");

    supports_types(HWCompType::MEM_CTRL | HWCompType::MMIO_DEV);

    // add MMIO region for VRAM
    add_mmio_region(VRAM_REGION_BASE, 0x01000000, this);

    // add MMIO region for the configuration and status registers
    add_mmio_region(PLATINUM_IOREG_BASE, 0x500, this);

    // get VRAM size
    this->vram_megs = GET_INT_PROP("gfxmem_size");
    this->vram_size = this->vram_megs << 20;

    // enable half bank access if 1MB VRAM + FB_CONFIG_1[CFG1_FULL_BANKS] = 1
    this->half_bank = !!(this->vram_megs == 1);
    this->half_access = 0;

    // allocate VRAM
    this->vram_ptr = std::unique_ptr<uint8_t[]> (new uint8_t[this->vram_size]);

    // initialize the CPUID register with the following CPU:
    // PowerPC 601 @ 90 MHz, bus frequency: 45 MHz
    this->cpu_id = (0x3001 << 16) | ClkSrc2 | (CpuSpeed2::CPU_90_BUS_45 << 8);

    this->display_id = std::unique_ptr<DisplayID> (new DisplayID());

    // attach DACula RAMDAC
    this->dacula = std::unique_ptr<AppleRamdac>(new AppleRamdac(DacFlavour::DACULA));
    this->dacula->get_clut_entry_cb = [this](uint8_t index, uint8_t *colors) {
        uint8_t a;
        this->get_palette_color(index, colors[0], colors[1], colors[2], a);
    };
    this->dacula->set_clut_entry_cb = [this](uint8_t index, uint8_t *colors) {
        this->set_palette_color(index, colors[0], colors[1], colors[2], 0xFF);
    };
    this->dacula->cursor_ctrl_cb = [this](bool cursor_on) {
        if (cursor_on) {
            this->dacula->measure_hw_cursor(this->fb_ptr - 16);
            this->cursor_ovl_cb = [this](uint8_t *dst_buf, int dst_pitch) {
                this->dacula->draw_hw_cursor(this->fb_ptr - 16,
                    dst_buf, dst_pitch);
            };
        } else {
            this->cursor_ovl_cb = nullptr;
        }
    };
}

int PlatinumCtrl::device_postinit() {
    // attach IOBus Device #2 0xF301B000 ; register DACula with the I/O controller
    GrandCentral* gc_obj = dynamic_cast<GrandCentral*>(gMachineObj->get_comp_by_name("GrandCentral"));
    gc_obj->attach_iodevice(1, this->dacula.get());

    this->int_ctrl = dynamic_cast<InterruptCtrl*>(
        gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
    this->irq_id = this->int_ctrl->register_dev_int(IntSrc::PLATINUM);

    this->vbl_cb = [this](uint8_t irq_line_state) {
        this->update_irq(irq_line_state, SWATCH_INT_VBL);
    };

    return 0;
}

uint32_t PlatinumCtrl::read(uint32_t rgn_start, uint32_t offset, int size) {
    uint32_t value;

    if (rgn_start == VRAM_REGION_BASE) {
        if (offset < this->vram_size) {
            // HACK: half bank configurations should return invalid data
            // for the lower DWORD (in the PPC order!) to be recognized.
            // The simplest way to achieve that is to redirect access
            // to the upper DWORD by setting bit 2 of the address.
            if (this->half_access)
                offset |= 4;
            return read_mem(&this->vram_ptr[offset], size);
        } else {
            LOG_F(WARNING, "%s: read from unmapped aperture address 0x%X",
                  this->name.c_str(), this->fb_addr + offset);
            return (uint32_t)-1;
        }
    }

    switch (offset >> 4) {
    case PlatinumReg::CPU_ID:
        value = this->cpu_id;
        break;
    case PlatinumReg::DRAM_REFRESH:
        value = this->dram_refresh;
        break;
    case PlatinumReg::BANK_0_BASE:
    case PlatinumReg::BANK_1_BASE:
    case PlatinumReg::BANK_2_BASE:
    case PlatinumReg::BANK_3_BASE:
    case PlatinumReg::BANK_4_BASE:
    case PlatinumReg::BANK_5_BASE:
    case PlatinumReg::BANK_6_BASE:
    case PlatinumReg::BANK_7_BASE:
        value = this->bank_base[(offset >> 4) - PlatinumReg::BANK_0_BASE];
        break;
    case PlatinumReg::CACHE_CONFIG:
        value = 0; // report no L2 cache installed
        break;
    case PlatinumReg::FB_BASE_ADDR:
        value = this->fb_addr;
        break;
    case PlatinumReg::MON_ID_SENSE:
        value = (this->mon_sense ^ 7);
        break;
    case PlatinumReg::SWATCH_CONFIG:
        value = this->swatch_config;
        break;
    case PlatinumReg::SWATCH_INT_STAT:
        value = this->swatch_int_stat;
        break;
    case PlatinumReg::CLR_CURSOR_INT:
        this->update_irq(0, SWATCH_INT_CURSOR);
        value = 0;
        break;
    //case PlatinumReg::CLR_ANIM_INT:
    //case PlatinumReg::CLR_VBL_INT:
    //case PlatinumReg::CURSOR_LINE:
    //case PlatinumReg::ANIMATE_LINE:
    //case PlatinumReg::COUNTER_TEST:
       //break;
    case PlatinumReg::SWATCH_HSERR:
    case PlatinumReg::SWATCH_HLFLN:
    case PlatinumReg::SWATCH_HEQ:
    case PlatinumReg::SWATCH_HSP:
    case PlatinumReg::SWATCH_HBWAY:
    case PlatinumReg::SWATCH_HBRST:
    case PlatinumReg::SWATCH_HBP:
    case PlatinumReg::SWATCH_HAL:
    case PlatinumReg::SWATCH_HFP:
    case PlatinumReg::SWATCH_HPIX:
    case PlatinumReg::SWATCH_VHLINE:
    case PlatinumReg::SWATCH_VSYNC:
    case PlatinumReg::SWATCH_VBPEQ:
    case PlatinumReg::SWATCH_VBP:
    case PlatinumReg::SWATCH_VAL:
    case PlatinumReg::SWATCH_VFP:
    case PlatinumReg::SWATCH_VFPEQ:
        value = this->swatch_params[REG_TO_INDEX(offset >> 4)];
        break;
    case PlatinumReg::TIMING_ADJUST:
        value = this->timing_adjust;
        break;
    case PlatinumReg::IRIDIUM_CONFIG:
        value = this->iridium_cfg;
        break;
    case PlatinumReg::POWER_DOWN_CTRL:
        value = this->power_down_ctrl;
        break;
    default:
        LOG_F(WARNING, "%s: unknown register read at offset 0x%X", this->name.c_str(),
              offset);
        value = 0;
    }

    if (size == 4)
        return value;
    else
        return extract_with_wrap_around(value, offset, size);
}

void PlatinumCtrl::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    static uint8_t vid_enable_seq[] = {3, 2, 0};

    if (rgn_start == VRAM_REGION_BASE) {
        if (offset < this->vram_size)
            write_mem(&this->vram_ptr[offset], value, size);
        else
            LOG_F(WARNING, "%s: write to unmapped aperture address 0x%X",
                  this->name.c_str(), this->fb_addr + offset);
        return;
    }

    switch (offset >> 4) {
    case PlatinumReg::ROM_TIMING:
        this->rom_timing = value;
        break;
    case PlatinumReg::DRAM_TIMING:
        this->dram_timing = value;
        break;
    case PlatinumReg::DRAM_REFRESH:
        this->dram_refresh = value;
        break;
    case PlatinumReg::BANK_0_BASE:
    case PlatinumReg::BANK_1_BASE:
    case PlatinumReg::BANK_2_BASE:
    case PlatinumReg::BANK_3_BASE:
    case PlatinumReg::BANK_4_BASE:
    case PlatinumReg::BANK_5_BASE:
    case PlatinumReg::BANK_6_BASE:
    case PlatinumReg::BANK_7_BASE:
        this->bank_base[(offset >> 4) - PlatinumReg::BANK_0_BASE] = value;
        break;
    case PlatinumReg::FB_BASE_ADDR:
        this->fb_addr   = value;
        this->fb_offset = value & 0x3FFFFF;
        break;
    case PlatinumReg::ROW_WORDS:
        this->row_words = value & ~7;
        break;
    case PlatinumReg::CLOCK_DIVISOR:
        this->clock_divisor = value;
        break;
    case PlatinumReg::FB_CONFIG_1:
        this->fb_config_1 = value;
        this->half_bank = !!(this->vram_megs == 1 && (value & CFG1_FULL_BANKS));
        break;
    case PlatinumReg::FB_CONFIG_2:
        this->fb_config_2 = value;
        break;
    case PlatinumReg::VMEM_PAGE_MODE:
        this->vmem_fp_mode = value;
        break;
    case PlatinumReg::MON_ID_SENSE:
        value &= 7;
        this->mon_sense = this->display_id->read_monitor_sense(value, value ^ 7)
            << (value ^ 7);
        break;
    case PlatinumReg::FB_RESET:

        if (value == 7 && this->crtc_on) {
            LOG_F(INFO, "%s: video disabled", this->name.c_str());
            this->reset_step = 0;
        } else if (value == vid_enable_seq[this->reset_step]) {
            if (++this->reset_step >= 3) {
                if (this->fb_config_1 & CFG1_VID_ENABLE) {
                    LOG_F(INFO, "%s: video enabled", this->name.c_str());
                    this->enable_display();
                } else {
                    this->blank_display();
                }
                this->reset_step = 0;
            }
        } else
            this->reset_step = 0;
        this->fb_reset = value;
        this->half_access = !!(this->half_bank && value == 6);
        break;
    case PlatinumReg::VRAM_REFRESH:
        this->vram_refresh = value;
        break;
    case PlatinumReg::SWATCH_CONFIG:
        this->swatch_config = value;
        break;
    case PlatinumReg::SWATCH_INT_MASK:
        this->swatch_int_mask = value;
        if (this->swatch_int_mask & SWATCH_INT_VBL)
            LOG_F(INFO, "%s: VBL interrupt enabled", this->name.c_str());
        break;
    case PlatinumReg::CURSOR_LINE:
        this->cursor_line = value;
        if (this->swatch_int_mask & SWATCH_INT_CURSOR)
            this->enable_cursor_int();
        break;
    //case PlatinumReg::CLR_CURSOR_INT:
    //case PlatinumReg::CLR_ANIM_INT:
    //case PlatinumReg::CLR_VBL_INT:
    //case PlatinumReg::CURSOR_LINE:
    //case PlatinumReg::ANIMATE_LINE:
    //case PlatinumReg::COUNTER_TEST:
        //break;
    case PlatinumReg::SWATCH_HSERR:
    case PlatinumReg::SWATCH_HLFLN:
    case PlatinumReg::SWATCH_HEQ:
    case PlatinumReg::SWATCH_HSP:
    case PlatinumReg::SWATCH_HBWAY:
    case PlatinumReg::SWATCH_HBRST:
    case PlatinumReg::SWATCH_HBP:
    case PlatinumReg::SWATCH_HAL:
    case PlatinumReg::SWATCH_HFP:
    case PlatinumReg::SWATCH_HPIX:
    case PlatinumReg::SWATCH_VHLINE:
    case PlatinumReg::SWATCH_VSYNC:
    case PlatinumReg::SWATCH_VBPEQ:
    case PlatinumReg::SWATCH_VBP:
    case PlatinumReg::SWATCH_VAL:
    case PlatinumReg::SWATCH_VFP:
    case PlatinumReg::SWATCH_VFPEQ:
        this->swatch_params[REG_TO_INDEX(offset >> 4)] = value;
        break;
    case PlatinumReg::TIMING_ADJUST:
        this->timing_adjust = value;
        break;
    case PlatinumReg::IRIDIUM_CONFIG:
        if (!(value & 1))
            LOG_F(ERROR, "%s: little-endian system bus is not implemented", this->name.c_str());
        this->iridium_cfg = (this->iridium_cfg & ~7) | (value & 7);
        break;
    case PlatinumReg::POWER_DOWN_CTRL:
        this->power_down_ctrl = value;
        if (value & 1)
            LOG_F(INFO, "%s: power down mode enabled", this->name.c_str());
        break;
    default:
        LOG_F(WARNING, "%s: unknown register write at offset 0x%X", this->name.c_str(),
              offset);
    }
}

void PlatinumCtrl::insert_ram_dimm(int slot_num, uint32_t capacity) {
    if (slot_num < 0 || slot_num >= 4) {
        ABORT_F("%s: invalid DIMM slot %d", this->name.c_str(), slot_num);
    }

    switch (capacity) {
    case DRAM_CAP_2MB:
    case DRAM_CAP_4MB:
    case DRAM_CAP_8MB:
    case DRAM_CAP_16MB:
    case DRAM_CAP_32MB:
    case DRAM_CAP_64MB:
        this->bank_size[slot_num * 2 + 0] = capacity;
        break;
    case DRAM_CAP_128MB:
        this->bank_size[slot_num * 2 + 0] = DRAM_CAP_64MB;
        this->bank_size[slot_num * 2 + 1] = DRAM_CAP_64MB;
        break;
    default:
        ABORT_F("%s: unsupported DRAM capacity %d", this->name.c_str(), capacity);
    }
}

void PlatinumCtrl::map_phys_ram() {
    uint32_t total_ram = 0;

    for (int i = 0; i < 8; i++) {
        total_ram += this->bank_size[i];
    }

    if (total_ram > DRAM_CAP_64MB) {
        ABORT_F("%s: RAM bigger than 64MB not supported yet", this->name.c_str());
    }

    if (!add_ram_region(0x00000000, total_ram)) {
        ABORT_F("%s: could not allocate RAM storage", this->name.c_str());
    }
}

// ====================== Framebuffer controller stuff =======================
void PlatinumCtrl::enable_display() {
    int clock_divisor = this->dacula->get_clock_div();

    this->pixel_clock = this->dacula->get_dot_freq();

    // calculate active_width and active_height from Swatch parameters
    int new_width  = swatch_params[REG_TO_INDEX(PlatinumReg::SWATCH_HFP)] -
                     swatch_params[REG_TO_INDEX(PlatinumReg::SWATCH_HAL)];
    int new_height = swatch_params[REG_TO_INDEX(PlatinumReg::SWATCH_VFP)] -
                     swatch_params[REG_TO_INDEX(PlatinumReg::SWATCH_VAL)];

    this->hori_blank = swatch_params[REG_TO_INDEX(PlatinumReg::SWATCH_HAL)] +
                      (swatch_params[REG_TO_INDEX(PlatinumReg::SWATCH_HSP)] -
                       swatch_params[REG_TO_INDEX(PlatinumReg::SWATCH_HFP)]);

    new_width        *= clock_divisor;
    this->hori_blank *= clock_divisor;

    this->vert_blank = swatch_params[REG_TO_INDEX(PlatinumReg::SWATCH_VAL)] +
                      (swatch_params[REG_TO_INDEX(PlatinumReg::SWATCH_VSYNC)] -
                       swatch_params[REG_TO_INDEX(PlatinumReg::SWATCH_VFP)]);

    if (!(this->fb_config_1 & CFG1_INTERLACE)) {
        new_height       >>= 1;
        this->vert_blank >>= 1;
    }

    this->active_width  = new_width;
    this->active_height = new_height;

    this->hori_total = this->hori_blank + new_width;
    this->vert_total = this->vert_blank + new_height;

    // set framebuffer parameters
    this->fb_ptr   = &this->vram_ptr[this->fb_offset] + 16;
    this->fb_pitch = this->row_words;

    this->pixel_depth = this->dacula->get_pix_width();

    // attach framebuffer conversion routine
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
        LOG_F(ERROR, "%s: invalid pixel width %d", this->name.c_str(), this->pixel_depth);
    }

    this->dacula->set_fb_parameters(this->active_width, this->active_height, this->fb_pitch);

    this->stop_refresh_task();

    this->refresh_rate = (double)(this->pixel_clock) / (this->hori_total * this->vert_total);
    this->start_refresh_task();

    LOG_F(INFO, "%s: video width=%d, height=%d", this->name.c_str(), new_width, new_height);
    LOG_F(INFO, "%s: refresh rate set to %f Hz", this->name.c_str(), this->refresh_rate);

    this->blank_on = false;
    this->crtc_on = true;
}

void PlatinumCtrl::enable_cursor_int() {
    if (!(this->swatch_int_mask & SWATCH_INT_CURSOR))
        return;

    uint64_t cursor_int_freq = static_cast<uint64_t>((1.0f / (double)this->pixel_clock) *
        this->hori_total * this->cursor_line * NS_PER_SEC + 0.5f);
    LOG_F(INFO, "%s: cursor interrupt frequency %lld ns", this->name.c_str(),
        cursor_int_freq);

    if (this->cursor_task_id)
        TimerManager::get_instance()->cancel_timer(this->cursor_task_id);

    this->cursor_task_id = TimerManager::get_instance()->add_cyclic_timer(
        cursor_int_freq,
        [this]() {
            this->update_irq(1, SWATCH_INT_CURSOR); // generate cursor interrupt
        }
    );
}

void PlatinumCtrl::update_irq(uint8_t irq_line_state, uint8_t irq_mask) {
    if (irq_line_state != !!(this->swatch_int_stat & irq_mask)) {
        if (irq_line_state)
            this->swatch_int_stat |= irq_mask;
        else
            this->swatch_int_stat &= ~irq_mask;

        if (this->swatch_int_mask & irq_mask)
            this->int_ctrl->ack_int(this->irq_id, irq_line_state);
    }
}

// ========================== Device registry stuff ==========================
static const PropMap Platinum_Properties = {
    {"gfxmem_size",
        new IntProperty(1, vector<uint32_t>({1, 2, 4}))},
    {"mon_id",
        new StrProperty("HiRes12-14in")},
};

static const DeviceDescription Platinum_Descriptor = {
    PlatinumCtrl::create, {}, Platinum_Properties
};

REGISTER_DEVICE(Platinum, Platinum_Descriptor);
