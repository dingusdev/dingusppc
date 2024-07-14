/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-24 divingkatae and maximum
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

/** @file Block storage device definitions. */

#ifndef BLOCK_STORAGE_DEVICE_H
#define BLOCK_STORAGE_DEVICE_H

#include <utils/imgfile.h>

#include <cinttypes>
#include <memory>
#include <string>

class BlockStorageDevice {
public:
    BlockStorageDevice(const uint32_t cache_blocks, const uint32_t block_size=512,
                       const uint64_t max_blocks=0xffffffffffffffff);
    ~BlockStorageDevice();

    int set_host_file(std::string file_path);
    int set_block_size(const int blk_size);

    int set_fpos(const uint32_t lba);
    int read_begin(int nblocks, uint32_t max_len);
    int data_left() { return this->remain_size; };
    int read_more();
    int write_begin(char *buf, int nblocks);

protected:
    void fill_cache(const int nblocks);

    ImgFile         img_file;
    uint64_t        size_bytes   = 0;   // image file size in bytes
    uint64_t        size_blocks  = 0;   // image file size in blocks
    uint64_t        max_blocks   = 0;   // maximum number of blocks supported
    uint64_t        cur_fpos     = 0;   // current image file pointer position
    uint32_t        block_size   = 512; // physical block size
    uint32_t        raw_blk_size = 512; // block size for raw images (CD-ROM)
    uint32_t        data_offset  = 0;   // offset to block data for raw images
    uint32_t        cache_blocks = 0;
    uint32_t        cache_size   = 0;   // cache size
    uint32_t        remain_size  = 0;
    bool            is_writeable = false;
    bool            is_ready     = false; // ready for operation

    std::unique_ptr<char[]>  data_cache;
};

#endif // BLOCK_STORAGE_DEVICE_H
