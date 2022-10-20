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

#include <devices/floppy/floppyimg.h>
#include <machines/machineproperties.h>
#include <loguru.hpp>
#include <memaccess.h>

#include <cinttypes>
#include <cstring>
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

//======================= RAW IMAGE CONVERTER ============================
RawFloppyImg::RawFloppyImg(std::string& file_path) : FloppyImgConverter()
{
    this->img_path = file_path;
}

/**
  For raw images, we'll attempt to guess disk format based on image size.
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

    // raw images don't include anything other than raw disk data
    this->data_size = this->img_size;

    // see if user has specified disk format manually
    std::string fmt = GET_STR_PROP("fdd_fmt");
    if (!fmt.empty()) {
        if (fmt == "GCR_400K") {
            this->img_size = 409600;
        } else if (fmt == "GCR_800K") {
            this->img_size = 819200;
        } else if (fmt == "MFM_720K") {
            this->img_size = 737280;
        } else if (fmt == "MFM_1440K") {
            this->img_size = 1474560;
        } else {
            LOG_F(WARNING, "Invalid floppy disk format %s", fmt.c_str());
        }
    }

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

            // fake format byte for GCR disks
            if (!this->rec_method) {
                this->format_byte = (this->num_sides == 2) ? 0x22 : 0x2;
            } else {
                // For MFM disks this byte indicates sector size in blocks
                this->format_byte = 2;
            }
            break;
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
        LOG_F(INFO, "WOZ v%s image", (itype == FlopImgType::WOZ2) ? "2" : "1");
        break;
    default:
        LOG_F(WARNING, "Unknown image format - assume RAW");
        fconv = new RawFloppyImg(img_path);
    }

    if (fconv->calc_phys_params()) {
        delete fconv;
        return nullptr;
    }

    return fconv;
}
