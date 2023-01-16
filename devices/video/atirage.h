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

#ifndef ATI_RAGE_H
#define ATI_RAGE_H

#include <devices/common/pci/pcidevice.h>
#include <devices/video/atimach64defs.h>
#include <devices/video/displayid.h>
#include <devices/video/videoctrl.h>

#include <cinttypes>
#include <memory>

/* Mach64 PLL register indices. */
enum {
    PLL_REF_DIV     =  2, // reference divider, same for all Mach64 clocks
    PLL_VCLK_CNTL   =  5,
    VCLK_POST_DIV   =  6,
    VCLK0_FB_DIV    =  7, // feedback divider for VCLK0
    VCLK1_FB_DIV    =  8, // feedback divider for VCLK1
    VCLK2_FB_DIV    =  9, // feedback divider for VCLK2
    VCLK3_FB_DIV    = 10, // feedback divider for VCLK3
    PLL_EXT_CNTL    = 11,
};

class ATIRage : public PCIDevice, public VideoCtrlBase {
public:
    ATIRage(uint16_t dev_id);
    ~ATIRage() = default;

    static std::unique_ptr<HWComponent> create_gt() {
        return std::unique_ptr<ATIRage>(new ATIRage(ATI_RAGE_GT_DEV_ID));
    }

    static std::unique_ptr<HWComponent> create_pro() {
        return std::unique_ptr<ATIRage>(new ATIRage(ATI_RAGE_PRO_DEV_ID));
    }

    /* MMIODevice methods */
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size);
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size);

    /* PCI device methods */
    uint32_t pci_cfg_read(uint32_t reg_offs, AccessDetails &details);
    void pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details);

    /* I/O space access methods */
    bool pci_io_read(uint32_t offset, uint32_t size, uint32_t* res);
    bool pci_io_write(uint32_t offset, uint32_t value, uint32_t size);

protected:
    void notify_bar_change(int bar_num);
    const char* get_reg_name(uint32_t reg_offset);
    bool io_access_allowed(uint32_t offset);
    uint32_t read_reg(uint32_t offset, uint32_t size);
    void write_reg(uint32_t offset, uint32_t value, uint32_t size);
    float calc_pll_freq(int scale, int fb_div);
    void verbose_pixel_format(int crtc_index);
    void crtc_enable();
    void draw_hw_cursor(uint8_t *dst_buf, int dst_pitch);

private:
    uint8_t mm_regs[2048] = {0}; // internal registers

    uint8_t plls[64] = {0}; // internal PLL registers

    /* Video RAM variables */
    std::unique_ptr<uint8_t[]>  vram_ptr;
    uint32_t    vram_size;

    uint32_t    aperture_base = 0;
    uint32_t    io_base = 0;
    uint8_t     user_cfg = 8;

    std::unique_ptr<DisplayID>  disp_id;

    int comp_index;          /* color component index for DAC palette access */
};

#endif /* ATI_RAGE_H */
