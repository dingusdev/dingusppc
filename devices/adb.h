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

#ifndef ADB_H
#define ADB_H

#include <cinttypes>
#include <thirdparty/SDL2/include/SDL.h>
#include <thirdparty/SDL2/include/SDL_events.h>

enum adb_default_values {
    adb_reserved0,
    adb_reserved1,
    adb_encoded,
    adb_relative,
    adb_absolute,
    adb_reserved5,
    adb_reserved6,
    adb_reserved7,
    adb_other8,
    adb_other9,
    adb_other10,
    adb_other11,
    adb_other12,
    adb_other13,
    adb_other14,
    adb_other15
};

class ADB_Bus {
public:
    ADB_Bus();
    ~ADB_Bus();

    bool listen(int device, int reg);
    bool talk(int device, int reg, uint16_t value);
    bool bus_reset();
    bool set_addr(int dev_addr, int new_addr);
    bool flush(int dev_addr);

    bool adb_keybd_listen(int reg);
    bool adb_mouse_listen(int reg);

    uint8_t get_input_byte(int offset);
    uint8_t get_output_byte(int offset);

    int get_input_len();
    int get_output_len();

private:
    int keyboard_access_no;
    int mouse_access_no;

    // Keyboard Variables

    uint16_t adb_keybd_register0;
    uint16_t adb_keybd_register2;
    uint16_t adb_keybd_register3;

    SDL_Event adb_keybd_evt;

    uint8_t ask_key_pressed;
    uint8_t mod_key_pressed;

    bool confirm_ask_reg_2;

    // Mouse Variables
    SDL_Event adb_mouse_evt;

    uint16_t adb_mouse_register0;
    uint16_t adb_mouse_register3;

    uint8_t input_data_stream[16];    // temp buffer
    int input_stream_len;
    uint8_t output_data_stream[16];    // temp buffer
    int output_stream_len;
};

#endif /* ADB_H */