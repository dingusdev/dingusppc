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

/** ADB mouse definitions

    Simulates Apple Desktop Bus (ADB) standard mice

 */

#include <array>
#include <cinttypes>
#include <cstring>
#include "adbmouse.h"
#include <thirdparty/SDL2/include/SDL.h>
#include <thirdparty/SDL2/include/SDL_events.h>
#include <thirdparty/SDL2/include/SDL_mouse.h>
#include <thirdparty/SDL2/include/SDL_stdinc.h>

using namespace std;

ADB_Mouse::ADB_Mouse() {
    this->adb_mousereg0 = 0x8080;
}

ADB_Mouse::~ADB_Mouse() {
}

void ADB_Mouse::input_mouse() {
	while (SDL_PollEvent(&adb_mouse_evt)) {
		if (adb_mouse_evt.motion.x) {
			this->adb_mousereg0 &= 0x7F;

			if (adb_mouse_evt.motion.xrel < 0) {
				if (adb_mouse_evt.motion.xrel <= -64) {
					this->adb_mousereg0 |= 0x7F;
				}
				else if (adb_mouse_evt.motion.xrel >= 63) {
					this->adb_mousereg0 |= 0x3F;
				}
				else {
					this->adb_mousereg0 |= adb_mouse_evt.motion.xrel;
				}
			}
		}
		if (adb_mouse_evt.motion.y) {
			this->adb_mousereg0 &= 0x7F00;

			if (adb_mouse_evt.motion.yrel < 0) {
				if (adb_mouse_evt.motion.yrel <= -64) {
					this->adb_mousereg0 |= 0x7F00;
				}
				else if (adb_mouse_evt.motion.yrel >= 63) {
					this->adb_mousereg0 |= 0x3F00;
				}
				else {
					this->adb_mousereg0 |= (adb_mouse_evt.motion.yrel << 8);
				}
			}

		}

		switch (adb_mouse_evt.type) {
		case SDL_MOUSEBUTTONDOWN:
			this->adb_mousereg0 &= 0x7FFF;
		case SDL_MOUSEBUTTONUP:
			this->adb_mousereg0 |= 0x8000;
		}
	}

	reg_stream[0] = (this->adb_mousereg0 >> 8);
	reg_stream[1] = (this->adb_mousereg0 & 0xff);
}

uint8_t ADB_Mouse::copy_stream_byte(int offset) {
	return this->reg_stream[offset];
}