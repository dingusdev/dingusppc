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

/** ADB keyboard definitions

    Simulates Apple Desktop Bus (ADB) standard keyboard

 */

#include <array>
#include <cinttypes>
#include <cstring>
#include "adbkeybd.h"
#include <thirdparty/SDL2/include/SDL.h>
#include <thirdparty/SDL2/include/SDL_events.h>
#include <thirdparty/SDL2/include/SDL_mouse.h>
#include <thirdparty/SDL2/include/SDL_stdinc.h>

using namespace std;

ADB_Keybd::ADB_Keybd() {
	this->ask_register0 = 0xFFFF;
	this->ask_register2 = 0xFFFF;
}

ADB_Keybd::~ADB_Keybd() {
}

void ADB_Keybd::input_keybd(int reg) {
	while (SDL_PollEvent(&adb_keybd_evt)) {
		//Poll our SDL key event for any keystrokes.
		switch (adb_keybd_evt.type) {
		case SDL_KEYDOWN:
			switch (adb_keybd_evt.key.keysym.sym) {
			case SDLK_a:
				this->ask_key_pressed = 0x00;
				break;
			case SDLK_s:
				this->ask_key_pressed = 0x01;
				break;
			case SDLK_d:
				this->ask_key_pressed = 0x02;
				break;
			case SDLK_f:
				this->ask_key_pressed = 0x03;
				break;
			case SDLK_h:
				this->ask_key_pressed = 0x04;
				break;
			case SDLK_g:
				this->ask_key_pressed = 0x05;
				break;
			case SDLK_z:
				this->ask_key_pressed = 0x06;
				break;
			case SDLK_x:
				this->ask_key_pressed = 0x07;
				break;
			case SDLK_c:
				this->ask_key_pressed = 0x08;
				break;
			case SDLK_v:
				this->ask_key_pressed = 0x09;
				break;
			case SDLK_b:
				this->ask_key_pressed = 0x0B;
				break;
			case SDLK_q:
				this->ask_key_pressed = 0x0C;
				break;
			case SDLK_w:
				this->ask_key_pressed = 0x0D;
				break;
			case SDLK_e:
				this->ask_key_pressed = 0x0E;
				break;
			case SDLK_r:
				this->ask_key_pressed = 0x0F;
				break;
			case SDLK_y:
				this->ask_key_pressed = 0x10;
				break;
			case SDLK_t:
				this->ask_key_pressed = 0x11;
				break;
			case SDLK_1:
				this->ask_key_pressed = 0x12;
				break;
			case SDLK_2:
				this->ask_key_pressed = 0x13;
				break;
			case SDLK_3:
				this->ask_key_pressed = 0x14;
				break;
			case SDLK_4:
				this->ask_key_pressed = 0x15;
				break;
			case SDLK_6:
				this->ask_key_pressed = 0x16;
				break;
			case SDLK_5:
				this->ask_key_pressed = 0x17;
				break;
			case SDLK_EQUALS:
				this->ask_key_pressed = 0x18;
				break;
			case SDLK_9:
				this->ask_key_pressed = 0x19;
				break;
			case SDLK_7:
				this->ask_key_pressed = 0x1A;
				break;
			case SDLK_MINUS:
				this->ask_key_pressed = 0x1B;
				break;
			case SDLK_8:
				this->ask_key_pressed = 0x1C;
				break;
			case SDLK_0:
				this->ask_key_pressed = 0x1D;
				break;
			case SDLK_RIGHTBRACKET:
				this->ask_key_pressed = 0x1E;
				break;
			case SDLK_o:
				this->ask_key_pressed = 0x1F;
				break;
			case SDLK_u:
				this->ask_key_pressed = 0x20;
				break;
			case SDLK_LEFTBRACKET:
				this->ask_key_pressed = 0x21;
				break;
			case SDLK_i:
				this->ask_key_pressed = 0x22;
				break;
			case SDLK_p:
				this->ask_key_pressed = 0x23;
				break;
			case SDLK_RETURN:
				this->ask_key_pressed = 0x24;
				break;
			case SDLK_l:
				this->ask_key_pressed = 0x25;
				break;
			case SDLK_j:
				this->ask_key_pressed = 0x26;
				break;
			case SDLK_QUOTE:
				this->ask_key_pressed = 0x27;
				break;
			case SDLK_k:
				this->ask_key_pressed = 0x28;
				break;
			case SDLK_SEMICOLON:
				this->ask_key_pressed = 0x29;
				break;
			case SDLK_BACKSLASH:
				this->ask_key_pressed = 0x2A;
				break;
			case SDLK_COMMA:
				this->ask_key_pressed = 0x2B;
				break;
			case SDLK_SLASH:
				this->ask_key_pressed = 0x2C;
				break;
			case SDLK_n:
				this->ask_key_pressed = 0x2D;
				break;
			case SDLK_m:
				this->ask_key_pressed = 0x2E;
				break;
			case SDLK_PERIOD:
				this->ask_key_pressed = 0x2F;
				break;
			case SDLK_BACKQUOTE:
				this->ask_key_pressed = 0x32;
				break;
			case SDLK_ESCAPE:
				this->ask_key_pressed = 0x35;
				break;
			case SDLK_LEFT:
				this->ask_key_pressed = 0x3B;
				break;
			case SDLK_RIGHT:
				this->ask_key_pressed = 0x3C;
				break;
			case SDLK_DOWN:
				this->ask_key_pressed = 0x3D;
				break;
			case SDLK_UP:
				this->ask_key_pressed = 0x3E;
				break;
			case SDLK_KP_PERIOD:
				this->ask_key_pressed = 0x41;
				break;
			case SDLK_KP_MULTIPLY:
				this->ask_key_pressed = 0x43;
				break;
			case SDLK_KP_PLUS:
				this->ask_key_pressed = 0x45;
				break;
			case SDLK_DELETE:
				this->ask_key_pressed = 0x47;
				break;
			case SDLK_KP_DIVIDE:
				this->ask_key_pressed = 0x4B;
				break;
			case SDLK_KP_ENTER:
				this->ask_key_pressed = 0x4C;
				break;
			case SDLK_KP_MINUS:
				this->ask_key_pressed = 0x4E;
				break;
			case SDLK_KP_0:
				this->ask_key_pressed = 0x52;
				break;
			case SDLK_KP_1:
				this->ask_key_pressed = 0x53;
				break;
			case SDLK_KP_2:
				this->ask_key_pressed = 0x54;
				break;
			case SDLK_KP_3:
				this->ask_key_pressed = 0x55;
				break;
			case SDLK_KP_4:
				this->ask_key_pressed = 0x56;
				break;
			case SDLK_KP_5:
				this->ask_key_pressed = 0x57;
				break;
			case SDLK_KP_6:
				this->ask_key_pressed = 0x58;
				break;
			case SDLK_KP_7:
				this->ask_key_pressed = 0x59;
				break;
			case SDLK_KP_8:
				this->ask_key_pressed = 0x5B;
				break;
			case SDLK_KP_9:
				this->ask_key_pressed = 0x5C;
				break;
			case SDLK_BACKSPACE:
				this->ask_key_pressed = 0x33;
				this->confirm_ask_reg_2 = true;
				this->mod_key_pressed = 0x40;
			case SDLK_CAPSLOCK:
				this->ask_key_pressed = 0x39;
				this->confirm_ask_reg_2 = true;
				this->mod_key_pressed = 0x20;
			case SDLK_LCTRL:
			case SDLK_RCTRL:
				this->ask_key_pressed = 0x36;
				this->confirm_ask_reg_2 = true;
				this->mod_key_pressed = 0x8;
			case SDLK_LSHIFT:
			case SDLK_RSHIFT:
				this->ask_key_pressed = 0x38;
				this->confirm_ask_reg_2 = true;
				this->mod_key_pressed = 0x4;
			case SDLK_LALT:
			case SDLK_RALT:
				this->ask_key_pressed = 0x3A;
				this->confirm_ask_reg_2 = true;
				this->mod_key_pressed = 0x2;
			case SDLK_HOME: //Temp key for the Command/Apple key
				this->ask_key_pressed = 0x37;
				this->confirm_ask_reg_2 = true;
				this->mod_key_pressed = 0x1;
			default:
				break;
			}

			if (this->ask_register0 & 0x8000) {
				this->ask_register0 &= 0x7FFF;
				this->ask_register0 &= (ask_key_pressed << 8);
			}
			else if (this->ask_register0 & 0x80) {
				this->ask_register0 &= 0xFF7F;
				this->ask_register0 &= (ask_key_pressed);
			}

			//check if mod keys are being pressed

			if (this->confirm_ask_reg_2) {
				this->ask_register2 |= (this->mod_key_pressed << 8);
			}

			break;

		case SDL_KEYUP:
			if (!(this->ask_register0 & 0x8000)) {
				this->ask_register0 |= 0x8000;
			}
			else if (this->ask_register0 & 0x80)
				this->ask_register0 |= 0x0080;

			if (this->confirm_ask_reg_2) {
				this->ask_register2 &= (this->mod_key_pressed << 8);
				this->confirm_ask_reg_2 = false;
			}
		}
	}

	if (reg == 0) {
		reg_stream[0] = (this->ask_register0 >> 8);
		reg_stream[1] = (this->ask_register0 & 0xff);
	}
	else if (reg == 2) {
		reg_stream[0] = (this->ask_register2 >> 8);
		reg_stream[1] = (this->ask_register2 & 0xff);
	}
}

uint8_t ADB_Keybd::copy_stream_byte(int offset) {
	return this->reg_stream[offset];
}