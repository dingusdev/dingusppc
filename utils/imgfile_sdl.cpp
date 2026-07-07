/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

(You may also contact divingkxt or powermax2286 on Discord)

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

#include <algorithm>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>

#if !defined(_WIN32) && \
    (defined(__APPLE__) || defined(__linux__) || defined(__unix__))
#define DPPC_HAS_PRIVATE_MMAP 1
#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

extern bool is_deterministic;

class ImgFileBackend {
public:
    virtual ~ImgFileBackend() = default;

    virtual bool open(const std::string &img_path) = 0;
    virtual void close() = 0;
    virtual uint64_t size() const = 0;
    virtual uint64_t read(void *buf, uint64_t offset, uint64_t length) const = 0;
    virtual uint64_t write(const void *buf, uint64_t offset, uint64_t length) = 0;
};

static std::unique_ptr<ImgFileBackend> make_imgfile_backend(const std::string &img_path);

class ImgFile::Impl {
public:
    std::unique_ptr<ImgFileBackend> backend;
};

ImgFile::ImgFile(): impl(std::make_unique<Impl>())
{

}

ImgFile::~ImgFile() = default;

bool ImgFile::open(const std::string &img_path)
{
    impl->backend = make_imgfile_backend(img_path);
    return !!impl->backend;
}

void ImgFile::close()
{
    if (!impl->backend) {
        LOG_F(WARNING, "ImgFile::close before disk was opened, ignoring.");
        return;
    }
    impl->backend->close();
    impl->backend.reset();
}

uint64_t ImgFile::size() const
{
    if (!impl->backend) {
        LOG_F(WARNING, "ImgFile::size before disk was opened, ignoring.");
        return 0;
    }
    return impl->backend->size();
}

uint64_t ImgFile::read(void* buf, uint64_t offset, uint64_t length) const
{
    if (!impl->backend) {
        LOG_F(WARNING, "ImgFile::read before disk was opened, ignoring.");
        return 0;
    }
    return impl->backend->read(buf, offset, length);
}

uint64_t ImgFile::write(const void* buf, uint64_t offset, uint64_t length)
{
    if (!impl->backend) {
        LOG_F(WARNING, "ImgFile::write before disk was opened, ignoring.");
        return 0;
    }
    return impl->backend->write(buf, offset, length);
}

class FileStreamBackend : public ImgFileBackend {
public:
    bool open(const std::string &img_path) override {
        stream = std::make_unique<std::fstream>(
            img_path, std::ios::in | std::ios::out | std::ios::binary);
        if (!stream->is_open())
            return false;

        stream->seekg(0, std::ios::end);
        file_size = stream->tellg();
        stream->clear();
        return stream->good();
    }

    void close() override {
        stream.reset();
    }

    uint64_t size() const override {
        return file_size;
    }

    uint64_t read(void *buf, uint64_t offset, uint64_t length) const override {
        stream->clear();
        stream->seekg(offset, std::ios::beg);
        stream->read(static_cast<char *>(buf), length);
        return stream->gcount();
    }

    uint64_t write(const void *buf, uint64_t offset, uint64_t length) override {
        stream->clear();
        stream->seekp(offset, std::ios::beg);
        stream->write(static_cast<const char *>(buf), length);
#if defined(WIN32) || defined(__APPLE__) || defined(__linux)
        stream->flush();
#endif
        if (!stream->good())
            return 0;

        file_size = std::max(file_size, offset + length);
        return length;
    }

private:
    mutable std::unique_ptr<std::fstream> stream;
    uint64_t file_size = 0;
};

class MemoryStreamBackend : public ImgFileBackend {
public:
    bool open(const std::string &img_path) override {
        std::ifstream file(img_path, std::ios::in | std::ios::binary);
        if (!file)
            return false;

        stream = std::make_unique<std::stringstream>();
        *stream << file.rdbuf();
        stream->seekg(0, std::ios::end);
        file_size = stream->tellg();
        stream->clear();
        return stream->good();
    }

    void close() override {
        stream.reset();
    }

    uint64_t size() const override {
        return file_size;
    }

    uint64_t read(void *buf, uint64_t offset, uint64_t length) const override {
        stream->clear();
        stream->seekg(offset, std::ios::beg);
        stream->read(static_cast<char *>(buf), length);
        return stream->gcount();
    }

    uint64_t write(const void *buf, uint64_t offset, uint64_t length) override {
        stream->clear();
        stream->seekp(offset, std::ios::beg);
        stream->write(static_cast<const char *>(buf), length);
        return stream->good() ? length : 0;
    }

private:
    mutable std::unique_ptr<std::stringstream> stream;
    uint64_t file_size = 0;
};

#if DPPC_HAS_PRIVATE_MMAP
class PrivateMmapBackend : public ImgFileBackend {
public:
    ~PrivateMmapBackend() override {
        close();
    }

    bool open(const std::string &img_path) override {
        fd = ::open(img_path.c_str(), O_RDONLY);
        if (fd < 0)
            return false;

        struct stat st {};
        if (::fstat(fd, &st) < 0 || st.st_size <= 0) {
            close();
            return false;
        }

        file_size = st.st_size;

        void *addr = ::mmap(nullptr, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
        if (addr == MAP_FAILED) {
            LOG_F(WARNING, "Private mmap failed: %s", std::strerror(errno));
            mapping = nullptr;
            close();
            return false;
        }

        mapping = static_cast<uint8_t *>(addr);
        return true;
    }

    void close() override {
        if (mapping) {
            ::munmap(mapping, file_size);
            mapping = nullptr;
        }
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
    }

    uint64_t size() const override {
        return file_size;
    }

    uint64_t read(void *buf, uint64_t offset, uint64_t length) const override {
        std::memcpy(buf, mapping + offset, length);
        return length;
    }

    uint64_t write(const void *buf, uint64_t offset, uint64_t length) override {
        std::memcpy(mapping + offset, buf, length);
        return length;
    }

private:
    int fd = -1;
    uint8_t *mapping = nullptr;
    uint64_t file_size = 0;
};
#endif

static std::unique_ptr<ImgFileBackend> make_imgfile_backend(const std::string &img_path) {
    std::unique_ptr<ImgFileBackend> backend;
    if (is_deterministic) {
#if DPPC_HAS_PRIVATE_MMAP
        backend = std::make_unique<PrivateMmapBackend>();
        if (backend->open(img_path))
            return backend;
        // Intentional fallthrough to MemoryStreamBackend if mmap fails.
#else
        backend = std::make_unique<MemoryStreamBackend>();
#endif
    } else {
        backend = std::make_unique<FileStreamBackend>();
    }

    if (!backend->open(img_path)) {
        return nullptr;
    }

    return backend;
}
