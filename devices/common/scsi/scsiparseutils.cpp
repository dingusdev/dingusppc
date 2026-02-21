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

#include <devices/common/scsi/scsiparseutils.h>
#include <loguru.hpp>

#include <cstring>

int parse_mode_select_6(const uint8_t* data, int data_len, const char* dev_name,
                        ModeSelectData& result)
{
    std::memset(&result, 0, sizeof(result));

    // Need at least the 4-byte mode parameter header
    if (data_len < 4) {
        if (dev_name)
            LOG_F(WARNING, "%s: MODE SELECT parameter list too short (%d bytes)",
                dev_name, data_len);
        return -1;
    }

    // Parse the mode parameter header (4 bytes for MODE SELECT(6))
    // Byte 0: mode data length (reserved for MODE SELECT, should be 0)
    result.medium_type   = data[1];
    result.device_param  = data[2];
    result.block_desc_len = data[3];

    if (dev_name)
        LOG_F(INFO, "%s: MODE SELECT medium_type=%d, dev_param=0x%02X, bdl=%d",
            dev_name, result.medium_type, result.device_param, result.block_desc_len);

    // Validate block descriptor length fits in the data
    if (4 + result.block_desc_len > data_len) {
        if (dev_name)
            LOG_F(WARNING, "%s: MODE SELECT block descriptor overflows data",
                dev_name);
        return -1;
    }

    // Parse block descriptor if present
    if (result.block_desc_len >= 8) {
        result.bd_num_blocks = (data[5] << 16) | (data[6] << 8) | data[7];
        result.bd_block_len  = (data[9] << 16) | (data[10] << 8) | data[11];
        if (dev_name && result.bd_num_blocks)
            LOG_F(INFO, "%s: MODE SELECT block descriptor: num_blocks=%d, block_len=%d",
                dev_name, result.bd_num_blocks, result.bd_block_len);
    }

    // Parse mode pages
    int offset = 4 + result.block_desc_len;
    result.num_pages = 0;

    while (offset + 1 < data_len) {
        uint8_t page_code = data[offset] & 0x3F;
        uint8_t page_len  = data[offset + 1];

        if (offset + 2 + page_len > data_len) {
            if (dev_name)
                LOG_F(WARNING, "%s: MODE SELECT page 0x%02X truncated "
                    "(need %d bytes at offset %d, only %d available)",
                    dev_name, page_code, page_len, offset + 2, data_len - offset - 2);
            return -1;
        }

        if (result.num_pages < ModeSelectData::MAX_PAGES) {
            result.pages[result.num_pages].page_code   = page_code;
            result.pages[result.num_pages].page_len    = page_len;
            result.pages[result.num_pages].data_offset = offset + 2;
            result.num_pages++;
        }

        if (dev_name)
            LOG_F(INFO, "%s: MODE SELECT page 0x%02X, length %d",
                dev_name, page_code, page_len);

        offset += 2 + page_len;
    }

    return result.num_pages;
}
