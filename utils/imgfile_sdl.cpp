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

#include <utils/imgfile.h>

#include <fstream>

class ImgFile::Impl {
public:
    std::fstream stream;
};

ImgFile::ImgFile(): impl(std::make_unique<Impl>())
{

}

ImgFile::~ImgFile() = default;

bool ImgFile::open(const std::string &img_path)
{
    impl->stream.open(img_path, std::ios::in | std::ios::out | std::ios::binary);
    return !impl->stream.fail();
}

void ImgFile::close()
{
    impl->stream.close();
}

uint64_t ImgFile::size() const
{
    impl->stream.seekg(0, impl->stream.end);
    return impl->stream.tellg();
}

uint64_t ImgFile::read(void* buf, uint64_t offset, uint64_t length) const
{
    impl->stream.seekg(offset, std::ios::beg);
    impl->stream.read((char *)buf, length);
    return impl->stream.gcount();
}

uint64_t ImgFile::write(const void* buf, uint64_t offset, uint64_t length)
{
    impl->stream.seekg(offset, std::ios::beg);
    impl->stream.write((const char *)buf, length);
    return impl->stream.gcount();
}
