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

/** @file Sander-Wozniak Machine 3 (SWIM3) definitions. */

#ifndef SWIM3_H
#define SWIM3_H

#include <devices/common/hwcomponent.h>
#include <devices/floppy/superdrive.h>

#include <cinttypes>
#include <memory>

class DmaBidirChannel;
class InterruptCtrl;

/** SWIM3 registers offsets. */
namespace Swim3 {

enum Swim3Reg : uint8_t {
    Data            = 0,
    Timer           = 1,
    Error           = 2,
    Param_Data      = 3,
    Phase           = 4,
    Setup           = 5,
    Status_Mode0    = 6,  // read: Status, write: zeroes to the mode register
    Handshake_Mode1 = 7,  // read: Handshake, write: ones to the mode register
    Interrupt_Flags = 8,
    Step            = 9,
    Current_Track   = 10,
    Current_Sector  = 11,
    Gap_Format      = 12,
    First_Sector    = 13,
    Sectors_To_Xfer = 14,
    Interrupt_Mask  = 15
};

/** Mode register bits. */
enum {
    SWIM3_INT_ENA     = 0x01,
    SWIM3_DRIVE_1     = 0x02,
    SWIM3_DRIVE_2     = 0x04,
    SWIM3_GO          = 0x08,
    SWIM3_WR_MODE     = 0x10,
    SWIM3_HEAD_SELECT = 0x20,
    SWIM3_FORMAT_MODE = 0x40,
    SWIM3_GO_STEP     = 0x80,
};

/** Interrupt flags. */
enum {
    INT_TIMER_DONE = 0x01,
    INT_STEP_DONE  = 0x02,
    INT_ID_READ    = 0x04,
    INT_SECT_DONE  = 0x08,
    INT_SENSE      = 0x10, // unused by SWIM3 driver
};

// SWIM3 internal states.
enum {
    SWIM3_IDLE,
    SWIM3_ADDR_MARK_SEARCH,
    SWIM3_DATA_XFER,
};

class Swim3Ctrl : public HWComponent {
public:
    Swim3Ctrl();
    ~Swim3Ctrl() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<Swim3Ctrl>(new Swim3Ctrl());
    }

    int device_postinit();

    // SWIM3 registers access
    uint8_t read(uint8_t reg_offset);
    void    write(uint8_t reg_offset, uint8_t value);

    void set_dma_channel(DmaBidirChannel *dma_ch) {
        this->dma_ch = dma_ch;
    };

protected:
    void    reset();
    void    update_irq();
    void    start_stepping();
    void    do_step();
    void    stop_stepping();
    void    start_disk_access();
    void    disk_access();
    void    stop_disk_access();
    void    init_timer(const uint8_t start_val);
    uint8_t calc_timer_val();

private:
    std::unique_ptr<MacSuperdrive::MacSuperDrive> int_drive;

    DmaBidirChannel*    dma_ch;

    uint8_t timer_val; // internal timer that decrements at a 1 us rate
    uint8_t setup_reg;
    uint8_t mode_reg;
    uint8_t error;
    uint8_t phase_lines;
    uint8_t int_reg;
    uint8_t int_flags;  // interrupt flags
    uint8_t int_mask;
    uint8_t pram;       // parameter RAM: two nibbles = {late_time, early_time}
    uint8_t step_count;
    uint8_t cur_track;
    uint8_t cur_sector;
    uint8_t target_sect;
    uint8_t format;     // format byte from the last GCR/MFM address field
    uint8_t first_sec;
    uint8_t xfer_cnt;
    uint8_t gap_size;
    uint8_t rd_line;
    int     cur_state;

    int     one_us_timer_id = 0;
    int     step_timer_id   = 0;
    int     access_timer_id = 0;

    uint64_t    one_us_timer_start = 0;

    // Interrupt related stuff
    InterruptCtrl* int_ctrl = nullptr;
    uint32_t       irq_id   = 0;
    uint8_t        irq      = 0;
};

}; // namespace Swim3

#endif // SWIM3_H
