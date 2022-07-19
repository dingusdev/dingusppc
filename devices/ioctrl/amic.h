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

/** @file Apple memory-mapped I/O controller emulation. */

#ifndef AMIC_H
#define AMIC_H

#include <devices/common/hwinterrupt.h>
#include <devices/common/mmiodevice.h>
#include <devices/common/scsi/sc53c94.h>
#include <devices/common/viacuda.h>
#include <devices/ethernet/mace.h>
#include <devices/floppy/swim3.h>
#include <devices/serial/escc.h>
#include <devices/sound/awacs.h>
#include <devices/video/displayid.h>
#include <devices/video/pdmonboard.h>

#include <cinttypes>
#include <memory>

/** Interrupt related constants. */
#define AMIC_INT_CLR    0x80 // clears CPU interrupt
#define AMIC_INT_MODE   0x40 // interrupt mode: 0 - native, 1 - 68k-style

/** AMIC sound buffers are located at fixed offsets from DMA base. */
#define AMIC_SND_BUF0_OFFS  0x10000
#define AMIC_SND_BUF1_OFFS  0x12000

// PDM HWInit source defines two constants: kExpBit = 0x80 and kCmdBit = 0x40
// I don't know what they mean but it seems that their combination will
// cause sound control parameters to be transferred to the sound chip.
#define PDM_SND_CTRL_VALID  0xC0

#define PDM_DMA_IF1         0x80 // DMA interrupt flag => buffer 1 drained
#define PDM_DMA_IF0         0x40 // DMA interrupt flag => buffer 0 drained
#define PDM_DMA_INTS_MASK   0xF0 // mask for clearing all interrupt flags

/** AMIC-specific sound output DMA implementation. */
class AmicSndOutDma : public DmaOutChannel {
public:
    AmicSndOutDma();
    ~AmicSndOutDma() = default;

    void            init(uint32_t buf_base, uint32_t buf_samples);
    void            enable()  { this->enabled = true;  };
    void            disable() { this->enabled = false; };
    uint8_t         read_stat();
    void            write_dma_out_ctrl(uint8_t value);
    uint32_t        get_cur_buf_pos() { return this->cur_buf_pos; };
    DmaPullResult   pull_data(uint32_t req_len, uint32_t *avail_len,
                              uint8_t **p_data);

private:
    bool        enabled;
    uint8_t     dma_out_ctrl;

    uint32_t    out_buf0;
    uint32_t    out_buf1;
    uint32_t    out_buf_len;
    uint32_t    snd_buf_num;
    uint32_t    cur_buf_pos;
};

/** AMIC-specific floppy DMA implementation. */
class AmicFloppyDma : public DmaBidirChannel {
public:
    AmicFloppyDma() = default;
    ~AmicFloppyDma() = default;

    void            reinit(const uint32_t addr_ptr, const uint16_t byte_cnt);
    void            reset(const uint32_t addr_ptr);
    void            write_ctrl(const uint8_t value);
    uint8_t         read_stat() { return this->stat; };

    int             push_data(const char* src_ptr, int len);
    DmaPullResult   pull_data(uint32_t req_len, uint32_t *avail_len,
                                      uint8_t **p_data);

private:
    uint32_t        addr_ptr;
    uint16_t        byte_count;
    uint8_t         stat;
};

// macro for byte wise updating of AMIC DMA address registers
#define SET_ADDR_BYTE(reg, offset, value)                                       \
    mask = 0xFF000000UL >> (8 * ((offset) & 3));                                \
    (reg) = ((reg) & ~mask) | (((value) & 0xFF) << (8 * (3 - ((offset) & 3))));

// macro for byte wise updating of AMIC DMA size registers
#define SET_SIZE_BYTE(reg, offset, value)                                       \
    mask = 0xFF00U >> (8 * ((offset) & 1));                                     \
    (reg) = ((reg) & ~mask) | (((value) & 0xFF) << (8 * (((offset) & 1) ^ 1)));

/* AMIC registers offsets from AMIC base (0x50F00000). */
enum AMICReg : uint32_t {
    // Sound control registers
    Snd_Ctrl_0          = 0x14000, // audio codec control register 0
    Snd_Ctrl_1          = 0x14001, // audio codec control register 1
    Snd_Ctrl_2          = 0x14002, // audio codec control register 2
    Snd_Stat_0          = 0x14004, // audio codec status register 0
    Snd_Stat_1          = 0x14005, // audio codec status register 1
    Snd_Stat_2          = 0x14006, // audio codec status register 2
    Snd_Buf_Size_Hi     = 0x14008, // sound buffer size, high-order byte
    Snd_Buf_Size_Lo     = 0x14009, // sound buffer size, low-order byte
    Snd_Phase0          = 0x1400C, // high-order byte of the sound phase register
    Snd_Phase1          = 0x1400D, // middle byte of the sound phase register
    Snd_Phase2          = 0x1400E, // low-order byte of the sound phase register
    Snd_Out_Ctrl        = 0x14010, // audio codec output control register
    Snd_In_Ctrl         = 0x14011, // audio codec input control register
    Snd_In_DMA          = 0x14014, // sound input DMA status/control register
    Snd_Out_DMA         = 0x14018, // sound output DMA status/control register

