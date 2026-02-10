/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

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

/** @file SCSI Mode Select parameter list parsing utilities. */

#ifndef SCSI_PARSE_UTILS_H
#define SCSI_PARSE_UTILS_H

#include <cinttypes>

/** Result of parsing a mode page entry. */
struct ModePageParsed {
    uint8_t page_code;
    uint8_t page_len;
    int     data_offset; // byte offset to page data (past page_code + page_len)
};

/** Parsed MODE SELECT(6) parameter list. */
struct ModeSelectData {
    uint8_t  medium_type;
    uint8_t  device_param;
    uint8_t  block_desc_len;

    // Block descriptor fields (valid when block_desc_len >= 8)
    uint32_t bd_num_blocks;
    uint32_t bd_block_len;

    int      num_pages;
    static const int MAX_PAGES = 16;
    ModePageParsed pages[MAX_PAGES];
};

/** Parse a MODE SELECT(6) parameter list.
 *
 *  @param data       Pointer to the parameter list data.
 *  @param data_len   Length of the parameter list in bytes.
 *  @param dev_name   Device name for log messages (may be nullptr to suppress logging).
 *  @param result     Parsed result output.
 *  @return           Number of pages parsed, or -1 on structural error.
 */
int parse_mode_select_6(const uint8_t* data, int data_len, const char* dev_name,
                        ModeSelectData& result);

#endif // SCSI_PARSE_UTILS_H
