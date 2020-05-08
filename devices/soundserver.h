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

/** @file Sound server definitions.

    This class manages host audio HW. It's directly connected
    to a sound abstraction API (libsoundio in our case).

    Sound server provides a way to select between various
    host input and output devices independendly of emulated
    sound HW.

    Emulated sound HW only need to process sound streams.
 */

#ifndef SOUND_SERVER_H
#define SOUND_SERVER_H

#include "hwcomponent.h"
#include <thirdparty/libsoundio/soundio/soundio.h>

enum {
    SND_SERVER_DOWN = 0,
    SND_API_READY,
    SND_SERVER_UP,
};


class SoundServer : public HWComponent {
public:
    SoundServer()  { this->start();    };
    ~SoundServer() { this->shutdown(); };

    int start();
    void shutdown();

    bool supports_type(HWCompType type) { return type == HWCompType::SND_SERVER; };

    SoundIoDevice *get_out_device() {
        if (this->status == SND_SERVER_UP) {
            return this->out_device;
        } else {
            return NULL;
        }
    };

private:
    int status;                     /* server status */
    SoundIo *soundio;
    int out_dev_index;              /* current output device index */
    SoundIoDevice *out_device;      /* current output device instance */
    //SoundIoOutStream *outstream;
};

#endif /* SOUND_SERVER_H */
