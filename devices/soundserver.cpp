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

#include "soundserver.h"
#include <thirdparty/libsoundio/soundio/soundio.h>
#include <thirdparty/loguru/loguru.hpp>

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
