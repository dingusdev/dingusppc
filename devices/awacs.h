/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-20 divingkatae and maximum
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

/** AWAC sound devices family definitions.

    Audio Waveform Aplifier and Converter (AWAC) is a family of custom audio
    chips used in Power Macintosh.
 */

#ifndef AWAC_H
#define AWAC_H

#include "dbdma.h"
#include "i2c.h"
#include "soundserver.h"
#include <cinttypes>

/** AWAC registers offsets. */
enum {
    AWAC_SOUND_CTRL_REG   = 0x00,
    AWAC_CODEC_CTRL_REG   = 0x10,
    AWAC_CODEC_STATUS_REG = 0x20,
};

/** AWAC manufacturer and revision. */
#define AWAC_MAKER_CRYSTAL  1
#define AWAC_REV_SCREAMER   3

/** Apple source calls this kValidData but doesn't explain
    what it actually means. It seems like it's used to check
    if the sound codec is available.
 */
#define AWAC_AVAILABLE 0x40


/** Audio processor chip (TDA7433) emulation. */
class AudioProcessor : public I2CDevice {
public:
    AudioProcessor()  = default;
    ~AudioProcessor() = default;

    void start_transaction() {
        this->pos = 0;
    };

    bool send_subaddress(uint8_t sub_addr) {
        if ((sub_addr & 0xF) > 6)
            return false; /* invalid subaddress -> no acknowledge */

        this->sub_addr = sub_addr & 0xF;
        this->auto_inc = !!(sub_addr & 0x10);
        LOG_F(INFO, "TDA7433 subaddress = 0x%X, auto increment = %d", this->sub_addr, this->auto_inc);
        this->pos++;
        return true;
    };

    bool send_byte(uint8_t data) {
        if (!this->pos) {
            return send_subaddress(data);
        } else if (this->sub_addr <= 6) {
            LOG_F(INFO, "TDA7433 byte 0x%X received", data);
            this->regs[this->sub_addr] = data;
            if (this->auto_inc) {
                this->sub_addr++;
            }
            return true;
        } else {
            return false; /* invalid sub_addr -> no acknowledge */
        }
    };

    bool receive_byte(uint8_t* p_data) {
        *p_data = this->regs[this->sub_addr];
        LOG_F(INFO, "TDA7433 byte 0x%X sent", *p_data);
        return true;
    };

private:
    uint8_t regs[7]; /* control registers, see TDA7433 datasheet */
    uint8_t sub_addr;
    int pos;
    int auto_inc;
};


class AWACDevice : public DMACallback {
public:
    AWACDevice();
    ~AWACDevice();

    void set_dma_out(DMAChannel* dma_out_ch);

    uint32_t snd_ctrl_read(uint32_t offset, int size);
    void snd_ctrl_write(uint32_t offset, uint32_t value, int size);

    /* DMACallback methods */
    void dma_start();
    void dma_end();

protected:
    void open_stream(int sample_rate);

private:
    static long sound_out_callback(cubeb_stream* stream, void* user_data,
        void const* input_buffer, void* output_buffer,
        long req_frames);

    uint32_t snd_ctrl_reg = {0};
    uint16_t control_regs[8] = {0}; /* control registers, each 12-bits wide */
    uint8_t is_busy          = 0;
    AudioProcessor* audio_proc;

    SoundServer *snd_server;

    bool out_stream_ready;
    int out_sample_rate;

    DMAChannel  *dma_out_ch;
};

/** AWAC PDM-style definitions. */

// PDM HWInit source defines two constants: kExpBit = 0x80 and kCmdBit = 0x40
// I don't know what they means but it seems that their combination will
// cause sound control parameters to be transferred to the sound chip.
#define PDM_SND_CNTL_VALID  0xC0

/** AWAC PDM-style sound codec. */
class AwacDevicePdm {
public:
    AwacDevicePdm();
    ~AwacDevicePdm() = default;

    uint32_t read_stat(void);
    void write_ctrl(uint32_t addr, uint16_t value);

private:
    uint16_t    ctrl_regs[5]; // 12-bit wide control registers
};


#endif /* AWAC_H */
