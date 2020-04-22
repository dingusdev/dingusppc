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

/** ADB input device definitions

    Simulates Apple Desktop Bus (ADB) keyboard and mice

 */

#ifndef ADBMOUSE_H
#define ADBMOUSE_H

#include <array>
#include <cinttypes>
#include <thirdparty/SDL2/include/SDL.h>
#include <thirdparty/SDL2/include/SDL_events.h>

class ADB_Mouse
{
public:
    ADB_Mouse();
    ~ADB_Mouse();

    void input_mouse();
    uint8_t copy_stream_byte(int offset);

private:
    SDL_Event adb_mouse_evt;

    uint16_t adb_mousereg0;
    uint8_t reg_stream[2] = { 0 };
};

#endif /* ADB_H */