/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-22 divingkatae and maximum
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

/** IBM RGB514 RAMDAC definitions. */

#include <cinttypes>

#ifndef IBM_RGB514_DEFS_H
#define IBM_RGB514_DEFS_H

namespace Rgb514 {

#define PLL_ENAB    1

/** RGB514 direct register addresses. */
enum {
    // standard VGA registers
    CLUT_ADDR_WR = 0,
    CLUT_DATA    = 1,
    CLUT_MASK    = 2,
    CLUT_ADDR_RD = 3,

    // extended registers
    INDEX_LOW    = 4,
    INDEX_HIGH   = 5,
    INDEX_DATA   = 6,
    INDEX_CNTL   = 7
};

/** RGB514 indirect registers. */
enum {
    MISC_CLK_CNTL   = 0x0002,
    HOR_SYNC_POS    = 0x0004,
    PWR_MNMGMT      = 0x0005,
    PIX_FORMAT      = 0x000A,
    PLL_CTL_1       = 0x0010,
    F0_M0           = 0x0020,
    F1_N0           = 0x0021,
    MISC_CNTL_1     = 0x0070,
    MISC_CNTL_2     = 0x0071,
    VRAM_MASK_LO    = 0x0090,
    VRAM_MASK_HI    = 0x0091,
};

}; // namespace Rgb514

#endif // IBM_RGB514_DEFS_H
