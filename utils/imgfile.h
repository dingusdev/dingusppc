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

/** @file Image file abstraction for floppy, hard drive and CD-ROM images
 * (implemented on each platform). */

#ifndef IMGFILE_H
#define IMGFILE_H

#include <cstddef>
#include <memory>
#include <string>

class ImgFile {
public:
    ImgFile();
    ~ImgFile();

    bool open(const std::string& img_path);
    void close();

    size_t size() const;

    size_t read(void* buf, off_t offset, size_t length) const;
    size_t write(const void* buf, off_t offset, size_t length);
private:
    class Impl; // Holds private fields
    std::unique_ptr<Impl> impl;
};

#endif // IMGFILE_H
