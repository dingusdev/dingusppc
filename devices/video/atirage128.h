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

#ifndef ATI_RAGE_H
#define ATI_RAGE_H

#include <devices/common/pci/pcidevice.h>
#include <devices/video/atirage128defs.h>
#include <devices/video/displayid.h>
#include <devices/video/videoctrl.h>

#include <cinttypes>
#include <memory>

/* Rage 128 PLL register indices. */
enum {
    CLK_PIN_CNTL      = 1,
    PPLL_CNTL         = 2,
    PPLL_REF_DIV      = 3,
    PPLL_DIV_0        = 4,
    PPLL_DIV_1        = 5,
    PPLL_DIV_2        = 6,
    PPLL_DIV_3        = 7,
    VCLK_ECP_CNTL     = 8,
    HTOTAL_CNTL       = 9,
    X_MPLL_REF_FB_DIV = 10,
    XPLL_CNTL         = 11,
    XDLL_CNTL         = 12,
    XCLK_CNTL         = 13,
    MPLL_CNTL         = 14,
    MCLK_CNTL         = 15,
    FCP_CNTL          = 18,
    PPLL_TEST_CNTL    = 42,
    P2PLL_DIV_0       = 43,
    POWER_MGMT        = 47
};

class ATIRage128 : public PCIDevice, public VideoCtrlBase {
public:
    ATIRage128(uint16_t dev_id);
    ~ATIRage128() = default;

    static std::unique_ptr<HWComponent> create_gl() {
        return std::unique_ptr<ATIRage128>(new ATIRage128(ATI_RAGE_128_GL_DEV_ID));
    }

    static std::unique_ptr<HWComponent> create_vr() {
        return std::unique_ptr<ATIRage128>(new ATIRage128(ATI_RAGE_128_VR_DEV_ID));
    }

    static std::unique_ptr<HWComponent> create_pro_128() {
        return std::unique_ptr<ATIRage128>(new ATIRage128(ATI_RAGE_128_PRO_DEV_ID));
    }

    // MMIODevice methods
    //uint32_t read(uint32_t rgn_start, uint32_t offset, int size);
    //void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size);

    // PCI device methods
    uint32_t pci_cfg_read(uint32_t reg_offs, AccessDetails& details);
    //void pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails& details);

    // I/O space access methods
   // bool pci_io_read(uint32_t offset, uint32_t size, uint32_t* res);
    //bool pci_io_write(uint32_t offset, uint32_t value, uint32_t size);

protected:
    //void notify_bar_change(int bar_num);
    const char* get_reg_name(uint32_t reg_offset);
    //bool io_access_allowed(uint32_t offset);
    uint32_t read_reg(uint32_t reg_offset, uint32_t size);
    //void write_reg(uint32_t reg_addr, uint32_t value, uint32_t size);
    //float calc_pll_freq(int scale, int fb_div);
    //void verbose_pixel_format(int crtc_index);
    //void crtc_update();
    //void crtc_enable();
    //void draw_hw_cursor(uint8_t* dst_buf, int dst_pitch);
    //void get_cursor_position(int& x, int& y);

private:
    void change_one_bar(uint32_t& aperture, uint32_t aperture_size, uint32_t aperture_new, int bar_num);

    uint32_t regs[2048] = {};    // internal registers
    uint8_t cfg[128]    = {};    // config registers
    uint8_t crt[100]    = {};    // internal CRT registers
    uint8_t plls[32]    = {};    // internal PLL registers

    // Video RAM variables
    std::unique_ptr<uint8_t[]> vram_ptr;
    uint32_t vram_size;

    uint32_t aperture_count   = 3;
    uint32_t aperture_size[3] = {0x1000000, 0x100, 0x1000};
    uint32_t aperture_flag[3] = {0, 1, 0};

    uint32_t aperture_base  = 0;
    uint32_t io_base        = 0;
    uint8_t user_cfg        = 8;

    std::unique_ptr<DisplayID> disp_id;

    // DAC interface state
    uint8_t dac_wr_index = 0;     // current DAC color index for writing
    uint8_t dac_rd_index = 0;     // current DAC color index for reading
    uint8_t dac_mask     = 0;     // current DAC mask
    int comp_index       = 0;     // current color component index
    uint8_t color_buf[3] = {};    // buffer for storing DAC color components
};

#endif    // ATI_RAGE_H