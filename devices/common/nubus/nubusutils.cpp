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

#include <devices/common/hwcomponent.h>
#include <devices/common/nubus/nubusutils.h>
#include <devices/memctrl/memctrlbase.h>
#include <machines/machinebase.h>
#include <loguru.hpp>
#include <memaccess.h>

#include <cinttypes>
#include <fstream>
#include <sstream>

uint32_t calculate_rom_crc(uint8_t *data_ptr, int length)
{
    uint32_t sum = 0;

    for (int i = 0; i < length; i++) {
        // rotate sum left by one bit
        if (sum & 0x80000000UL)
            sum = (sum << 1) | 1;
        else
            sum = (sum << 1) | 0;
        sum += data_ptr[i];
    }

    return sum;
}

int load_declaration_rom(const std::string rom_path, int slot_num)
{
    std::ifstream rom_file;

    int result = 0;

    try {
        rom_file.open(rom_path, std::ios::in | std::ios::binary);
        if (rom_file.fail())
            throw std::runtime_error("could not open declaration ROM image " + rom_path);

        // determine image size
        rom_file.seekg(0, std::ios::end);
        uint64_t rom_size = rom_file.tellg();

        if (rom_size > 16 * 1024 * 1024)
            throw std::runtime_error("declaration ROM image " + rom_path + " size " +
                                     std::to_string(rom_size) + " is too large");

        // load it
        auto rom_data = std::unique_ptr<uint8_t[]> (new uint8_t[rom_size]);
        rom_file.seekg(0, std::ios::beg);
        rom_file.read((char *)rom_data.get(), rom_size);

        // verify image data
        uint8_t byte_lane = rom_data[rom_size - 1];
        if ((byte_lane & 0xF) != ((byte_lane >> 4) ^ 0xF))
            throw std::runtime_error("declaration ROM image " + rom_path +
                                     " has invalid byte lane value");

        if (READ_DWORD_BE_A(&rom_data[rom_size - 6]) != 0x5A932BC7UL)
            throw std::runtime_error("declaration ROM image " + rom_path +
                                     " has invalid test pattern");

        if (rom_data[rom_size - 7] != 1)
            throw std::runtime_error("declaration ROM image " + rom_path +
                                     " has unsupported format");

        uint32_t crc   = READ_DWORD_BE_A(&rom_data[rom_size - 12]);
        int hdr_length = READ_DWORD_BE_A(&rom_data[rom_size - 16]);

        // patch the CRC field of the format header to 0
        WRITE_DWORD_BE_A(&rom_data[rom_size - 12], 0);

        uint32_t test_crc = calculate_rom_crc(rom_data.get(), hdr_length);

        // restore the CRC field value
        WRITE_DWORD_BE_A(&rom_data[rom_size - 12], crc);

        if (test_crc != crc) {
            std::stringstream err;
            err << "declaration ROM image " + rom_path + " has invalid CRC, expected = 0x"
                << std::hex << crc << ", got = 0x" << std::hex << test_crc;
            throw std::runtime_error(err.str());
        }

        int lanes_used = 0;

        // count meaningful lanes in a data quad
        for (int i = 0; i < 4; i++) {
            if (byte_lane & (1 << i))
                lanes_used++;
        }

        // padded_len is the size in bytes of all pages needed to contain all the bytes of the ROM
        uint32_t padded_len =
            uint32_t(((rom_size * 4 + lanes_used - 1) / lanes_used + 4095) / 4096 * 4096);

        if (padded_len > 16 * 1024 * 1024)
            throw std::runtime_error("declaration ROM image " + rom_path + " size " +
                                     std::to_string(rom_size) + " is too large");

        // calculate starting physical address of the ROM
        uint32_t rom_phys_start = (0xF0FFFFFFUL | ((slot_num & 0xF) << 24)) - padded_len + 1;

        auto mem_crtl = dynamic_cast<MemCtrlBase*>(gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));
        if (!mem_crtl->add_rom_region(rom_phys_start, padded_len))
            throw std::runtime_error("could not allocate ROM space for declaration ROM image " +
                                     rom_path);

        int data_pos = int(rom_size) - 1;

        auto new_data = std::unique_ptr<uint8_t[]> (new uint8_t[padded_len]);

        // prepare new ROM data by copying over used lanes
        // and padding unused lanes with 0xFF
        // NOTE: speed uncritical - don't optimize!
        for (int i = padded_len - 1; i >= 0; ) {
            for (int b = 8; i>= 0 && b != 0; b >>= 1) {
                if (data_pos >= 0 && (byte_lane & b))
                    new_data[i--] = rom_data[data_pos--];
                else
                    new_data[i--] = 0xFF;
            }
        }

        // move padded ROM data to the memory region
        mem_crtl->set_data(rom_phys_start, new_data.get(), padded_len);
    }
    catch (const std::exception& exc) {
        LOG_F(ERROR, "load_declaration_rom failed: %s", exc.what());
        result = -1;
    }

    rom_file.close();

    return result;
}
