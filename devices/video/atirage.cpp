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
    #define one_reg_name(x) {ATI_ ## x, #x}
    one_reg_name(CRTC_H_TOTAL_DISP),
    one_reg_name(CRTC_H_SYNC_STRT_WID),
    one_reg_name(CRTC_V_TOTAL_DISP),
    one_reg_name(CRTC_V_SYNC_STRT_WID),
    one_reg_name(CRTC_VLINE_CRNT_VLINE),
    one_reg_name(CRTC_OFF_PITCH),
    one_reg_name(CRTC_INT_CNTL),
    one_reg_name(CRTC_GEN_CNTL),
    one_reg_name(DSP_CONFIG),
    one_reg_name(DSP_ON_OFF),
    one_reg_name(MEM_BUF_CNTL),
    one_reg_name(MEM_ADDR_CFG),
    one_reg_name(OVR_CLR),
    one_reg_name(OVR_WID_LEFT_RIGHT),
    one_reg_name(OVR_WID_TOP_BOTTOM),
    one_reg_name(CUR_CLR0),
    one_reg_name(CUR_CLR1),
    one_reg_name(CUR_OFFSET),
    one_reg_name(CUR_HORZ_VERT_POSN),
    one_reg_name(CUR_HORZ_VERT_OFF),
    one_reg_name(GP_IO),
    one_reg_name(HW_DEBUG),
    one_reg_name(SCRATCH_REG0),
    one_reg_name(SCRATCH_REG1),
    one_reg_name(SCRATCH_REG2),
    one_reg_name(SCRATCH_REG3),
    one_reg_name(CLOCK_CNTL),
    one_reg_name(BUS_CNTL),
    one_reg_name(EXT_MEM_CNTL),
    one_reg_name(MEM_CNTL),
    one_reg_name(DAC_REGS),
    one_reg_name(DAC_CNTL),
    one_reg_name(GEN_TEST_CNTL),
    one_reg_name(CUSTOM_MACRO_CNTL),
    one_reg_name(CONFIG_CHIP_ID),
    one_reg_name(CONFIG_STAT0),
    one_reg_name(SRC_CNTL),
    one_reg_name(SCALE_3D_CNTL),
    one_reg_name(FIFO_STAT),
    one_reg_name(GUI_STAT),
    one_reg_name(MPP_CONFIG),
    one_reg_name(MPP_STROBE_SEQ),
    one_reg_name(MPP_ADDR),
    one_reg_name(MPP_DATA),
    one_reg_name(TVO_CNTL),
    one_reg_name(SETUP_CNTL),
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
    for (int i = 0; i < this->aperture_count; i++) {
        this->bars_cfg[i] = (uint32_t)(-this->aperture_size[i] | this->aperture_flag[i]);
    }
    this->finish_config_bars();

    this->pci_notify_bar_change = [this](int bar_num) {
        this->notify_bar_change(bar_num);
    };

    // stuff default values into chip registers
    this->regs[ATI_CONFIG_CHIP_ID] = (asic_id << ATI_CFG_CHIP_MAJOR) | (dev_id << ATI_CFG_CHIP_TYPE);

    // initialize display identification
    this->disp_id = std::unique_ptr<DisplayID> (new DisplayID());

    uint8_t mon_code = this->disp_id->read_monitor_sense(0, 0);

    this->regs[ATI_GP_IO] = ((mon_code & 6) << 11) | ((mon_code & 1) << 8);
    insert_bits<uint32_t>(this->regs[ATI_GUI_STAT], 32, ATI_FIFO_CNT, ATI_FIFO_CNT_size);
    set_bit(regs[ATI_CRTC_GEN_CNTL], ATI_CRTC_DISPLAY_DIS); // because blank_on is true
}

