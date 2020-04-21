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

#ifndef ADB_H
#define ADB_H

#include <cinttypes>
#include <thirdparty/SDL2/include/SDL.h>
#include <thirdparty/SDL2/include/SDL_events.h>

class ADB_Input
{
public:
    ADB_Input();
    ~ADB_Input();

    void adb_input_keybd();
    void adb_input_mouse();

private:
    SDL_Event adb_keybd_evt;
    SDL_Event adb_mouse_evt;

    uint16_t adb_mousereg0;

    uint16_t ask_register0;
    uint16_t ask_register2; 
    
    uint8_t ask_key_pressed;
    uint8_t mod_key_pressed;

    bool confirm_ask_reg_2;
};

#endif /* ADB_H */