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

/** AWAC sound devices family definitions.

    Audio Waveform Amplifier and Converters (AWACs) is a family of custom audio
    chips used in Power Macintosh.
 */

#ifndef AWAC_H
#define AWAC_H

#include <devices/common/hwcomponent.h>
#include <devices/common/i2c/i2c.h>

#include <cinttypes>
#include <memory>
#include <string>

class DmaOutChannel;
class DmaInChannel;
class SoundServer;

/** Base class for the AWACs codecs. */
class AwacsBase : public HWComponent {
public:
    AwacsBase(std::string name);
    ~AwacsBase() = default;

    void set_dma_out(DmaOutChannel *dma_out_ch) {
        this->dma_out_ch = dma_out_ch;
    };

    void set_dma_in(DmaInChannel *dma_in_ch) {
        this->dma_in_ch = dma_in_ch;
    };

    void set_sample_rate(int sr_id);
    void dma_out_start();
    void dma_out_stop();
    void dma_out_pause();
    void dma_in_start();
    void dma_in_stop();
    void dma_in_pause();
    void dma_in_data();

protected:
    SoundServer     *snd_server; // SoundServer instance pointer
    DmaOutChannel   *dma_out_ch; // DMA output channel instance pointer
    DmaInChannel    *dma_in_ch; // DMA input channel instance pointer
    uint32_t        dma_in_timer_id = 0;

    int     *sr_table;  // pointer to the table of supported sample rates
    int     max_sr_id;  // maximum value for sample rate ID

    bool    out_stream_ready    = false;
    bool    out_stream_running  = false;
    int     cur_sample_rate     = -1;
    int     out_sample_rate     = -1;
};

/** AWACs PDM-style sound codec. */
class AwacDevicePdm : public AwacsBase {
public:
    AwacDevicePdm();
    ~AwacDevicePdm() = default;

    uint32_t read_stat(void);
    void write_ctrl(uint32_t addr, uint16_t value);

private:
    uint16_t    ctrl_regs[5] = {}; // 12-bit wide control registers
};

/** Offsets to MacIO sound codec registers. */
enum {
    AWAC_SOUND_CTRL_REG   = 0x00,
    AWAC_CODEC_CTRL_REG   = 0x10,
    AWAC_CODEC_STATUS_REG = 0x20,
};

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
            return false; // invalid subaddress -> no acknowledge

        this->sub_addr = sub_addr & 0xF;
        this->auto_inc = !!(sub_addr & 0x10);
        LOG_F(9, "TDA7433 subaddress = 0x%X, auto increment = %d",
              this->sub_addr, this->auto_inc);
        this->pos++;
        return true;
    };

    bool send_byte(uint8_t data) {
        if (!this->pos) {
            return send_subaddress(data);
        } else if (this->sub_addr <= 6) {
            LOG_F(9, "TDA7433 byte 0x%X received", data);
            this->regs[this->sub_addr] = data;
            if (this->auto_inc) {
                this->sub_addr++;
            }
            return true;
        } else {
            return false; // invalid sub_addr -> no acknowledge
        }
    };

    bool receive_byte(uint8_t* p_data) {
        *p_data = this->regs[this->sub_addr];
        LOG_F(9, "TDA7433 byte 0x%X sent", *p_data);
        return true;
    };

private:
    uint8_t regs[7] = {}; // control registers, see TDA7433 datasheet
    uint8_t sub_addr;
    int pos;
    int auto_inc;
};

/** Sound codec interface with the typical MacIO access. */
class MacioSndCodec : public AwacsBase {
public:
    MacioSndCodec(std::string name) : AwacsBase(name) {};
    virtual uint32_t snd_ctrl_read(uint32_t offset, int size) = 0;
    virtual void     snd_ctrl_write(uint32_t offset, uint32_t value, int size) = 0;
};

/** AWACs manufacturers and revisions. */
#define AWAC_MAKER_CRYSTAL  1
#define AWAC_REV_AWACS      2
#define AWAC_REV_SCREAMER   3

/** Screamer sound codec. */
class AwacsScreamer : public MacioSndCodec {
public:
    AwacsScreamer(std::string name = "Screamer");
    ~AwacsScreamer() = default;

    uint32_t    snd_ctrl_read(uint32_t offset, int size);
    void        snd_ctrl_write(uint32_t offset, uint32_t value, int size);

    int device_postinit();

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<AwacsScreamer>(new AwacsScreamer("Screamer"));
    }

private:
    uint32_t snd_ctrl_reg    = 0;
    uint16_t control_regs[8] = {}; // control registers, each 12-bits wide
    uint8_t  is_busy         = 0;

    std::unique_ptr<AudioProcessor> audio_proc;
};

#endif /* AWAC_H */
