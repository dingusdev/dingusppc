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

/** @file Support for reading and writing of various floppy images. */

#include "floppyimg.h"
#include <loguru.hpp>
#include <memaccess.h>

#include <cinttypes>
#include <fstream>
#include <string>

static FlopImgType identify_image(std::ifstream& img_file)
{
    // WOZ images identification strings
    static uint8_t WOZ1_SIG[] = {0x57, 0x4F, 0x5A, 0x31, 0xFF, 0x0A, 0x0D, 0x0A};
    static uint8_t WOZ2_SIG[] = {0x57, 0x4F, 0x5A, 0x32, 0xFF, 0x0A, 0x0D, 0x0A};

    uint8_t buf[8] = { 0 };

    img_file.seekg(0, std::ios::beg);
    img_file.read((char *)buf, sizeof(buf));

    // WOZ files are easily identified
    if (!std::memcmp(buf, WOZ1_SIG, sizeof(buf))) {
        return FlopImgType::WOZ1;
    } else if (!std::memcmp(buf, WOZ2_SIG, sizeof(buf))) {
        return FlopImgType::WOZ2;
    } else {
        for (int offset = 0; offset <=84; offset += 84) {
            // rewind to logical block 2
            img_file.seekg(2*BLOCK_SIZE + offset, std::ios::beg);
            img_file.read((char *)buf, sizeof(buf));

            // check for HFS/MFS signature at the start of the logical block 2
            if ((buf[0] == 0x42 && buf[1] == 0x44) ||
                (buf[0] == 0xD2 && buf[1] == 0xD7)) {
                if (offset) {
                    return FlopImgType::DC42;
                } else {
                    return FlopImgType::RAW;
                }
            }
        }
    }

    return FlopImgType::UNKNOWN;
}

static int64_t get_hfs_vol_size(const uint8_t *mdb_data)
{
    uint16_t drNmAlBlks = READ_WORD_BE_A(&mdb_data[18]);
    uint32_t drAlBlkSiz = READ_DWORD_BE_A(&mdb_data[20]);

    // calculate size of the volume bitmap
    uint32_t vol_bmp_size = (((drNmAlBlks + 8) >> 3) + 512) & 0xFFFFFE00UL;

    return (drNmAlBlks * drAlBlkSiz + vol_bmp_size + 3*BLOCK_SIZE);
}

//======================= RAW IMAGE CONVERTER ============================
RawFloppyImg::RawFloppyImg(std::string& file_path) : FloppyImgConverter()
{
    this->img_path = file_path;
}

/** For raw images, we're going to ensure that the data fits into
    one of the supported floppy disk sizes as well as image size
    matches the size of the embedded HFS/MFS volume.
    Then we'll attempt to guess disk format based on image size.
*/
int RawFloppyImg::calc_phys_params()
{
    std::ifstream img_file;

    img_file.open(img_path, std::ios::in | std::ios::binary);
    if (img_file.fail()) {
        img_file.close();
        LOG_F(ERROR, "RawFloppyImg: Could not open specified floppy image!");
        return -1;
    }

    // determine image size
    img_file.seekg(0, img_file.end);
    this->img_size = img_file.tellg();
    img_file.seekg(0, img_file.beg);

    // verify image size
    if (this->img_size < 5*BLOCK_SIZE) {
        img_file.close();
        LOG_F(ERROR, "RawFloppyImg: image too short!");
        return -1;
    }

    if (this->img_size > MFM_HD_SIZE) {
        img_file.close();
        LOG_F(ERROR, "RawFloppyImg: image too big!");
        return -1;
    }

    // read Master Directory Block from logical block 2
    uint8_t buf[512] = { 0 };

    img_file.seekg(2*BLOCK_SIZE, img_file.beg);
    img_file.read((char *)buf, sizeof(buf));
    img_file.close();

    uint64_t vol_size = 0;

    if (buf[0] == 0x42 && buf[1] == 0x44) {
        // check HFS volume size
        vol_size = get_hfs_vol_size(buf);
    } else if (buf[0] == 0xD2 && buf[1] == 0xD7) {
        // check MFS volume size
    } else {
        LOG_F(ERROR, "RawFloppyImg: unknown volume type!");
        return -1;
    }

    if (vol_size > this->img_size) {
        LOG_F(INFO, "RawFloppyImg: volume size > image size!");
        LOG_F(INFO, "Volume size: %llu, Image size: %d", vol_size, this->img_size);
        return -1;
    }

    // raw images don't include anything than raw disk data
    this->data_size = this->img_size;

    // guess disk format from image file size
    static struct {
        int capacity;
        int rec_method;
        int num_tracks;
        int num_sectors;
        int num_sides;
        int density;
    } size_to_params[] = {
        { 409600, 0, 80,  800, 1, 0}, //  400K GCR
        { 819200, 0, 80,  800, 2, 0}, //  800K GCR
        { 737280, 1, 80, 1440, 2, 0}, //  720K MFM
        {1474560, 1, 80, 2880, 2, 1}, // 1440K MFM
    };

    this->rec_method = -1;

    for (int i = 0; i < 4; i++) {
        if (this->img_size == size_to_params[i].capacity) {
            this->rec_method  = size_to_params[i].rec_method;
            this->num_tracks  = size_to_params[i].num_tracks;
            this->num_sectors = size_to_params[i].num_sectors;
            this->num_sides   = size_to_params[i].num_sides;
            this->density     = size_to_params[i].density;
        }
    }

    if (this->rec_method == -1) {
        LOG_F(ERROR, "RawFloppyImg: could't determine disk format from image size!");
        return -1;
    }

    return 0;
}

/** Retrieve raw disk data. */
int RawFloppyImg::get_raw_disk_data(char* buf)
{
    std::ifstream img_file;

    img_file.open(img_path, std::ios::in | std::ios::binary);
    if (img_file.fail()) {
        img_file.close();
        LOG_F(ERROR, "RawFloppyImg: Could not open specified floppy image!");
        return -1;
    }

    img_file.seekg(0, img_file.beg);
    img_file.read(buf, this->data_size);
    img_file.close();

    return 0;
}

/** Convert low-level disk data to high-level image data. */
int RawFloppyImg::export_data()
{
    return 0;
}

FloppyImgConverter* open_floppy_image(std::string& img_path)
{
    FloppyImgConverter *fconv;

    std::ifstream img_file;

    img_file.open(img_path, std::ios::in | std::ios::binary);
    if (img_file.fail()) {
        img_file.close();
        LOG_F(ERROR, "Could not open specified floppy image!");
        return nullptr;
    }

    FlopImgType itype = identify_image(img_file);

    img_file.close();

    switch(itype) {
    case FlopImgType::RAW:
        LOG_F(INFO, "Raw floppy image");
        fconv = new RawFloppyImg(img_path);
        break;
    case FlopImgType::DC42:
        LOG_F(INFO, "Disk Copy 4.2 image");
        break;
    case FlopImgType::WOZ1:
    case FlopImgType::WOZ2:
        LOG_F(INFO, "WOZ v%s image\n", (itype == FlopImgType::WOZ2) ? "2" : "1");
        break;
    default:
        LOG_F(ERROR, "Unknown/unsupported image format!");
        return nullptr;
    }

    if (fconv->calc_phys_params()) {
        delete fconv;
        return nullptr;
    }

    return fconv;
}