    // Video DAC (Ariel II) control registers
    Ariel_Clut_Index    = 0x24000,
    Ariel_Clut_Color    = 0x24001,
    Ariel_Config        = 0x24002,

    // VIA2 registers
    VIA2_IFR            = 0x26003,
    VIA2_Slot_IER       = 0x26012,
    VIA2_IER            = 0x26013,
    VIA2_IFR_RBV        = 0x27A03, // RBV-compatible mirror for the VIA2_IFR
    VIA2_IER_RBV        = 0x27C13, // RBV-compatible mirror for the VIA2_IER

    // Video control registers
    Video_Mode          = 0x28000,
    Pixel_Depth         = 0x28001,
    Monitor_Id          = 0x28002,

    // Interrupt registers
    Int_Ctrl            = 0x2A000,

    // Undocumented diagnostics register
    Diag_Reg            = 0x2C000,

    // DMA control registers
    DMA_Base_Addr_0     = 0x31000,
    DMA_Base_Addr_1     = 0x31001,
    DMA_Base_Addr_2     = 0x31002,
    DMA_Base_Addr_3     = 0x31003,
    Enet_DMA_Xmt_Ctrl   = 0x31C20,
    SCSI_DMA_Ctrl       = 0x32008,
    Enet_DMA_Rcv_Ctrl   = 0x32028,

    // Floppy (SWIM3) DMA registers
    Floppy_Addr_Ptr_0   = 0x32060,
    Floppy_Addr_Ptr_1   = 0x32061,
    Floppy_Addr_Ptr_2   = 0x32062,
    Floppy_Addr_Ptr_3   = 0x32063,
    Floppy_Byte_Cnt_Hi  = 0x32064,
    Floppy_Byte_Cnt_Lo  = 0x32065,
    Floppy_DMA_Ctrl     = 0x32068,

    SCC_DMA_Xmt_A_Ctrl  = 0x32088,
    SCC_DMA_Rcv_A_Ctrl  = 0x32098,
    SCC_DMA_Xmt_B_Ctrl  = 0x320A8,
    SCC_DMA_Rcv_B_Ctrl  = 0x320B8,
};

/** Apple Memory-mapped I/O controller device. */
class AMIC : public MMIODevice, public InterruptCtrl {
public:
    AMIC();
    ~AMIC() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<AMIC>(new AMIC());
    }

    // HWComponent methods
    int device_postinit();

    /* MMIODevice methods */
    uint32_t read(uint32_t reg_start, uint32_t offset, int size);
    void write(uint32_t reg_start, uint32_t offset, uint32_t value, int size);

    // InterruptCtrl methods
    uint32_t register_dev_int(IntSrc src_id);
    uint32_t register_dma_int(IntSrc src_id);
    void ack_int(uint32_t irq_id, uint8_t irq_line_state);
    void ack_dma_int(uint32_t irq_id, uint8_t irq_line_state);

protected:
    void ack_via2_int(uint32_t irq_id, uint8_t irq_line_state);
    void ack_cpu_int(uint32_t irq_id, uint8_t irq_line_state);

private:
    uint8_t imm_snd_regs[4]; // temporary storage for sound control registers

    uint8_t     emmo_pin; // EMMO aka factory tester pin status, active low

    uint32_t    dma_base     = 0;  // DMA physical base address
    uint16_t    snd_buf_size = 0;  // sound buffer size in bytes
    uint8_t     snd_out_ctrl = 0;

    // floppy DMA state
    uint32_t    floppy_addr_ptr;
    uint16_t    floppy_byte_cnt;

    uint8_t     scsi_dma_cs = 0;   // SCSI DMA control/status register value

    uint8_t     int_ctrl = 0;
    uint8_t     dev_irq_lines = 0; // state of the IRQ lines

    // pseudo VIA2 state
    uint8_t     via2_ier = 0;
    uint8_t     via2_ifr = 0;
    uint8_t     via2_irq = 0;

    uint32_t    pseudo_vbl_tid; // ID for the pseudo-VBL timer

    // AMIC subdevice instances
    Sc53C94*            scsi;
    EsccController*     escc;
    MaceController*     mace;
    ViaCuda*            viacuda;
    Swim3::Swim3Ctrl*   swim3;

    std::unique_ptr<AwacDevicePdm>  awacs;
    std::unique_ptr<AmicSndOutDma>  snd_out_dma;
    std::unique_ptr<AmicFloppyDma>  floppy_dma;

    // on-board video
    std::unique_ptr<DisplayID>          disp_id;
    std::unique_ptr<PdmOnboardVideo>    def_vid;
    uint8_t                             mon_id;
};

#endif // AMIC_H
