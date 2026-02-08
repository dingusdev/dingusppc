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

#ifndef SCSI_BASE_DEVICE_H
#define SCSI_BASE_DEVICE_H

#include <core/bitops.h>
#include <devices/common/scsi/scsi.h>
#include <devices/common/scsi/scsiphysinterface.h>

#include <array>
#include <cinttypes>
#include <cstring>

/** IDs for command groups with non-standard length. */
enum : int {
    CMD_GRP_RESERVED        = -1,
    CMD_GRP_VENDOR_SPECIFIC = -2,
};

/** Base class for logical SCSI devices. */
class ScsiCommonCmds {
public:
    ScsiCommonCmds();
    ~ScsiCommonCmds() = default;

    void set_phys_dev(ScsiPhysInterface* dev_obj) { this->phy_impl = dev_obj; }
    void set_cdb_ptr(uint8_t* ptr) { this->cdb_ptr = ptr; }
    void set_buf_ptr(uint8_t* ptr) { this->buf_ptr = ptr; }

    virtual int verify_cdb();

    virtual int get_cdb_length(uint8_t op_code);

    // override this to support several LUNs
    virtual bool lun_supported(uint8_t lun) { return lun == 0; }

    // override this to support linked commands
    virtual bool linked_cmds_supported() { return false; }

protected:
    virtual void     process_command();
    virtual int      inquiry_new();
    virtual int      request_sense_new();
    virtual int      test_unit_ready();
    virtual uint8_t  get_lun();
    virtual uint32_t get_xfer_len();
    virtual void     reset_sense();
    virtual void     invalid_lun();
    virtual void     invalid_cdb();
    virtual void     invalid_command();
    virtual void     illegal_request(uint8_t asc, uint8_t ascq, bool is_cdb = true);

    virtual void set_field_pointer(const uint16_t fp) {
        this->field_ptr = fp;
        this->field_ptr_valid = true;
    }

    virtual void set_bit_pointer(const uint8_t bp) {
        this->bit_ptr = bp;
        this->bit_ptr_valid = true;
    }

    bool cmd_enabled(uint8_t cmd) {
        return bit_set(this->cmd_enables[cmd >> 3], cmd & 7);
    }

    void enable_cmd(uint8_t cmd) {
        set_bit(this->cmd_enables[cmd >> 3], cmd & 7);
    }

    void disable_cmd(uint8_t cmd) {
        clear_bit(this->cmd_enables[cmd >> 3], cmd & 7);
    }

    void set_vendor_id(char* vid) {
        std::memset(this->vendor_id, 0, sizeof(this->vendor_id));
        std::strncpy(this->vendor_id, vid, sizeof(this->vendor_id));
    }

    void set_product_id(char* pid) {
        std::memset(this->product_id, 0, sizeof(this->product_id));
        std::strncpy(this->product_id, pid, sizeof(this->product_id));
    }

    void set_revision_id(char* rid) {
        std::memset(this->revision_id, 0, sizeof(this->revision_id));
        std::strncpy(this->revision_id, rid, sizeof(this->revision_id));
    }

    ScsiPhysInterface*  phy_impl = nullptr;
    uint8_t*            cdb_ptr  = nullptr;
    uint8_t*            buf_ptr  = nullptr;

    std::array<uint8_t, 16> cmd_enables{};

    // REQUEST SENSE state
    uint8_t     sense_key       = ScsiSense::NO_SENSE;
    uint8_t     asc             = 0; // additional sense code
    uint8_t     ascq            = 0; // additional sense code qualifier
    bool        field_ptr_valid = false; // field pointer valid
    bool        bit_ptr_valid   = false; // bit pointer valid
    bool        is_cdb_err      = false; // error in CDB?
    uint8_t     bit_ptr         = 0; // field pointer value
    uint16_t    field_ptr       = 0; // bit pointer value

    // INQUIRY state
    uint8_t dev_type        = ScsiDevType::UNKNOWN;
    bool    is_removable    = false;
    uint8_t dev_type_mod    = 0;
    uint8_t std_versions    = 0; // compliance with some standards
    uint8_t resp_fmt        = 0;
    uint8_t cap_flags       = 0; // capability flags
    char    vendor_id[8]    = {};
    char    product_id[16]  = {};
    char    revision_id[4]  = {};

    uint8_t link_ctrl = 0; // control field from CDB
};

#endif // SCSI_BASE_DEVICE_H
