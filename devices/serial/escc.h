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

/** @file Enhanced Serial Communications Controller (ESCC) definitions. */

#ifndef ESCC_H
#define ESCC_H

#include <cinttypes>
#include <memory>
#include <string>

/** ESCC register addresses */
enum EsccReg : uint8_t {
    Port_B_Cmd      = 0,
    Port_B_Data     = 1,
    Port_A_Cmd      = 2,
    Port_A_Data     = 3,
    Enh_Reg_B       = 4,
    Enh_Reg_A       = 5,
};

/** LocalTalk LTPC registers */
enum LocalTalkReg : uint8_t {
    Rec_Count   = 8,
    Start_A     = 9,
    Start_B     = 0xA,
    Detect_AB   = 0xB,
};

enum WR0Cmd : uint8_t {
    Point_High = 1,
};

/** ESCC Channel class. */
class EsccChannel {
public:
    EsccChannel(std::string name) { this->name = name; };
    ~EsccChannel() = default;

    void write_reg(int reg_num, uint8_t value);

private:
    std::string     name;
    uint8_t         write_regs[16];
};

/** ESCC Controller class. */
class EsccController {
public:
    EsccController();
    ~EsccController() = default;

    // ESCC registers access
    uint8_t read(uint8_t reg_offset);
    void    write(uint8_t reg_offset, uint8_t value);

private:
    void write_internal(EsccChannel* ch, uint8_t value);

    std::unique_ptr<EsccChannel>    ch_a;
    std::unique_ptr<EsccChannel>    ch_b;

    int reg_ptr; // register pointer for reading/writing (same for both channels)
};

#endif // ESCC_H
