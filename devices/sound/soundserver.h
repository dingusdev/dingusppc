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

#include <devices/common/hwcomponent.h>

#include <memory>

class DmaOutChannel;

class SoundServer : public HWComponent {
public:
    SoundServer();
    ~SoundServer();

    int start();
    void shutdown();
    int open_out_stream(uint32_t sample_rate, DmaOutChannel *dma_ch);
    int start_out_stream();
    void close_out_stream();

private:
    class Impl; // Holds private fields
    std::unique_ptr<Impl> impl;
};

#endif /* SOUND_SERVER_H */
