/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-24 divingkatae and maximum
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
#include <devices/common/scsi/scsibusctrl.h>

#include <cinttypes>
#include <memory>

// Chip ID returned by the MESH ASIC on TNT machines (Apple part 343S1146-a)
#define TntMeshID       0xE2

// Chip ID returned by the MESH cell inside the Heathrow ASIC
#define HeathrowMeshID  4

namespace MeshScsi {
    /** MESH registers offsets. */
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

    /** MESH Sequencer commands. */
    enum SeqCmd : uint8_t {
        NoOperation     = 0x0,
        Arbitrate       = 0x1,
        Select          = 0x2,
        Command         = 0x3,
        Status          = 0x4,
        DataOut         = 0x5,
        DataIn          = 0x6,
        MessageOut      = 0x7,
        MessageIn       = 0x8,
        BusFree         = 0x9,
        EnaParityCheck  = 0xA,
        DisParityCheck  = 0xB,
        EnaReselect     = 0xC,
        DisReselect     = 0xD,
        ResetMesh       = 0xE,
        FlushFIFO       = 0xF
    };

    /** Exception register bits. */
    enum {
        EXC_SEL_TIMEOUT = 1 << 0,
        EXC_PHASE_MM    = 1 << 1,
        EXC_ARB_LOST    = 1 << 2,
    };

    /** Interrupt register bits. */
    enum {
        INT_CMD_DONE    = 1 << 0,
        INT_EXCEPTION   = 1 << 1,
        INT_ERROR       = 1 << 2,
        INT_MASK        = INT_CMD_DONE | INT_EXCEPTION | INT_ERROR
    };
}; // namespace MeshScsi

class MeshBase {
public:
    MeshBase()  = default;
    ~MeshBase() = default;
    virtual uint8_t read(uint8_t reg_offset) = 0;
    virtual void    write(uint8_t reg_offset, uint8_t value) = 0;
};

class MeshStub : public MeshBase {
public:
    MeshStub()  = default;
    ~MeshStub() = default;

    // registers access
    uint8_t read(uint8_t reg_offset) override { return 0; };
    void   write(uint8_t reg_offset, uint8_t value) override {};
};

class MeshController : public ScsiBusController, public MeshBase {
public:
    MeshController(uint8_t mesh_id) : ScsiBusController("MESH", 7) {
        this->chip_id = mesh_id;
        this->reset(true);
    };
    ~MeshController() = default;

    static std::unique_ptr<HWComponent> create_for_tnt() {
        return std::unique_ptr<MeshController>(new MeshController(TntMeshID));
    }

    static std::unique_ptr<HWComponent> create_for_heathrow() {
        return std::unique_ptr<MeshController>(new MeshController(HeathrowMeshID));
    }

    // MeshBase methods
    uint8_t read(uint8_t reg_offset) override;
    void   write(uint8_t reg_offset, uint8_t value) override;

    // HWComponent methods
    int device_postinit() override;

    // ScsiBusController methods
    void step_completed() override;
    void report_error(const int error) override;

protected:
    void    reset(bool is_hard_reset);
    void    perform_command(const uint8_t cmd);
    void    update_bus_status(const uint16_t new_stat);

private:
    uint8_t     chip_id;
    uint8_t     sync_params;
    uint8_t     cur_cmd;
    uint8_t     error;
    uint8_t     exception  = 0;
    uint16_t    bus_stat;
    bool        check_parity = true;
};

#endif // MESH_H
