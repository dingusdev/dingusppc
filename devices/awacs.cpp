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

#include <algorithm>
#include <thirdparty/loguru/loguru.hpp>
#include "endianswap.h"
#include "awacs.h"
#include "dbdma.h"
#include "machines/machinebase.h"
#include "soundserver.h"
//#include <thirdparty/libsoundio/soundio/soundio.h>

static int awac_freqs[8] = {44100, 29400, 22050, 17640, 14700, 11025, 8820, 7350};


AWACDevice::AWACDevice()
{
    this->audio_proc = new AudioProcessor();

    /* register audio processor chip with the I2C bus */
    I2CBus *i2c_bus = dynamic_cast<I2CBus *>(gMachineObj->get_comp_by_type(HWCompType::I2C_HOST));
    i2c_bus->register_device(0x45, this->audio_proc);

    this->snd_server = dynamic_cast<SoundServer *>
        (gMachineObj->get_comp_by_name("SoundServer"));
    //this->out_device = snd_server->get_out_device();
    this->out_stream_ready = false;
}

AWACDevice::~AWACDevice()
{
    if (this->out_stream_ready) {
        snd_server->close_out_stream();
    }

    delete this->audio_proc;
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

#if 0
static void convert_data(const uint8_t *in, SoundIoChannelArea *out_buf, uint32_t frame_count)
{
    uint16_t *p_in = (uint16_t *)in;

    for (int i = 0; i < frame_count; i += 2, p_in += 4) {
        *(uint16_t *)(out_buf[0].ptr) = BYTESWAP_16(p_in[0]);
        out_buf[0].ptr += out_buf[0].step;
        *(uint16_t *)(out_buf[0].ptr) = BYTESWAP_16(p_in[1]);
        out_buf[0].ptr += out_buf[0].step;

        *(uint16_t *)(out_buf[1].ptr) = BYTESWAP_16(p_in[2]);
        out_buf[1].ptr += out_buf[1].step;
        *(uint16_t *)(out_buf[1].ptr) = BYTESWAP_16(p_in[3]);
        out_buf[1].ptr += out_buf[1].step;
    }
}

static void insert_silence(SoundIoChannelArea *out_buf, uint32_t frame_count)
{
    for (int i = 0; i < frame_count; i += 2) {
        *(uint16_t *)(out_buf[0].ptr) = 0;
        out_buf[0].ptr += out_buf[0].step;
        *(uint16_t *)(out_buf[0].ptr) = 0;
        out_buf[0].ptr += out_buf[0].step;

        *(uint16_t *)(out_buf[1].ptr) = 0;
        out_buf[1].ptr += out_buf[1].step;
        *(uint16_t *)(out_buf[1].ptr) = 0;
        out_buf[1].ptr += out_buf[1].step;
    }
}
#endif

#if 0
static void sound_out_callback(struct SoundIoOutStream *outstream,
        int frame_count_min, int frame_count_max)
{
    int err, frame_count;
    uint8_t *p_in;
    uint32_t buf_len, rem_len, got_len;
    struct SoundIoChannelArea *areas;
    DMAChannel *dma_ch = (DMAChannel *)outstream->userdata; /* C API baby! */
    int n_channels = outstream->layout.channel_count;
    bool stop = false;

    //if (!dma_ch->is_active()) {
    //    soundio_outstream_clear_buffer(outstream);
    //    soundio_outstream_pause(outstream, true);
    //    return;
    //}

    if (frame_count_max > 512) {
        frame_count = 512;
    }
    else {
        frame_count = frame_count_max;
    }

    buf_len = (frame_count * n_channels) << 1;
    //frame_count = frame_count_max;

    //LOG_F(INFO, "frame_count_min=%d", frame_count_min);
    //LOG_F(INFO, "frame_count_max=%d", frame_count_max);
    //LOG_F(INFO, "channel count: %d", n_channels);
    //LOG_F(INFO, "buf_len: %d", buf_len);

    if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count))) {
        LOG_F(ERROR, "unrecoverable stream error: %s\n", soundio_strerror(err));
        return;
    }

    for (rem_len = buf_len; rem_len > 0; rem_len -= got_len) {
        if (!dma_ch->get_data(rem_len, &got_len, &p_in)) {
            //LOG_F(INFO, "SndCallback: got_len = %d", got_len);
            convert_data(p_in, areas, got_len >> 2);
            //LOG_F(9, "Converted sound data, len = %d", got_len);
        } else { /* no more data */
            //memset(buf, 0, rem_len); /* fill the buffer with silence */
            //LOG_F(9, "Inserted silence, len = %d", rem_len);

            /* fill the buffer with silence */
            //LOG_F(ERROR, "rem_len=%d", rem_len);
            insert_silence(areas, rem_len >> 2);
            stop = true;
            break;
        }
    }

    if ((err = soundio_outstream_end_write(outstream))) {
        LOG_F(ERROR, "unrecoverable stream error: %s\n", soundio_strerror(err));
        return;
    }

    if (stop) {
        LOG_F(INFO, "pausing result: %s",
            soundio_strerror(soundio_outstream_pause(outstream, true)));
    }
}
#endif

