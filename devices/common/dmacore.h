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

/** @file Core definitions for direct memory access (DMA) emulation. */

#ifndef DMA_CORE_H
#define DMA_CORE_H

#include <cinttypes>

enum DmaPullResult : int {
    MoreData,       // data source has more data to be pulled
    NoMoreData,     // data source has no more data to be pulled
};

class DmaOutChannel {
public:
    virtual bool            is_active() { return true; };
    virtual DmaPullResult   pull_data(uint32_t req_len, uint32_t *avail_len,
                                      uint8_t **p_data) = 0;
};

// Base class for bidirectional DMA channels.
class DmaBidirChannel {
public:
    virtual bool            is_active() { return true; };
    virtual int             push_data(const char* src_ptr, int len) = 0;
    virtual DmaPullResult   pull_data(uint32_t req_len, uint32_t *avail_len,
                                      uint8_t **p_data) = 0;
};

#endif // DMA_CORE_H
