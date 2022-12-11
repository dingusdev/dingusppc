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
    virtual ~FloppyImgConverter() = default;

    virtual int calc_phys_params(void) = 0;
    virtual int get_raw_disk_data(char* buf) = 0;
    virtual int export_data(void) = 0;

    int get_data_size()        { return this->data_size;   };
    int get_disk_rec_method()  { return this->rec_method;  };
    int get_number_of_tracks() { return this->num_tracks;  };
    int get_number_of_sides()  { return this->num_sides;   };
    int get_sectors_per_side() { return this->num_sectors; };
    int get_rec_density()      { return this->density;     };
    uint8_t get_format_byte()  { return this->format_byte; };

protected:
    std::string img_path;
    int         img_size;
    int         data_size; // disk data size without image format specific stuff

    int         rec_method;
    int         num_tracks;
    int         num_sides;
    int         num_sectors;
    int         density;
    uint8_t     format_byte; // GCR format byte from sector header
};

/** Converter for raw floppy images. */
class RawFloppyImg : public FloppyImgConverter {
public:
    RawFloppyImg(std::string& file_path);
    ~RawFloppyImg() = default;

    int calc_phys_params(void);
    int get_raw_disk_data(char* buf);
    int export_data(void);
};

/** Converter for Disk Copy 4.2 images. */
class DiskCopy42Img : public FloppyImgConverter {
public:
    DiskCopy42Img(std::string& file_path);
    ~DiskCopy42Img() = default;

    int calc_phys_params(void);
    int get_raw_disk_data(char* buf);
    int export_data(void);
};

extern FloppyImgConverter* open_floppy_image(std::string& img_path);

#endif // FLOPPY_IMG_H
