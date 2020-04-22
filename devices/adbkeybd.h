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

#ifndef ADBKEYBD_H
#define ADBKEYBD_H

#include <array>
#include <cinttypes>
#include <thirdparty/SDL2/include/SDL.h>
#include <thirdparty/SDL2/include/SDL_events.h>

using namespace std;

class ADB_Keybd
{
public:
    ADB_Keybd();
    ~ADB_Keybd();

    void input_keybd(int reg);
    uint8_t copy_stream_byte(int offset);

private:
    SDL_Event adb_keybd_evt;

    uint16_t ask_register0;
    uint16_t ask_register2;

    uint8_t ask_key_pressed;
    uint8_t mod_key_pressed;

    uint8_t reg_stream[2] = { 0 };

    bool confirm_ask_reg_2;
};

#endif /* ADB_H */