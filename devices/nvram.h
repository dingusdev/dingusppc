#ifndef NVRAM_H
#define NVRAM_H

#include <iostream>
#include <fstream>
#include <cinttypes>

class NVram
{
public:
    NVram();
    ~NVram();

    uint8_t read(int value);
    void write(int reg, uint8_t value);

private:
    std::fstream nvram_file; //NVRAM file for storing and reading values
    std::ofstream nvram_savefile; //Save file when closing out DingusPPC

    /* NVRAM state. */
    uint32_t nvram_offset;
    uint32_t nvram_filesize;
    uint8_t nvram_value;
    uint8_t nvram_storage[8192];

    void nvram_init();
    void nvram_save();
    uint8_t nvram_read(uint32_t nvram_offset);
    void nvram_write(uint32_t nvram_offset, uint8_t write_val);
};

#endif /* NVRAM_H */
