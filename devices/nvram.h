#ifndef NVRAM_H
#define NVRAM_H

#include <string>
#include <cinttypes>

/** @file Non-volatile RAM emulation.

    It implements a non-volatile random access storage whose content will be
    automatically saved to and restored from the dedicated file.
 */

class NVram
{
public:
    NVram(std::string file_name = "nvram.bin", uint32_t ram_size = 8192);
    ~NVram();

    uint8_t read_byte(uint32_t offset);
    void    write_byte(uint32_t offset, uint8_t value);

private:
    std::string file_name; /* file name for the backing file. */
    uint16_t    ram_size;  /* NVRAM size. */
    uint8_t*    storage;

    void    init();
    void    save();
};

#endif /* NVRAM_H */
