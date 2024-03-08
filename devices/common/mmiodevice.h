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

#ifndef MMIO_DEVICE_H
#define MMIO_DEVICE_H

#include <devices/common/hwcomponent.h>

#include <cinttypes>
#include <string>

/** Abstract class representing a simple, memory-mapped I/O device */
class MMIODevice : public HWComponent {
public:
    MMIODevice()                                                                      = default;
    virtual uint32_t read(uint32_t rgn_start, uint32_t offset, int size)              = 0;
    virtual void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) = 0;
    virtual ~MMIODevice()                                                             = default;
};

#define SIZE_ARG(size) (size == 4 ? 'l' : size == 2 ? 'w' : \
                        size == 1 ? 'b' : '0' + size)

#endif /* MMIO_DEVICE_H */
