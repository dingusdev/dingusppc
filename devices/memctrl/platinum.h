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

/** Platinum Memory Controller definitions.

    Author: Max Poliakovski

    Platinum is a single-chip memory and video subsystem controller designed
    especially for the Power Macintosh 7200 computer, code name Catalyst.
*/

#ifndef PLATINUM_MEMCTRL_H
#define PLATINUM_MEMCTRL_H

#include <devices/common/hwcomponent.h>
#include <devices/common/mmiodevice.h>
#include <devices/memctrl/memctrlbase.h>
#include <devices/video/appleramdac.h>
#include <devices/video/displayid.h>
#include <devices/video/videoctrl.h>

#include <cinttypes>
#include <memory>

namespace Platinum {

/** Clock source encoding in the CPUID register */
enum {
    ClkSrc0 = 0x0000, // 33 MHz
    ClkSrc2 = 0x8000, // 20 MHz
    ClkSrc3 = 0x8040  // 31,3344 MHz
};

/** CPU/Bus speed encoding for ClkSrc0 */
enum CpuSpeed0 {
     CPU_66_BUS_33   =  0,
     CPU_80_BUS_40_1 =  1,
     CPU_80_BUS_27_1 =  2,
    CPU_100_BUS_50   =  3,
    CPU_100_BUS_33   =  4,
    CPU_120_BUS_60   =  5,
    CPU_120_BUS_40   =  6,
    CPU_120_BUS_30   =  7,
    CPU_133_BUS_66   =  8,
    CPU_133_BUS_44   =  9,
    CPU_133_BUS_33   = 10,
    CPU_150_BUS_75   = 11,
    CPU_150_BUS_50   = 12,
    CPU_150_BUS_37   = 13
};

/** CPU/Bus speed encoding for ClkSrc2 */
enum CpuSpeed2 {
    CPU_40_BUS_20   =  0,
    CPU_48_BUS_24   =  1,
    CPU_48_BUS_16   =  2,
    CPU_60_BUS_30   =  3,
    CPU_60_BUS_20   =  4,
    CPU_72_BUS_36   =  5,
    CPU_72_BUS_24   =  6,
    CPU_72_BUS_18   =  7,
    CPU_80_BUS_40_2 =  8,
    CPU_80_BUS_27_2 =  9,
    CPU_80_BUS_20   = 10,
    CPU_90_BUS_45   = 11,
    CPU_90_BUS_30   = 12,
    CPU_90_BUS_22   = 13
};

/** CPU/Bus speed encoding for ClkSrc3 */
enum CpuSpeed3 {
     CPU_62_BUS_31 =  0,
     CPU_75_BUS_38 =  1, // actual bus frequency: 37500000 Hz
     CPU_75_BUS_25 =  2,
     CPU_94_BUS_47 =  3,
     CPU_94_BUS_31 =  4,
    CPU_113_BUS_56 =  5,
    CPU_113_BUS_38 =  6, // actual bus frequency: 37500000 Hz
    CPU_113_BUS_28 =  7,
    CPU_125_BUS_64 =  8, // actual bus frequency: 63500000 Hz
    CPU_125_BUS_42 =  9,
    CPU_125_BUS_31 = 10,
    CPU_141_BUS_72 = 11, // actual bus frequency: 71500000 Hz
    CPU_141_BUS_47 = 12,
    CPU_141_BUS_35 = 13
};

/** Configuration and status register offsets. */
enum PlatinumReg : uint32_t {
    CPU_ID          = 0x00, // read 0x30018140 ; read byte happens
    ASIC_REVISION   = 0x01,
    ROM_TIMING      = 0x02,
    CACHE_CONFIG    = 0x03,
    DRAM_TIMING     = 0x04,
    DRAM_REFRESH    = 0x05,
    BANK_0_BASE     = 0x06,
    BANK_1_BASE     = 0x07,
    BANK_2_BASE     = 0x08,
    BANK_3_BASE     = 0x09, // read byte happens
    BANK_4_BASE     = 0x0A,
    BANK_5_BASE     = 0x0B,
    BANK_6_BASE     = 0x0C,
    BANK_7_BASE     = 0x0D,
    GP_SW_SCRATCH   = 0x0E,
    PCI_ADDR_MASK   = 0x0F,
    FB_BASE_ADDR    = 0x10,
    //              = 0x11,
    ROW_WORDS       = 0x12,
    CLOCK_DIVISOR   = 0x13, // write 0xff, 0x02
    FB_CONFIG_1     = 0x14,
    FB_CONFIG_2     = 0x15,
    VMEM_PAGE_MODE  = 0x16,
    MON_ID_SENSE    = 0x17,
    FB_RESET        = 0x18, // write 6, 3, 7, 2
    DBL_BUF_CNTL    = 0x19,
    V_TEST_REG      = 0x1A,
    VRAM_REFRESH    = 0x1B,