void ATIRage::change_one_bar(uint32_t &aperture, uint32_t aperture_size, uint32_t aperture_new, int bar_num) {
    if (aperture != aperture_new) {
        if (aperture)
            this->host_instance->pci_unregister_mmio_region(aperture, aperture_size, this);

        aperture = aperture_new;
        if (aperture)
            this->host_instance->pci_register_mmio_region(aperture, aperture_size, this);

        LOG_F(INFO, "%s: aperture[%d] set to 0x%08X", this->name.c_str(), bar_num, aperture);
    }
}

void ATIRage::notify_bar_change(int bar_num)
{
    switch (bar_num) {
    case 0:
        change_one_bar(this->aperture_base[bar_num], this->aperture_size[bar_num] - this->vram_size, this->bars[bar_num] & ~15, bar_num);
        break;
    case 2:
        change_one_bar(this->aperture_base[bar_num], this->aperture_size[bar_num], this->bars[bar_num] & ~15, bar_num);
        break;
    case 1:
        this->aperture_base[1] = this->bars[bar_num] & ~3;
        LOG_F(INFO, "%s: I/O space address set to 0x%08X", this->name.c_str(), this->aperture_base[1]);
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

const char* ATIRage::get_reg_name(uint32_t reg_num) {
    auto iter = mach64_reg_names.find(reg_num);
    if (iter != mach64_reg_names.end()) {
        return iter->second.c_str();
    } else {
        return "unknown Mach64 register";
    }
}

uint32_t ATIRage::read_reg(uint32_t reg_offset, uint32_t size) {
    uint32_t reg_num = reg_offset >> 2;
    uint32_t offset = reg_offset & 3;
    uint64_t result = this->regs[reg_num];

    switch (reg_num) {
    case ATI_CLOCK_CNTL:
        if (offset <= 2 && offset + size > 2) {
            uint8_t pll_addr = extract_bits<uint64_t>(result, ATI_PLL_ADDR, ATI_PLL_ADDR_size);
            insert_bits<uint64_t>(result, this->plls[pll_addr], ATI_PLL_DATA, ATI_PLL_DATA_size);
        }
        break;
    case ATI_DAC_REGS:
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
                get_palette_color(this->dac_rd_index, color_buf[0],
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
    }

    if (offset || size != 4) { // slow path
        if ((offset + size) > 4) {
            result |= (uint64_t)(this->regs[reg_num + 1]) << 32;
        }
        result = extract_bits<uint64_t>(result, offset * 8, size * 8);
    }

    return static_cast<uint32_t>(result);
}

void ATIRage::write_reg(uint32_t reg_offset, uint32_t value, uint32_t size) {
    uint32_t reg_num = reg_offset >> 2;
    uint32_t offset = reg_offset & 3;
    uint32_t old_value = this->regs[reg_num];
    uint32_t new_value;

    if (offset || size != 4) { // slow path
        if ((offset + size) > 4) {
            ABORT_F("%s: unaligned DWORD writes not implemented", this->name.c_str());
        }
        uint64_t val = old_value;
        insert_bits<uint64_t>(val, value, offset * 8, size * 8);
        value = static_cast<uint32_t>(val);
    }

    switch (reg_num) {
    case ATI_CRTC_H_TOTAL_DISP:
        new_value = value;
        LOG_F(9, "%s: ATI_CRTC_H_TOTAL_DISP set to 0x%08X", this->name.c_str(), value);
        break;
    case ATI_CRTC_VLINE_CRNT_VLINE:
        new_value = old_value;
        insert_bits<uint32_t>(new_value, value, ATI_CRTC_VLINE, ATI_CRTC_VLINE_size);
        break;
    case ATI_CRTC_OFF_PITCH:
        new_value = value;
        if (old_value != new_value) {
            this->regs[reg_num] = new_value;
            this->crtc_update();
        }
        break;
    case ATI_CRTC_INT_CNTL: {
        uint32_t bits_read_only =
            (1 << ATI_CRTC_VBLANK) |
            (1 << ATI_CRTC_VLINE_SYNC) |
            (1 << ATI_CRTC_FRAME) |
#if 1
#else
            (1 << ATI_CRTC2_VBLANK) |
            (1 << ATI_CRTC2_VLINE_SYNC) |
#endif
            0;

        uint32_t bits_AK =
            (1 << ATI_CRTC_VBLANK_INT_AK) |
            (1 << ATI_CRTC_VLINE_INT_AK) |
#if 1
            (1 << ATI_VIDEOIN_EVEN_INT_AK) |
            (1 << ATI_VIDEOIN_ODD_INT_AK) |
            (1 << ATI_OVERLAY_EOF_INT_AK) |
            (1 << ATI_VMC_EC_INT_AK) |
#else
            (1 << ATI_SNAPSHOT_INT_AK) |
            (1 << ATI_I2C_INT_AK) |
            (1 << ATI_CRTC2_VBLANK_INT_AK) |
            (1 << ATI_CRTC2_VLINE_INT_AK) |
            (1 << ATI_CUPBUF0_INT_AK) |
            (1 << ATI_CUPBUF1_INT_AK) |
            (1 << ATI_OVERLAY_EOF_INT_AK) |
            (1 << ATI_ONESHOT_CAP_INT_AK) |
            (1 << ATI_BUSMASTER_EOL_INT_AK) |
            (1 << ATI_GP_INT_AK) |
            (1 << ATI_SNAPSHOT2_INT_AK) |
            (1 << ATI_VBLANK_BIT2_INT_AK) |
#endif
            0;
/*
        uint32_t bits_EN =
            (1 << ATI_CRTC_VBLANK_INT_EN) |
            (1 << ATI_CRTC_VLINE_INT_EN) |
#if 1
            (1 << ATI_VIDEOIN_EVEN_INT_EN) |
            (1 << ATI_VIDEOIN_ODD_INT_EN) |
            (1 << ATI_OVERLAY_EOF_INT_EN) |
            (1 << ATI_VMC_EC_INT_EN) |
#else
            (1 << ATI_SNAPSHOT_INT_EN) |
            (1 << ATI_I2C_INT_EN) |
            (1 << ATI_CRTC2_VBLANK_INT_EN) |
            (1 << ATI_CRTC2_VLINE_INT_EN) |
            (1 << ATI_CUPBUF0_INT_EN) |
            (1 << ATI_CUPBUF1_INT_EN) |
            (1 << ATI_OVERLAY_EOF_INT_EN) |
            (1 << ATI_ONESHOT_CAP_INT_EN) |
            (1 << ATI_BUSMASTER_EOL_INT_EN) |
            (1 << ATI_GP_INT_EN) |
            (1 << ATI_SNAPSHOT2_INT_EN) |
#endif
            0;
*/
        uint32_t bits_AKed = bits_AK & value; // AK bits that are to be AKed
        uint32_t bits_not_AKed = bits_AK & ~value; // AK bits that are not to be AKed

        new_value = value & ~bits_AKed; // clear the AKed bits
        bits_read_only |= bits_not_AKed; // the not AKed bits will remain unchanged

        new_value = (old_value & bits_read_only) | (new_value & ~bits_read_only);
        break;
    }
    case ATI_CRTC_GEN_CNTL:
        new_value = value;
        if (bit_changed(old_value, new_value, ATI_CRTC_DISPLAY_DIS)) {
            if (bit_set(new_value, ATI_CRTC_DISPLAY_DIS)) {
                this->blank_on = true;
                this->blank_display();
            } else {
                this->blank_on = false;
            }
        }

        this->regs[reg_num] = new_value;
        if (bit_changed(old_value, new_value, ATI_CRTC_ENABLE)) {
            if (bit_set(new_value, ATI_CRTC_ENABLE) &&
                !bit_set(new_value, ATI_CRTC_DISPLAY_DIS)) {
                this->crtc_update();
            }
        }
        break;
    case ATI_GP_IO:
        new_value = value;
        if (offset <= 1 && offset + size > 1) {
            uint8_t gpio_levels = (new_value >> 8) & 0xFFU;
            gpio_levels = ((gpio_levels & 0x30) >> 3) | (gpio_levels & 1);
            uint8_t gpio_dirs = (new_value >> 24) & 0xFFU;
            gpio_dirs = ((gpio_dirs & 0x30) >> 3) | (gpio_dirs & 1);
            gpio_levels = this->disp_id->read_monitor_sense(gpio_levels, gpio_dirs);
            insert_bits<uint32_t>(new_value,
                                ((gpio_levels & 6) << 3) | (gpio_levels & 1), 8, 8);
        }
        break;
    case ATI_CLOCK_CNTL:
    {
        uint32_t bits_write_only =
            (1 << ATI_CLOCK_STROBE);

        new_value = value & ~bits_write_only; // clear the write only bits

        uint8_t pll_addr = extract_bits<uint32_t>(new_value, ATI_PLL_ADDR, ATI_PLL_ADDR_size);
        if (offset <= 2 && offset + size > 2 && bit_set(new_value, ATI_PLL_WR_EN)) {
            uint8_t pll_data = extract_bits<uint32_t>(new_value, ATI_PLL_DATA, ATI_PLL_DATA_size);
            this->plls[pll_addr] = pll_data;
            LOG_F(9, "%s: PLL #%d set to 0x%02X", this->name.c_str(), pll_addr, pll_data);
        }
        else {
            insert_bits<uint32_t>(new_value, this->plls[pll_addr], ATI_PLL_DATA, ATI_PLL_DATA_size);
        }
        break;
    }
    case ATI_DAC_REGS:
        new_value = old_value; // no change
        switch (reg_offset) {
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
        break;
    case ATI_GEN_TEST_CNTL:
        new_value = value;
        if (bit_changed(old_value, new_value, ATI_GEN_CUR_ENABLE)) {
            if (bit_set(new_value, ATI_GEN_CUR_ENABLE))
                this->setup_hw_cursor();
            else
                this->cursor_on = false;
        }
        if (bit_changed(old_value, new_value, ATI_GEN_GUI_RESETB)) {
            if (!bit_set(new_value, ATI_GEN_GUI_RESETB))
                LOG_F(9, "%s: reset GUI engine", this->name.c_str());
        }
        if (bit_changed(old_value, new_value, ATI_GEN_SOFT_RESET)) {
            if (bit_set(new_value, ATI_GEN_SOFT_RESET))
                LOG_F(9, "%s: reset memory controller", this->name.c_str());
        }
        if (new_value & 0xFFFFFC00) {
            LOG_F(WARNING, "%s: unhandled GEN_TEST_CNTL state=0x%X",
                  this->name.c_str(), new_value);
        }
        break;
    case ATI_CONFIG_CHIP_ID:
        new_value = old_value; // prevent writes to this read-only register
        break;
    case ATI_CONFIG_STAT0:
    {
        uint32_t bits_read_only =
#if 1
#else
            (1 << ATI_MACROVISION_ENABLE) |
            (1 << ATI_ARITHMOS_ENABLE) |
#endif
            0;

        new_value = value;
        new_value = (old_value & bits_read_only) | (new_value & ~bits_read_only);
        break;
    }
    default:
        new_value = value;
        break;
    }

    this->regs[reg_num] = new_value;
}

bool ATIRage::io_access_allowed(uint32_t offset) {
    if (offset >= this->aperture_base[1] && offset < (this->aperture_base[1] + 0x100)) {
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

    *res = BYTESWAP_SIZED(this->read_reg(offset - this->aperture_base[1], size), size);
    return true;
}

bool ATIRage::pci_io_write(uint32_t offset, uint32_t value, uint32_t size) {
    if (!this->io_access_allowed(offset)) {
        return false;
    }

    this->write_reg(offset - this->aperture_base[1], BYTESWAP_SIZED(value, size), size);
    return true;
}

uint32_t ATIRage::read(uint32_t rgn_start, uint32_t offset, int size)
{
    if (rgn_start == this->aperture_base[0] && offset < this->aperture_size[0]) {
        if (offset < this->vram_size) { // little-endian VRAM region
            return read_mem(&this->vram_ptr[offset], size);
        }
        if (offset >= BE_FB_OFFSET) { // big-endian VRAM region
            return read_mem(&this->vram_ptr[offset - BE_FB_OFFSET], size);
        }
        //if (!bit_set(this->regs[ATI_BUS_CNTL], ATI_BUS_APER_REG_DIS)) {
            if (offset >= MM_REGS_0_OFF) { // memory-mapped registers, block 0
                return BYTESWAP_SIZED(this->read_reg(offset & 0x3FF, size), size);
            }
            if (offset >= MM_REGS_1_OFF
                //&& bit_set(this->regs[ATI_BUS_CNTL], ATI_BUS_EXT_REG_EN)
            ) { // memory-mapped registers, block 1
                return BYTESWAP_SIZED(this->read_reg((offset & 0x3FF) + 0x400, size), size);
            }
        //}
        LOG_F(WARNING, "%s: read  unmapped aperture[0] region %08x.%c", this->name.c_str(), offset, SIZE_ARG(size));
        return 0;
    }

    if (rgn_start == this->aperture_base[2] && offset < this->aperture_size[2]) {
        LOG_F(WARNING, "%s: read  unmapped aperture[2] region %08x.%c", this->name.c_str(), offset, SIZE_ARG(size));
        return 0;
    }

    LOG_F(WARNING, "%s: read  unmapped aperture region %08x.%c", this->name.c_str(), offset, SIZE_ARG(size));
    return 0;
}

void ATIRage::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    if (rgn_start == this->aperture_base[0] && offset < this->aperture_size[0]) {
        if (offset < this->vram_size) { // little-endian VRAM region
            return write_mem(&this->vram_ptr[offset], value, size);
        }
        if (offset >= BE_FB_OFFSET) { // big-endian VRAM region
            return write_mem(&this->vram_ptr[offset & (BE_FB_OFFSET - 1)], value, size);
        }
        //if (!bit_set(this->regs[ATI_BUS_CNTL], ATI_BUS_APER_REG_DIS)) {
            if (offset >= MM_REGS_0_OFF) { // memory-mapped registers, block 0
                return this->write_reg(offset & 0x3FF, BYTESWAP_SIZED(value, size), size);
            }
            if (offset >= MM_REGS_1_OFF
                //&& bit_set(this->regs[ATI_BUS_CNTL], ATI_BUS_EXT_REG_EN)
            ) { // memory-mapped registers, block 1
                return this->write_reg((offset & 0x3FF) + 0x400, BYTESWAP_SIZED(value, size), size);
            }
        //}
        LOG_F(WARNING, "%s: write unmapped aperture[0] region %08x.%c = %0*x", this->name.c_str(), offset, SIZE_ARG(size), size * 2, value);
        return;
    }

    if (rgn_start == this->aperture_base[2] && offset < this->aperture_size[2]) {
        LOG_F(WARNING, "%s: write unmapped aperture[2] region %08x.%c = %0*x", this->name.c_str(), offset, SIZE_ARG(size), size * 2, value);
        return;
    }

    LOG_F(WARNING, "%s: write unmapped aperture region %08x.%c = %0*x", this->name.c_str(), offset, SIZE_ARG(size), size * 2, value);
}

float ATIRage::calc_pll_freq(int scale, int fb_div) {
    return (ATI_XTAL * scale * fb_div) / this->plls[PLL_REF_DIV];
}

void ATIRage::verbose_pixel_format(int crtc_index) {
    if (crtc_index) {
        LOG_F(ERROR, "CRTC2 not supported yet");
        return;
    }

    uint32_t pix_fmt = extract_bits<uint32_t>(this->regs[ATI_CRTC_GEN_CNTL], ATI_CRTC_PIX_WIDTH, ATI_CRTC_PIX_WIDTH_size);

    const char* what = "Pixel format:";

    switch (pix_fmt) {
    case 1:
        LOG_F(INFO, "%s 4 bpp with DAC palette", what);
        break;
    case 2:
        // check the undocumented DAC_DIRECT bit
        if (bit_set(this->regs[ATI_DAC_CNTL], ATI_DAC_DIRECT)) {
            LOG_F(INFO, "%s 8 bpp direct color (RGB332)", what);
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
        LOG_F(ERROR, "%s: CRTC pixel format %d not supported", this->name.c_str(), pix_fmt);
    }
}

void ATIRage::crtc_update() {
    uint32_t new_width, new_height, new_htotal, new_vtotal;

    // check for unsupported modes and fail early
    if (!bit_set(this->regs[ATI_CRTC_GEN_CNTL], ATI_CRTC_EXT_DISP_EN))
        ABORT_F("%s: VGA not supported", this->name.c_str());

    if ((this->plls[PLL_VCLK_CNTL] & 3) != 3)
        ABORT_F("%s: VLCK source != VPLL", this->name.c_str());

    bool need_recalc = false;

    new_width  = (extract_bits<uint32_t>(this->regs[ATI_CRTC_H_TOTAL_DISP], ATI_CRTC_H_DISP, ATI_CRTC_H_DISP_size) + 1) * 8;
    new_height = extract_bits<uint32_t>(this->regs[ATI_CRTC_V_TOTAL_DISP], ATI_CRTC_V_DISP, ATI_CRTC_V_DISP_size) + 1;

    if (new_width != this->active_width || new_height != this->active_height) {
        this->create_display_window(new_width, new_height);
        need_recalc = true;
    }

    new_htotal = (extract_bits<uint32_t>(this->regs[ATI_CRTC_H_TOTAL_DISP], ATI_CRTC_H_TOTAL, ATI_CRTC_H_TOTAL_size) + 1) * 8;
    new_vtotal = extract_bits<uint32_t>(this->regs[ATI_CRTC_V_TOTAL_DISP], ATI_CRTC_V_TOTAL, ATI_CRTC_V_TOTAL_size) + 1;

    if (new_htotal != this->hori_total || new_vtotal != this->vert_total) {
        this->hori_total = new_htotal;
        this->vert_total = new_vtotal;
        need_recalc = true;
    }

    uint32_t new_vert_blank = new_vtotal - new_height;
    if (new_vert_blank != this->vert_blank) {
        this->vert_blank = vert_blank;
        need_recalc = true;
    }

    int new_pixel_format = extract_bits<uint32_t>(this->regs[ATI_CRTC_GEN_CNTL], ATI_CRTC_PIX_WIDTH, ATI_CRTC_PIX_WIDTH_size);
    if (new_pixel_format != this->pixel_format) {
        this->pixel_format = new_pixel_format;
        need_recalc = true;
    }

    // look up which VPLL ouput is requested
    int clock_sel = extract_bits<uint32_t>(this->regs[ATI_CLOCK_CNTL], ATI_CLOCK_SEL, ATI_CLOCK_SEL_size);

    // calculate VPLL output frequency
    float vpll_freq = calc_pll_freq(2, this->plls[VCLK0_FB_DIV + clock_sel]);

    // calculate post divider's index
    // NOTE: post divider's index has been extended by an additional
    // bit in Rage Pro. This bit is resided in PLL_EXT_CNTL register.
    int post_div_idx = ((this->plls[PLL_EXT_CNTL] >> (clock_sel + 2)) & 4) |
                       ((this->plls[VCLK_POST_DIV] >> (clock_sel * 2)) & 3);

    // pixel clock = source_freq / post_div
    float new_pixel_clock = vpll_freq / mach64_post_div[post_div_idx];
    if (new_pixel_clock != this->pixel_clock) {
        this->pixel_clock = new_pixel_clock;
        need_recalc = true;
    }

    if (!need_recalc)
        return;

    // calculate display refresh rate
    this->refresh_rate = pixel_clock / this->hori_total / this->vert_total;

    if (this->refresh_rate < 24 || this->refresh_rate > 120) {
        LOG_F(ERROR, "%s: Refresh rate is weird. Will try 60 Hz", this->name.c_str());
        this->refresh_rate = 60;
        this->pixel_clock = this->refresh_rate * this->hori_total / this->vert_total;
    }

    // set up frame buffer converter
    switch (this->pixel_format) {
    case 1:
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_4bpp_indexed(dst_buf, dst_pitch);
        };
        break;
    case 2:
        if (bit_set(this->regs[ATI_DAC_CNTL], ATI_DAC_DIRECT)) {
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
        LOG_F(ERROR, "%s: unsupported pixel format %d", this->name.c_str(), this->pixel_format);
    }

    static uint8_t bits_per_pixel[8] = {0, 4, 8, 16, 16, 24, 32, 0};

    this->fb_pitch = extract_bits<uint32_t>(this->regs[ATI_CRTC_OFF_PITCH], ATI_CRTC_PITCH, ATI_CRTC_PITCH_size) *
        (bits_per_pixel[this->pixel_format & 7] * 8) >> 3;

    this->fb_ptr = &this->vram_ptr[extract_bits<uint32_t>(this->regs[ATI_CRTC_OFF_PITCH], ATI_CRTC_OFFSET, ATI_CRTC_OFFSET_size) * 8];

    LOG_F(INFO, "%s: primary CRT controller enabled:", this->name.c_str());
    LOG_F(INFO, "Video mode: %s",
         bit_set(this->regs[ATI_CRTC_GEN_CNTL], ATI_CRTC_EXT_DISP_EN) ? "extended" : "VGA");
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

    int vert_offset = extract_bits<uint32_t>(this->regs[ATI_CUR_HORZ_VERT_OFF], ATI_CUR_VERT_OFF, ATI_CUR_VERT_OFF_size);

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
                    WRITE_DWORD_BE_A(dst_row, 0x0000007F);
                    break;
                }
            }
        }
    }
}

void ATIRage::get_cursor_position(int& x, int& y) {
    x = extract_bits<uint32_t>(this->regs[ATI_CUR_HORZ_VERT_POSN], ATI_CUR_HORZ_POSN, ATI_CUR_HORZ_POSN_size);
    y = extract_bits<uint32_t>(this->regs[ATI_CUR_HORZ_VERT_POSN], ATI_CUR_VERT_POSN, ATI_CUR_VERT_POSN_size);
}

int ATIRage::device_postinit()
{
    this->vbl_cb = [this](uint8_t irq_line_state) {
        insert_bits<uint32_t>(this->regs[ATI_CRTC_INT_CNTL], irq_line_state, ATI_CRTC_VBLANK, 1);
        if (irq_line_state) {
            set_bit(this->regs[ATI_CRTC_INT_CNTL], ATI_CRTC_VBLANK_INT);
            set_bit(this->regs[ATI_CRTC_INT_CNTL], ATI_CRTC_VLINE_INT);
#if 1
#else
            set_bit(this->regs[ATI_CRTC_GEN_CNTL], ATI_CRTC_VSYNC_INT);
#endif
        }

        bool do_interrupt =
            bit_set(this->regs[ATI_CRTC_INT_CNTL], ATI_CRTC_VBLANK_INT_EN) ||
            bit_set(this->regs[ATI_CRTC_INT_CNTL], ATI_CRTC_VLINE_INT_EN) ||
#if 1
#else
            bit_set(this->regs[ATI_CRTC_GEN_CNTL], ATI_CRTC_VSYNC_INT_EN) ||
#endif
            0;

        if (do_interrupt) {
            this->pci_interrupt(irq_line_state);
        }
    };
    return 0;
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
