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

/** @file Definitions for working with various floppy images. */

#ifndef FLOPPY_IMG_H
#define FLOPPY_IMG_H

#include <cinttypes>
#include <string>

#define BLOCK_SIZE 512 // size in bytes of a logical block

#define MFM_HD_SIZE (BLOCK_SIZE*2880) // maximal size of a high-density floppy

/** Floppy image types. */
enum class FlopImgType {
    RAW,        // raw image without metadata or low-level formatting
    DC42,       // Disk Copy 4.2 image
    WOZ1,       // WOZ v1 image with metadata and low-level data retained
    WOZ2,       // WOZ v2 image with metadata and low-level data retained
    UNKNOWN,    // invalid/unknown image format
};

/** Interface for floppy image converters. */
class FloppyImgConverter {
public:
    FloppyImgConverter()  = default;
    ~FloppyImgConverter() = default;

    virtual int validate(void) = 0;
    virtual int get_phys_params(void) = 0;
    virtual int import_data(void) = 0;
    virtual int export_data(void) = 0;
};

/** Converter for raw floppy images. */
class RawFloppyImg : public FloppyImgConverter {
public:
    RawFloppyImg(std::string& file_path);
    ~RawFloppyImg() = default;

    int validate(void);
    int get_phys_params(void);
    int import_data(void);
    int export_data(void);

private:
    std::string img_path;
    int         img_size;
};

extern int open_floppy_image(std::string& img_path);

#endif // FLOPPY_IMG_H