    // Swatch timing generator registers
    SWATCH_CONFIG   = 0x20, // 0xff0 ; pxffc for 1280 modes
    SWATCH_INT_MASK = 0x21, // 4
    SWATCH_INT_STAT = 0x22, // 0
    CLR_CURSOR_INT  = 0x23, // 0
    CLR_ANIM_INT    = 0x24, // 0
    CLR_VBL_INT     = 0x25, // 0
    CURSOR_LINE     = 0x26, // write 0x320, 0x209, 0x299, 0x38f, 0x428
    ANIMATE_LINE    = 0x27, // 0
    COUNTER_TEST    = 0x28, // 0
    FIRST_SWATCH    = 0x29,
    SWATCH_HSERR    = 0x29,
    SWATCH_HLFLN    = 0x2A,
    SWATCH_HEQ      = 0x2B,
    SWATCH_HSP      = 0x2C,
    SWATCH_HBWAY    = 0x2D,
    SWATCH_HBRST    = 0x2E,
    SWATCH_HBP      = 0x2F,
    SWATCH_HAL      = 0x30,
    SWATCH_HFP      = 0x31,
    SWATCH_HPIX     = 0x32,
    SWATCH_VHLINE   = 0x33,
    SWATCH_VSYNC    = 0x34,
    SWATCH_VBPEQ    = 0x35,
    SWATCH_VBP      = 0x36,
    SWATCH_VAL      = 0x37,
    SWATCH_VFP      = 0x38,
    SWATCH_VFPEQ    = 0x39,
    TIMING_ADJUST   = 0x3A,
    CURRENT_LINE    = 0x3B, // Current Scan Line

    // Iridium datapath registers
    IRIDIUM_CONFIG  = 0x4A, // write 4
    POWER_DOWN_CTRL = 0x4B, // 1-bit register, writing "1" enables power down mode
};

#define REG_TO_INDEX(reg) ((reg) - FIRST_SWATCH)

// FB_CONFIG_1 register bits.
enum {
    CFG1_INTERLACE  = (1 <<  2), // 1 - interlaced video enabled
    CFG1_VID_ENABLE = (1 <<  4), // 1 - display refresh enabled
    CFG1_FULL_BANKS = (1 << 12), // full VRAM banks (64-bit) access enable
};

// FB_RESET register bits.
enum {
    VRAM_SM_RESET     = (1 << 0), // VRAM state machine reset
    VREFRESH_SM_RESET = (1 << 1), // Video refresh state machine reset
    SWATCH_RESET      = (1 << 2), // Swatch reset
};

// SWATCH_INT_MASK register bits.
enum {
    SWATCH_INT_VBL    = (1 << 0),
    SWATCH_INT_ANIM   = (1 << 1),
    SWATCH_INT_CURSOR = (1 << 2)
};

#define DAMFB_VERSION_PLATINUM  6 // DAMFB cell version in the Platinum ASIC
#define IRIDIUM_VENDOR_VLSI     0 // Vendor ID for the Iridium ASIC => VLSI

constexpr auto VRAM_REGION_BASE     = 0xF1000000UL;
constexpr auto PLATINUM_IOREG_BASE  = 0xF8000000UL;

}; // namespace Platinum

class PlatinumCtrl : public MemCtrlBase, public VideoCtrlBase, public MMIODevice {
public:
    PlatinumCtrl();
    ~PlatinumCtrl() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<PlatinumCtrl>(new PlatinumCtrl());
    }

    // HWComponent methods
    int device_postinit();

    // MMIODevice methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size);
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size);

    void insert_ram_dimm(int slot_num, uint32_t capacity);
    void map_phys_ram();

protected:
    void enable_display();
    void enable_cursor_int();
    void update_irq(uint8_t irq_line_state, uint8_t irq_mask);

private:
    uint32_t    cpu_id;
    uint8_t     cpu_type; // 0 - MPC601, 1 - 603/604 CPU

    // memory controller state
    uint32_t    rom_timing   = 0;
    uint32_t    dram_timing  = 0xEFF;
    uint32_t    dram_refresh = 0x1F4;
    uint32_t    bank_base[8] = {};
    uint32_t    bank_size[8] = {};

    // frame buffer controller state
    uint32_t    fb_addr       = Platinum::VRAM_REGION_BASE;
    uint32_t    fb_offset     = 0;
    uint32_t    fb_config_1   = 0x1F00;
    uint32_t    fb_config_2   = 0x1FFF;
    uint32_t    clock_divisor = 0;
    uint32_t    row_words     = 0;
    uint32_t    fb_reset      = 7;
    int         reset_step    = 0;
    uint32_t    fb_test       = DAMFB_VERSION_PLATINUM << 9;
    uint32_t    vram_refresh  = 0x1F4;
    uint32_t    vram_size     = 0;
    uint32_t    iridium_cfg   = (IRIDIUM_VENDOR_VLSI << 24) | 1; // big-endian bus
    uint8_t     vram_megs     = 0;
    uint8_t     half_bank     = 0;
    uint8_t     half_access   = 0;
    uint8_t     vmem_fp_mode  = 0;
    uint8_t     mon_sense     = 0;

    // video timing generator (Swatch) state
    uint32_t    swatch_config       = 0xFFD;
    uint32_t    swatch_params[17]   = {};
    uint32_t    timing_adjust       = 0;
    uint32_t    power_down_ctrl     = 0;

    // interrupt related state
    uint32_t    swatch_int_mask     = 0;
    uint32_t    swatch_int_stat     = 0;
    uint32_t    cursor_line         = 0;
    uint32_t    cursor_task_id      = 0;

    std::unique_ptr<uint8_t[]>      vram_ptr = nullptr;
    std::unique_ptr<DisplayID>      display_id = nullptr;
    std::unique_ptr<AppleRamdac>    dacula = nullptr;
};

#endif // PLATINUM_MEMCTRL_H
