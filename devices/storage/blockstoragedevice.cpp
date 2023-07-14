/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-23 divingkatae and maximum
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
#include <loguru.hpp>

#include <stdio.h>
#include <sys/stat.h>

using namespace std;

BlockStorageDevice::BlockStorageDevice(const uint32_t cache_blocks, const uint32_t block_size, const uint64_t max_blocks) {
    this->block_size = block_size;
    this->cache_size = cache_blocks * this->block_size;
    this->max_blocks = max_blocks;

    // allocate device cache and fill it with zeroes
    this->data_cache = std::unique_ptr<char[]>(new char[this->cache_size] ());
}

BlockStorageDevice::~BlockStorageDevice() {
    if (this->img_file)
        this->img_file.close();
}

int BlockStorageDevice::set_host_file(std::string file_path) {
    this->is_ready = false;

    this->img_file.open(file_path, ios::out | ios::in | ios::binary);

    struct stat stat_buf;
    int rc = stat(file_path.c_str(), &stat_buf);
    if (rc)
        return -1;

    this->size_bytes  = stat_buf.st_size;
    this->size_blocks = this->size_bytes / this->block_size;
    if (this->size_blocks > this->max_blocks)
        return -1;

    this->set_fpos(0);

    this->is_ready = true;

    return 0;
}

int BlockStorageDevice::set_fpos(const uint32_t lba) {
    this->cur_fpos = lba * this->block_size;
    this->img_file.seekg(this->cur_fpos, this->img_file.beg);
    return 0;
}

int BlockStorageDevice::read_begin(int nblocks, uint32_t max_len) {
    uint32_t xfer_len = std::min(this->cache_size, max_len);
    uint32_t read_size = nblocks * this->block_size;
    if (read_size > xfer_len) {
        this->remain_size = read_size - xfer_len;
        read_size = xfer_len;
    } else {
        this->remain_size = 0;
    }

    this->img_file.read(this->data_cache.get(), read_size);
    this->cur_fpos += read_size;

    return read_size;
}

int BlockStorageDevice::read_more() {
    uint32_t read_size;

    if (!this->remain_size)
        return 0;

    if (this->remain_size > this->cache_size) {
        this->remain_size -= this->cache_size;
        read_size = this->cache_size;
    } else {
        read_size = this->remain_size;
        this->remain_size = 0;
    }

    this->img_file.read(this->data_cache.get(), read_size);
    this->cur_fpos += read_size;

    return read_size;
}

int BlockStorageDevice::write_begin(char *buf, int nblocks) {
    if (!this->is_writeable)
        return -1;

    return 0;
}
