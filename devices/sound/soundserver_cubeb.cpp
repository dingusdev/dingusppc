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
#include <devices/common/dmacore.h>
#include <devices/sound/soundserver.h>
#include <endianswap.h>

#include <algorithm>
#include <loguru.hpp>
#include <cubeb/cubeb.h>
#include <cubeb_ringbuffer.h>
#ifdef _WIN32
#include <objbase.h>
#endif

typedef enum {
    SND_SERVER_DOWN = 0,
    SND_API_READY,
    SND_SERVER_UP,
    SND_STREAM_OPENED,
    SND_SERVER_STREAM_STARTED,
    SND_STREAM_CLOSED
} Status;

class SoundServer::Impl {
public:
    Status status = Status::SND_SERVER_DOWN;
    uint32_t poll_timer = 0;
    DmaOutChannel *dma_ch = nullptr;
    lock_free_queue<uint8_t> audio_buffer{2 * sizeof(uint16_t) * 44100}; // 1 second of 16-bit stereo audio at 44100 Hz

    cubeb *cubeb_ctx;
    cubeb_stream *out_stream;
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
    case SND_SERVER_STREAM_STARTED:
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
        // Nothing to do
        break;
    }

    impl->status = SND_SERVER_DOWN;

    LOG_F(INFO, "Sound Server shut down.");
}

long sound_out_callback(cubeb_stream *stream, void *user_data,
                        void const *input_buffer, void *output_buffer,
                        long req_frames)
{
    auto impl = static_cast<SoundServer::Impl*>(user_data); /* C API baby! */

    int req_bytes = int(req_frames) << 2;
    int out_bytes = impl->audio_buffer.dequeue(static_cast<uint8_t*>(output_buffer), req_bytes);
    int out_frames = out_bytes >> 2;
    uint16_t *output_buffer16 = static_cast<uint16_t*>(output_buffer);
    for (int i = 0; i < out_frames; i++) {
        output_buffer16[i * 2] = BYTESWAP_16(output_buffer16[i * 2]);
        output_buffer16[i * 2 + 1] = BYTESWAP_16(output_buffer16[i * 2 + 1]);
    }
    return out_frames;
}

static void status_callback(cubeb_stream *stream, void *user_data, cubeb_state state)
{
    if (state == CUBEB_STATE_ERROR) {
        LOG_F(ERROR, "Cubeb error state");
    } else {
        LOG_F(9, "Cubeb status callback fired, status = %d", state);
    }
}

int SoundServer::open_out_stream(uint32_t sample_rate, DmaOutChannel *dma_channel)
{
    cubeb_stream_params params;
    params.format = CUBEB_SAMPLE_S16NE;
    params.rate = sample_rate;
    params.channels = 2;
    params.layout = CUBEB_LAYOUT_STEREO;
    params.prefs = CUBEB_STREAM_PREF_NONE;

    uint32_t latency_frames;
    int res = cubeb_get_min_latency(impl->cubeb_ctx, &params, &latency_frames);
    if (res != CUBEB_OK) {
        LOG_F(ERROR, "Could not get minimum latency, error: %d", res);
        return -1;
    } else {
        LOG_F(9, "Minimum sound latency: %d frames", latency_frames);
    }

    res = cubeb_stream_init(impl->cubeb_ctx, &impl->out_stream, "SndOut stream",
                            NULL, NULL, NULL, &params, latency_frames,
                            sound_out_callback, status_callback, impl.get());
    if (res != CUBEB_OK) {
        LOG_F(ERROR, "Could not open sound output stream, error: %d", res);
        return -1;
    }

    LOG_F(9, "Sound output stream opened.");

    impl->dma_ch = dma_channel;
    impl->status = SND_STREAM_OPENED;

    return 0;
}

int SoundServer::start_out_stream()
{
    LOG_F(9, "Sound output stream started.");
    impl->poll_timer = TimerManager::get_instance()->add_cyclic_timer(
        MSECS_TO_NSECS(1),
        0,
        [this]() {
            auto dma_ch = impl->dma_ch;
            if (!dma_ch->is_out_active()) {
                return;
            }

            // Try to read as much as possible without filling up the buffer.
            // AmicSndOutDma does not report get_pull_data_remaining so we make
            // a best guess as to how much we need.
            int req_size = std::min(impl->audio_buffer.available_write(), std::max(dma_ch->get_pull_data_remaining(), 1024));

            while (req_size > 0) {
                uint8_t *chunk;
                uint32_t chunk_size;
                if (!dma_ch->pull_data(req_size, &chunk_size, &chunk)) {
                    int write_size = impl->audio_buffer.enqueue(chunk, chunk_size);
                    if (write_size < chunk_size) {
                        LOG_F(WARNING, "Could only write %d/%d bytes into the audio buffer, it's likely full", write_size, chunk_size);
                    }
                    req_size -= chunk_size;
                } else {
                    break;
                }
            }
        });
    impl->status = SND_SERVER_STREAM_STARTED;

    return cubeb_stream_start(impl->out_stream);
}

void SoundServer::close_out_stream()
{
    if (impl->status == SND_SERVER_STREAM_STARTED) {
        TimerManager::get_instance()->cancel_timer(impl->poll_timer);
        impl->poll_timer = 0;
    }
    cubeb_stream_stop(impl->out_stream);
    cubeb_stream_destroy(impl->out_stream);
    impl->status = SND_STREAM_CLOSED;
    impl->dma_ch = nullptr;
    LOG_F(9, "Sound output stream closed.");
}
