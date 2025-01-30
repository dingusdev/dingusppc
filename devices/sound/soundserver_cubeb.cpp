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

#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX

#include <core/timermanager.h>
#include <cpu/ppc/ppcemu.h>
#include <devices/common/dmacore.h>
#include <devices/sound/soundserver.h>
#include <endianswap.h>

#include <algorithm>
#include <functional>
#include <loguru.hpp>
#include <cubeb/cubeb.h>
#ifdef _WIN32
#include <objbase.h>
#endif

typedef enum {
    SND_SERVER_DOWN = 0,
    SND_API_READY,
    SND_SERVER_UP,
    SND_STREAM_OPENED,
    SND_STREAM_CLOSED
} Status;

class SoundServer::Impl {
public:
    Status status = SND_SERVER_DOWN;
    cubeb *cubeb_ctx;
    cubeb_stream *out_stream;

    uint32_t deterministic_poll_timer = 0;
    std::function<void()> deterministic_poll_cb;
};

SoundServer::SoundServer(): impl(std::make_unique<Impl>())
{
    supports_types(HWCompType::SND_SERVER);
    this->start();
}

SoundServer::~SoundServer()
{
    this->shutdown();
}

int SoundServer::start()
{
    int res;

#ifdef _WIN32
    CoInitialize(nullptr);
#endif

    impl->status = SND_SERVER_DOWN;

    res = cubeb_init(&impl->cubeb_ctx, "Dingus sound server", NULL);
    if (res != CUBEB_OK) {
        LOG_F(ERROR, "Could not initialize Cubeb library");
        return -1;
    }

    LOG_F(INFO, "Connected to backend: %s", cubeb_get_backend_id(impl->cubeb_ctx));

    impl->status = SND_API_READY;

    return 0;
}

void SoundServer::shutdown()
{
    switch (impl->status) {
    case SND_STREAM_OPENED:
        close_out_stream();
        /* fall through */
    case SND_STREAM_CLOSED:
        /* fall through */
    case SND_SERVER_UP:
        /* fall through */
    case SND_API_READY:
        cubeb_destroy(impl->cubeb_ctx);
        break;
    case SND_SERVER_DOWN:
        // Nothing to do.
        break;
    }

    impl->status = SND_SERVER_DOWN;

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

    if (!dma_ch->is_out_active()) {
        return 0;
    }

    out_buf = (int16_t*)output_buffer;

    out_frames = 0;

    while (req_frames > 0) {
        if (!dma_ch->pull_data((uint32_t)req_frames << 2, &got_len, &p_in)) {
            if ((in_buf = (int16_t*)p_in)) {
                frames = got_len >> 2;

                for (int i = (int)frames; i > 0; i--) {
                    out_buf[0] = BYTESWAP_16(in_buf[0]);
                    out_buf[1] = BYTESWAP_16(in_buf[1]);
                    in_buf += 2;
                    out_buf += 2;
                }

                req_frames -= frames;
                out_frames += frames;
            }
            else {
                LOG_F(ERROR, "Didn't get qdata");
            }
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

int SoundServer::open_out_stream(uint32_t sample_rate, DmaOutChannel *dma_ch)
{
    if (is_deterministic) {
        impl->deterministic_poll_cb = [dma_ch] {
            if (!dma_ch->is_out_active()) {
               return;
            }
            // Drain the DMA buffer, but don't do anything else.
            int req_size = std::max(dma_ch->get_pull_data_remaining(), 1024);
            int out_size = 0;
            while (req_size > 0) {
                uint8_t *chunk;
                uint32_t chunk_size;
                if (!dma_ch->pull_data(req_size, &chunk_size, &chunk)) {
                    req_size -= chunk_size;
                } else {
                    break;
                }
            }
        };
        impl->status = SND_STREAM_OPENED;
        LOG_F(9, "Deterministic sound output callback set up.");
        return 0;
    }
    int res;
    uint32_t latency_frames;
    cubeb_stream_params params;

    params.format = CUBEB_SAMPLE_S16NE;
    params.rate = sample_rate;
    params.channels = 2;
    params.layout = CUBEB_LAYOUT_STEREO;
    params.prefs = CUBEB_STREAM_PREF_NONE;

    res = cubeb_get_min_latency(impl->cubeb_ctx, &params, &latency_frames);
    if (res != CUBEB_OK) {
        LOG_F(ERROR, "Could not get minimum latency, error: %d", res);
        return -1;
    } else {
        LOG_F(9, "Minimum sound latency: %d frames", latency_frames);
    }

    res = cubeb_stream_init(impl->cubeb_ctx, &impl->out_stream, "SndOut stream",
                            NULL, NULL, NULL, &params, latency_frames,
                            sound_out_callback, status_callback, dma_ch);
    if (res != CUBEB_OK) {
        LOG_F(ERROR, "Could not open sound output stream, error: %d", res);
        return -1;
    }

    LOG_F(9, "Sound output stream opened.");

    impl->status = SND_STREAM_OPENED;

    return 0;
}

int SoundServer::start_out_stream()
{
    if (is_deterministic) {
        LOG_F(9, "Starting sound output deterministic polling.");
        impl->deterministic_poll_timer =
            TimerManager::get_instance()->add_cyclic_timer(MSECS_TO_NSECS(10), impl->deterministic_poll_cb);
        return 0;
    }
    return cubeb_stream_start(impl->out_stream);
}

void SoundServer::close_out_stream()
{
    if (is_deterministic) {
        LOG_F(9, "Stopping sound output deterministic polling.");
        TimerManager::get_instance()->cancel_timer(impl->deterministic_poll_timer);
        impl->status = SND_STREAM_CLOSED;
        return;
    }
    cubeb_stream_stop(impl->out_stream);
    cubeb_stream_destroy(impl->out_stream);
    impl->status = SND_STREAM_CLOSED;
    LOG_F(9, "Sound output stream closed.");
}
