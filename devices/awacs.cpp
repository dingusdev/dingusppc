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

/** AWAC sound device emulation.

    Author: Max Poliakovski 2019-20
*/

#include <thirdparty/loguru/loguru.hpp>
#include "endianswap.h"
#include "awacs.h"
#include "dbdma.h"
#include "machines/machinebase.h"
#include <thirdparty/SDL2/include/SDL.h>

static int awac_freqs[8] = {44100, 29400, 22050, 17640, 14700, 11025, 8820, 7350};

AWACDevice::AWACDevice()
{
    this->audio_proc = new AudioProcessor();

    /* register audio processor chip with the I2C bus */
    I2CBus *i2c_bus = dynamic_cast<I2CBus *>(gMachineObj->get_comp_by_type(HWCompType::I2C_HOST));
    i2c_bus->register_device(0x45, this->audio_proc);
}

AWACDevice::~AWACDevice()
{
    delete this->audio_proc;

    if (this->snd_buf)
        delete[] this->snd_buf;

    if (this->snd_out_dev)
        SDL_CloseAudioDevice(snd_out_dev);
}

void AWACDevice::set_dma_out(DMAChannel *dma_out_ch)
{
    this->dma_out_ch = dma_out_ch;
}

uint32_t AWACDevice::snd_ctrl_read(uint32_t offset, int size)
{
    switch(offset) {
    case AWAC_SOUND_CTRL_REG:
        return this->snd_ctrl_reg;
    case AWAC_CODEC_CTRL_REG:
        return this->is_busy;
    case AWAC_CODEC_STATUS_REG:
        return (AWAC_AVAILABLE << 8) | (AWAC_MAKER_CRYSTAL << 16) |
               (AWAC_REV_SCREAMER << 20);
        break;
    default:
        LOG_F(ERROR, "AWAC: unsupported register at offset 0x%X", offset);
    }

    return 0;
}

void AWACDevice::snd_ctrl_write(uint32_t offset, uint32_t value, int size)
{
    int subframe, reg_num;
    uint16_t data;

    switch(offset) {
    case AWAC_SOUND_CTRL_REG:
        this->snd_ctrl_reg = BYTESWAP_32(value);
        LOG_F(INFO, "New sound control value = 0x%X", this->snd_ctrl_reg);
        break;
    case AWAC_CODEC_CTRL_REG:
        subframe = (value >> 14) & 3;
        reg_num  = (value >> 20) & 7;
        data = ((value >> 8) & 0xF00) | ((value >> 24) & 0xFF);
        LOG_F(INFO, "AWAC subframe = %d, reg = %d, data = %08X\n", subframe, reg_num, data);
        if (!subframe)
            this->control_regs[reg_num] = data;
        break;
    default:
        LOG_F(ERROR, "AWAC: unsupported register at offset 0x%X", offset);
    }
}

static void convert_data(const uint8_t *in, uint8_t *out, uint32_t len)
{
    uint16_t *p_in, *p_out;

    if (len & 7) {
        LOG_F(WARNING, "AWAC sound buffer len not a multiply of 8, %d", len);
    }

    p_in  = (uint16_t *)in;
    p_out = (uint16_t *)out;
    len >>= 1;

    LOG_F(INFO, "Converting %d samples", len);

    /* AWAC data comes as LLRR -> convert it to LRLR */
    for (int i = 0; i < len; i += 8) {
        p_out[i]   = p_in[i];
        p_out[i+1] = p_in[i+2];
        p_out[i+2] = p_in[i+1];
        p_out[i+3] = p_in[i+3];
    }
}

static void audio_out_callback(void *user_data, uint8_t *buf, int buf_len)
{
    uint8_t *p_in;
    uint32_t rem_len, got_len;

    DMAChannel *dma_ch = (DMAChannel *)user_data; /* C API baby! */

    for (rem_len = buf_len; rem_len > 0; rem_len -= got_len, buf += got_len) {
        if (!dma_ch->get_data(rem_len, &got_len, &p_in)) {
            convert_data(p_in, buf, got_len);
            LOG_F(9, "Converted sound data, len = %d", got_len);
        } else { /* no more data */
            memset(buf, 0, rem_len); /* fill the buffer with silence */
            LOG_F(9, "Inserted silence, len = %d", rem_len);
            break;
        }
    }
}

uint32_t AWACDevice::convert_data(const uint8_t *data, int len)
{
    int i;
    uint16_t *p_in, *p_out;

    if (len > this->buf_len) {
        if (this->snd_buf)
            delete this->snd_buf;
        this->snd_buf = new uint8_t[len];
        this->buf_len = len;
    }

    p_in  = (uint16_t *)data;
    p_out = (uint16_t *)this->snd_buf;

    for (i = 0; i < len; i += 8) {
        p_out[i]   = p_in[i];
        p_out[i+1] = p_in[i+2];
        p_out[i+2] = p_in[i+1];
        p_out[i+3] = p_in[i+3];
    }

    return i;
}

void AWACDevice::dma_start()
{
    SDL_AudioSpec snd_spec, snd_settings;

    SDL_zero(snd_spec);
    snd_spec.freq = awac_freqs[(this->snd_ctrl_reg >> 8) & 7];
    snd_spec.format = AUDIO_S16MSB; /* yes, AWAC accepts big-endian data */
    snd_spec.channels = 2;
    snd_spec.samples = 4096; /* buffer size, chosen empirically */
    snd_spec.callback = audio_out_callback;
    snd_spec.userdata = (void *)this->dma_out_ch;


    this->snd_out_dev = SDL_OpenAudioDevice(NULL, 0, &snd_spec, &snd_settings, 0);
    if (!this->snd_out_dev) {
        LOG_F(ERROR, "Could not open sound output device, error %s", SDL_GetError());
    } else {
        LOG_F(INFO, "Created audio output channel, sample rate = %d", snd_spec.freq);
        this->wake_up = true;
    }

    SDL_PauseAudioDevice(this->snd_out_dev, 0); /* start audio playing */
}

void AWACDevice::dma_end()
{
    if (this->snd_out_dev) {
        SDL_CloseAudioDevice(this->snd_out_dev);
        this->snd_out_dev = 0;
    }
}

void AWACDevice::dma_push(uint8_t *buf, int size)
{
    uint32_t dst_len;

    dst_len = this->convert_data(buf, size);
    if (dst_len) {
        if (SDL_QueueAudio(this->snd_out_dev, this->snd_buf, dst_len)) {
            LOG_F(ERROR, "SDL_QueueAudio error: %s", SDL_GetError());
        }
    }

    if (this->wake_up) {
        SDL_PauseAudioDevice(this->snd_out_dev, 0); /* start audio playing */
        this->wake_up = false;
    }
}

void AWACDevice::dma_pull(uint8_t *buf, int size)
{
}
