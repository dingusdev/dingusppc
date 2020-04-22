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

/** ADB bus definitions

    Simulates Apple Desktop Bus (ADB) bus

 */

#include <array>
#include <cinttypes>
#include <cstring>
#include "adb.h"
#include "adbkeybd.h"
#include "adbmouse.h"
#include <thirdparty/SDL2/include/SDL.h>
#include <thirdparty/SDL2/include/SDL_events.h>
#include <thirdparty/SDL2/include/SDL_keycode.h>
#include <thirdparty/SDL2/include/SDL_mouse.h>
#include <thirdparty/SDL2/include/SDL_stdinc.h>

using namespace std;

ADB_Bus::ADB_Bus() {
    //set data streams as clear
    input_stream_len = 0;
    output_stream_len = 0;

    this->keyboard = new ADB_Keybd();
    this->mouse = new ADB_Mouse();
}

ADB_Bus::~ADB_Bus() {

}

void ADB_Bus::add_adb_device(int type) {

}

bool ADB_Bus::adb_verify_listen(int device, int reg) {
    switch (device) {
    case 0:
        if ((reg == 0) | (reg == 2)) {
            output_stream_len = 2;
            for (int i = 0; output_stream_len < 2; i++) {
                output_data_stream[i] = this->keyboard->copy_stream_byte(i);
            }
            return true;
        }
        else {
            return false;
        }
    case 1:
        if (reg == 0) {
            output_stream_len = 2;
            for (int i = 0; output_stream_len < 2; i++) {
                output_data_stream[i] = this->mouse->copy_stream_byte(i);
            }
            return true;
        }
        else {
            return false;
        }
    default:
        return false;
    }
}

bool ADB_Bus::adb_verify_talk(int device, int reg) {
    //temp code
    return false;
}

uint8_t ADB_Bus::get_input_byte(int offset) {
    return input_data_stream[offset];
}

uint8_t ADB_Bus::get_output_byte(int offset) {
    return output_data_stream[offset];
}

int ADB_Bus::get_input_len() {
    return input_stream_len;
}

int ADB_Bus::get_output_len() {
    return output_stream_len;
}