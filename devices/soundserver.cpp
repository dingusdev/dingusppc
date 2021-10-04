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

#include "dmacore.h"
#include "endianswap.h"
#include "soundserver.h"
#include <loguru.hpp>
#include <cubeb/cubeb.h>
#ifdef _WIN32
#include <objbase.h>
#endif

#if 0
int SoundServer::start()
{
    int err;

    this->status = SND_SERVER_DOWN;

    this->soundio = soundio_create();
    if (!this->soundio) {
        LOG_F(ERROR, "Sound Server: out of memory");
        return -1;
    }

    if ((err = soundio_connect(this->soundio))) {
        LOG_F(ERROR, "Unable to connect to backend: %s", soundio_strerror(err));
        return -1;
    }

    LOG_F(INFO, "Connected to backend: %s", soundio_backend_name(soundio->current_backend));

    soundio_flush_events(this->soundio);

    this->status = SND_API_READY;

    this->out_dev_index = soundio_default_output_device_index(this->soundio);
    if (this->out_dev_index < 0) {
        LOG_F(ERROR, "Sound Server: no output device found");
        return -1;
    }

    this->out_device = soundio_get_output_device(this->soundio, this->out_dev_index);
    if (!this->out_device) {
        LOG_F(ERROR, "Sound Server: out of memory");
        return -1;
    }

    LOG_F(INFO, "Sound Server output device: %s", this->out_device->name);

    this->status = SND_SERVER_UP;

    return 0;
}

void SoundServer::shutdown()
{
    switch (this->status) {
    case SND_SERVER_UP:
        soundio_device_unref(this->out_device);
        /* fall through */
    case SND_API_READY:
        soundio_destroy(this->soundio);
    }

    this->status = SND_SERVER_DOWN;

    LOG_F(INFO, "Sound Server shut down.");
}
#endif

int SoundServer::start()
{
    int res;

#ifdef _WIN32
    CoInitialize(nullptr);
#endif

    this->status = SND_SERVER_DOWN;

    res = cubeb_init(&this->cubeb_ctx, "Dingus sound server", NULL);
    if (res != CUBEB_OK) {
        LOG_F(ERROR, "Could not initialize Cubeb library");
        return -1;
    }

    LOG_F(INFO, "Connected to backend: %s", cubeb_get_backend_id(this->cubeb_ctx));

    this->status = SND_API_READY;

    return 0;
}

void SoundServer::shutdown()
{
    switch (this->status) {
    case SND_SERVER_UP:
        //soundio_device_unref(this->out_device);
        /* fall through */
    case SND_API_READY:
        cubeb_destroy(this->cubeb_ctx);
    }

    this->status = SND_SERVER_DOWN;

    LOG_F(INFO, "Sound Server shut down.");
}

long sound_out_callback(cubeb_stream *stream, void *user_data,
                        void const *input_buffer, void *output_buffer,
                        long req_frames)
{
    uint8_t *p_in;
    int16_t* in_buf, * out_buf;
    uint32_t got_len;
    long frames, out_frames;
    DmaOutChannel *dma_ch = static_cast<DmaOutChannel*>(user_data); /* C API baby! */

    if (!dma_ch->is_active()) {
        return 0;
    }

    out_buf = (int16_t*)output_buffer;

    out_frames = 0;

    while (req_frames > 0) {
        if (!dma_ch->pull_data(req_frames << 2, &got_len, &p_in)) {
            frames = got_len >> 2;

            in_buf = (int16_t*)p_in;

            for (int i = frames; i > 0; i--) {
                out_buf[0] = BYTESWAP_16(in_buf[0]);
                out_buf[1] = BYTESWAP_16(in_buf[1]);
                in_buf += 2;
                out_buf += 2;
            }

            req_frames -= frames;
            out_frames += frames;
        }
        else {
            break;
        }
    }

    return out_frames;
}

static void status_callback(cubeb_stream *stream, void *user_data, cubeb_state state)
{
    LOG_F(9, "Cubeb status callback fired, status = %d", state);
}

int SoundServer::open_out_stream(uint32_t sample_rate, void *user_data)
{
    int res;
    uint32_t latency_frames;
    cubeb_stream_params params;

    params.format = CUBEB_SAMPLE_S16NE;
    params.rate = sample_rate;
    params.channels = 2;
    params.layout = CUBEB_LAYOUT_STEREO;
    params.prefs = CUBEB_STREAM_PREF_NONE;

    res = cubeb_get_min_latency(this->cubeb_ctx, &params, &latency_frames);
    if (res != CUBEB_OK) {
        LOG_F(ERROR, "Could not get minimum latency, error: %d", res);
        return -1;
    } else {
        LOG_F(INFO, "Minimum sound latency: %d frames", latency_frames);
    }

    res = cubeb_stream_init(this->cubeb_ctx, &this->out_stream, "SndOut stream",
                            NULL, NULL, NULL, &params, latency_frames,
                            sound_out_callback, status_callback, user_data);
    if (res != CUBEB_OK) {
        LOG_F(ERROR, "Could not open sound output stream, error: %d", res);
        return -1;
    }

    LOG_F(INFO, "Sound output stream opened.");

    return 0;
}

int SoundServer::start_out_stream()
{
    return cubeb_stream_start(this->out_stream);
}

void SoundServer::close_out_stream()
{
    cubeb_stream_stop(this->out_stream);
    cubeb_stream_destroy(this->out_stream);
    LOG_F(INFO, "Sound output stream closed.");
}
