/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 divingkatae and maximum
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
#include <loguru.hpp>

#include <fstream>
#include <sstream>
#include <memory>

extern bool is_deterministic;

class ImgFile::Impl {
public:
    std::unique_ptr<std::iostream> stream;
};

ImgFile::ImgFile(): impl(std::make_unique<Impl>())
{

}

ImgFile::~ImgFile() = default;

bool ImgFile::open(const std::string &img_path)
{
    if (is_deterministic) {
        // Avoid writes to the underlying file by reading it all in memory and
        // only operating on that.
        auto mem_stream = std::make_unique<std::stringstream>();
        std::ifstream temp(img_path, std::ios::in | std::ios::binary);
        if (!temp) return false;
        *mem_stream << temp.rdbuf();
        impl->stream = std::move(mem_stream);
    } else {
        auto file_stream = std::make_unique<std::fstream>(img_path, std::ios::in | std::ios::out | std::ios::binary);
        if (!file_stream->is_open()) return false;
        impl->stream = std::move(file_stream);
    }
    return impl->stream && impl->stream->good();
}

void ImgFile::close()
{
    if (!impl->stream) {
        LOG_F(WARNING, "ImgFile::close before disk was opened, ignoring.");
        return;
    }
    impl->stream.reset();
}

uint64_t ImgFile::size() const
{
    if (!impl->stream) {
        LOG_F(WARNING, "ImgFile::size before disk was opened, ignoring.");
        return 0;
    }
    impl->stream->seekg(0, impl->stream->end);
    return impl->stream->tellg();
}

uint64_t ImgFile::read(void* buf, uint64_t offset, uint64_t length) const
{
    if (!impl->stream) {
        LOG_F(WARNING, "ImgFile::read before disk was opened, ignoring.");
        return 0;
    }
    impl->stream->seekg(offset, std::ios::beg);
    impl->stream->read((char *)buf, length);
    return impl->stream->gcount();
}

uint64_t ImgFile::write(const void* buf, uint64_t offset, uint64_t length)
{
    if (!impl->stream) {
        LOG_F(WARNING, "ImgFile::write before disk was opened, ignoring.");
        return 0;
    }
    impl->stream->seekp(offset, std::ios::beg);
    impl->stream->write((const char *)buf, length);
    #if defined(WIN32) || defined(__APPLE__) || defined(__linux)
        impl->stream->flush();
    #endif
    return uint64_t(impl->stream->tellp()) - offset;
}
