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

/** @file Block storage device implementation. */

#include <devices/storage/blockstoragedevice.h>

#include <cstring>

using namespace std;

BlockStorageDevice::BlockStorageDevice(const uint32_t cache_blocks,
                                       const uint32_t block_size,
                                       const uint64_t max_blocks) {
    this->block_size   = block_size;
    this->raw_blk_size = block_size;
    this->cache_blocks = cache_blocks;
    this->max_blocks   = max_blocks;
    this->is_ready     = false;
    this->cache_size   = 0;
    this->data_offset  = 0;
}

BlockStorageDevice::~BlockStorageDevice() {
    this->img_file.close();
}

int BlockStorageDevice::set_host_file(std::string file_path) {
    this->is_ready = false;

    if (!this->img_file.open(file_path))
        return -1;

    this->size_bytes  = this->img_file.size();

    this->set_fpos(0);

    this->is_ready = true;

    return this->set_block_size(this->block_size);
}

int BlockStorageDevice::set_block_size(const int blk_size) {
    this->raw_blk_size = blk_size;

    if (this->is_ready) {
        this->size_blocks = this->size_bytes / this->raw_blk_size;
        if (this->size_blocks > this->max_blocks)
            return -1;
    }

    this->cache_size = this->cache_blocks * this->raw_blk_size;

    // allocate data cache and fill it with zeroes
    this->data_cache = std::unique_ptr<char[]>(new char[this->cache_size] ());

    return 0;
}

int BlockStorageDevice::set_fpos(const uint32_t lba) {
    this->cur_fpos = (uint64_t)lba * (uint64_t)this->raw_blk_size;
    return 0;
}

void BlockStorageDevice::fill_cache(const int nblocks) {
    uint32_t read_size = nblocks * this->raw_blk_size;

    this->img_file.read(this->data_cache.get(), this->cur_fpos, read_size);
    this->cur_fpos += read_size;

    // extract block data from raw images while discarding anything else
    if (this->raw_blk_size > this->block_size) {
        char *cache_ptr = this->data_cache.get();

        for (int blk = 0; blk < nblocks; blk++) {
            memcpy(cache_ptr + blk * this->block_size,
                   cache_ptr + blk * this->raw_blk_size + this->data_offset,
                   this->block_size
            );
        }
    }
}

int BlockStorageDevice::read_begin(int nblocks, uint32_t max_len) {
    uint32_t xfer_len = std::min(this->cache_blocks * this->block_size, max_len);
    uint32_t read_size = nblocks * this->block_size;
    if (read_size > xfer_len) {
        this->remain_size = read_size - xfer_len;
        read_size = xfer_len;
    } else {
        this->remain_size = 0;
    }

    this->fill_cache(read_size / this->block_size);

    return read_size;
}

int BlockStorageDevice::read_more() {
    if (!this->remain_size)
        return 0;

    uint32_t read_size = this->cache_blocks * this->block_size;

    if (this->remain_size > read_size) {
        this->remain_size -= read_size;
    } else {
        read_size = this->remain_size;
        this->remain_size = 0;
    }

    this->fill_cache(read_size / this->block_size);

    return read_size;
}

int BlockStorageDevice::write_begin(char *buf, int nblocks) {
    if (!this->is_writeable)
        return -1;

    return 0;
}