long AWACDevice::sound_out_callback(cubeb_stream *stream, void *user_data,
                        void const *input_buffer, void *output_buffer,
                        long req_frames)
{
    uint8_t *p_in, *buf;
    int16_t* in_buf, * out_buf;
    uint32_t buf_len, rem_len, data_len, got_len;
    long frames, out_frames;
    AWACDevice *this_ptr = static_cast<AWACDevice*>(user_data); /* C API baby! */

    if (!this_ptr->dma_out_ch->is_active()) {
        return 0;
    }

    out_buf = (int16_t*)output_buffer;

    out_frames = 0;

    /* handle remainder chunk from previous call */
    if (req_frames > 0 && this_ptr->remainder) {
        out_buf[0] = this_ptr->rem_data[0];
        out_buf[1] = this_ptr->rem_data[1];
        out_buf += 2;
        req_frames--;
        out_frames++;
        this_ptr->remainder = false;
    }

    if (req_frames <= 0) {
        return out_frames;
    }

    while (req_frames > 0) {
        data_len = ((req_frames << 2) + 8) & ~7;
        if (!this_ptr->dma_out_ch->get_data(data_len, &got_len, &p_in)) {
            frames = std::min((long)(got_len >> 2), req_frames);

            in_buf = (int16_t*)p_in;

            for (int i = frames & ~1; i > 0; i -= 2) {
                out_buf[0] = BYTESWAP_16(in_buf[0]);
                out_buf[1] = BYTESWAP_16(in_buf[1]);
                out_buf[2] = BYTESWAP_16(in_buf[2]);
                out_buf[3] = BYTESWAP_16(in_buf[3]);
                in_buf += 4;
                out_buf += 4;
            }
            if (frames & 1) {
                out_buf[0] = BYTESWAP_16(in_buf[0]);
                out_buf[1] = BYTESWAP_16(in_buf[1]);
                this_ptr->rem_data[0] = BYTESWAP_16(in_buf[2]);
                this_ptr->rem_data[1] = BYTESWAP_16(in_buf[3]);
                this_ptr->remainder = true;
            }

            req_frames -= frames;
            out_frames += frames;
        }
        else {
            out_frames += got_len >> 2;
            break;
        }
    }

    return out_frames;
}

void status_callback(cubeb_stream *stream, void *user_data, cubeb_state state)
{
    LOG_F(9, "Cubeb status callback fired, status = %d", state);
}

void AWACDevice::open_stream(int sample_rate)
{
    int err;

#if 0
    this->out_stream = soundio_outstream_create(this->out_device);
    this->out_stream->write_callback = sound_out_callback;
    this->out_stream->format = SoundIoFormatS16LE;
    this->out_stream->sample_rate = sample_rate;
    this->out_stream->userdata = (void *)this->dma_out_ch;

    if ((err = soundio_outstream_open(this->out_stream))) {
        LOG_F(ERROR, "AWAC: unable to open sound output stream: %s",
            soundio_strerror(err));
    } else {
        this->out_sample_rate = sample_rate;
        this->out_stream_ready = true;
    }
#endif

    if ((err = this->snd_server->open_out_stream(sample_rate, sound_out_callback,
            status_callback, (void *)this))) {
        LOG_F(ERROR, "AWAC: unable to open sound output stream: %d", err);
        this->out_stream_ready = false;
    } else {
        this->out_sample_rate = sample_rate;
        this->out_stream_ready = true;
    }
}

void AWACDevice::dma_start()
{
    int err;

    if (!this->out_stream_ready) {
        this->open_stream(awac_freqs[(this->snd_ctrl_reg >> 8) & 7]);
    } else if (this->out_sample_rate != awac_freqs[(this->snd_ctrl_reg >> 8) & 7]) {
        //soundio_outstream_destroy(this->out_stream);
        snd_server->close_out_stream();
        this->open_stream(awac_freqs[(this->snd_ctrl_reg >> 8) & 7]);
    } else {
        LOG_F(ERROR, "AWAC: unpausing attempted!");
        //soundio_outstream_clear_buffer(this->out_stream);
        //LOG_F(INFO, "AWAC: unpausing result: %s",
        //    soundio_strerror(soundio_outstream_pause(this->out_stream, false)));
        return;
    }

    if (!this->out_stream_ready) {
        return;
    }

    this->remainder = false;

    //if ((err = soundio_outstream_start(this->out_stream))) {
    //    LOG_F(ERROR, "AWAC: unable to start stream: %s\n", soundio_strerror(err));
    //    return;
    //}

    if ((err = snd_server->start_out_stream())) {
        LOG_F(ERROR, "Could not start sound output stream");
    }
}

void AWACDevice::dma_end()
{
}
