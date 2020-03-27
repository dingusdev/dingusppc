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

#include <atirage.h>
#include <cstdint>
#include "endianswap.h"
#include <thirdparty/loguru/loguru.hpp>

ATIRage::ATIRage() {

}

uint32_t ATIRage::read(int reg, int size) {
    LOG_F(INFO, "Reading reg=%X, size %d", reg, (size * 8));

}

uint32_t ATIRage::write(int reg, uint32_t value, int size) {
    LOG_F(INFO, "Writing reg=%X, value=%X, size %d", reg, value, (size * 8));
}

void ATIRage::atirage_init() {

}