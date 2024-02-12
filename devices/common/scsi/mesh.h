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

/** @file MESH (Macintosh Enhanced SCSI Hardware) controller definitions. */

#ifndef MESH_H
#define MESH_H

#include <devices/common/dmacore.h>
#include <devices/common/hwcomponent.h>

#include <cinttypes>
#include <memory>

class InterruptCtrl;
class ScsiBus;

// Chip ID returned by the MESH ASIC on TNT machines (Apple part 343S1146-a)
#define TntMeshID       0xE2

// Chip ID returned by the MESH cell inside the Heathrow ASIC
#define HeathrowMESHID  4

namespace MeshScsi {

// MESH registers offsets.
enum MeshReg : uint8_t {
    XferCount0 = 0,
    XferCount1 = 1,
    FIFO       = 2,
    Sequence   = 3,
    BusStatus0 = 4,
    BusStatus1 = 5,
    FIFOCount  = 6,
    Exception  = 7,
    Error      = 8,
    IntMask    = 9,
    Interrupt  = 0xA,
    SourceID   = 0xB,
    DestID     = 0xC,
    SyncParms  = 0xD,
    MeshID     = 0xE,
    SelTimeOut = 0xF
};

// MESH Sequencer commands.
enum SeqCmd : uint8_t {
    NoOperation = 0,
    Arbitrate   = 1,
    Select      = 2,
    BusFree     = 9,
    EnaReselect = 0xC,
    DisReselect = 0xD,
    ResetMesh   = 0xE,
    FlushFIFO   = 0xF,
};

// Exception register bits.
enum {
    EXC_SEL_TIMEOUT = 1 << 0,
    EXC_PHASE_MM    = 1 << 1,
    EXC_ARB_LOST    = 1 << 2,
};

// Interrupt register bits.
enum {
    INT_CMD_DONE    = 1 << 0,
    INT_EXCEPTION   = 1 << 1,
    INT_ERROR       = 1 << 2,
    INT_MASK        = INT_CMD_DONE | INT_EXCEPTION | INT_ERROR
};


enum SeqState : uint32_t {
    IDLE = 0,
    BUS_FREE,
    ARB_BEGIN,
    ARB_END,
    SEL_BEGIN,
    SEL_END,
    SEND_MSG,
    SEND_CMD,
    CMD_COMPLETE,
    XFER_BEGIN,
    XFER_END,
    SEND_DATA,
    RCV_DATA,
    RCV_STATUS,
    RCV_MESSAGE,
};

}; // namespace MeshScsi

class MeshStub : public HWComponent {
public:
    MeshStub()  = default;
    ~MeshStub() = default;

    // registers access
    uint8_t read(uint8_t reg_offset) { return 0; };
    void   write(uint8_t reg_offset, uint8_t value) {};
};

class MeshController : public HWComponent {
public:
    MeshController(uint8_t mesh_id) {
        supports_types(HWCompType::SCSI_HOST | HWCompType::SCSI_DEV);
        this->chip_id = mesh_id;
        this->set_name("MESH");
        this->reset(true);
    };
    ~MeshController() = default;

    static std::unique_ptr<HWComponent> create_for_tnt() {
        return std::unique_ptr<MeshController>(new MeshController(TntMeshID));
    }

    static std::unique_ptr<HWComponent> create_for_heathrow() {
        return std::unique_ptr<MeshController>(new MeshController(HeathrowMESHID));
    }

    // MESH registers access
    uint8_t read(uint8_t reg_offset);
    void   write(uint8_t reg_offset, uint8_t value);

    // HWComponent methods
    int device_postinit();

    void set_dma_channel(DmaBidirChannel *dma_ch) {
        this->dma_ch = dma_ch;
    };

protected:
    void    reset(bool is_hard_reset);
    void    perform_command(const uint8_t cmd);
    void    seq_defer_state(uint64_t delay_ns);
    void    sequencer();
    void    update_irq();

private:
    uint8_t     chip_id;
    uint8_t     int_mask;
    uint8_t     int_stat = 0;
    uint8_t     sync_params;
    uint8_t     src_id;
    uint8_t     dst_id;
    uint8_t     cur_cmd;
    uint8_t     error;
    uint8_t     fifo_cnt;
    uint8_t     exception;
    uint32_t    xfer_count;

    ScsiBus*    bus_obj;
    uint16_t    bus_stat;

    // Sequencer state
    uint32_t    seq_timer_id;
    uint32_t    cur_state;
    uint32_t    next_state;

    // interrupt related stuff
    InterruptCtrl* int_ctrl = nullptr;
    uint32_t       irq_id   = 0;
    uint8_t        irq      = 0;

    // DMA related stuff
    DmaBidirChannel*    dma_ch;
};

#endif // MESH_H
