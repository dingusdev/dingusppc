#ifndef MMIO_DEVICE_H
#define MMIO_DEVICE_H

#include <cinttypes>
#include <string>

/** Abstract class representing a simple, memory-mapped I/O device */
class MMIODevice {
public:
    virtual uint32_t read(uint32_t offset, int size) = 0;
    virtual void     write(uint32_t offset, uint32_t value, int size) = 0;
    virtual ~MMIODevice() = default;

protected:
    std::string name;
};

#endif /* MMIO_DEVICE_H */
