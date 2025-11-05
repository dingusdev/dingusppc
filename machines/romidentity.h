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

/** @file Rom identity maps rom info to machine name and description.
 */

#ifndef ROM_IDENTITY_H
#define ROM_IDENTITY_H

#include <cinttypes>

typedef struct {
    uint32_t    firmware_version;
    uint32_t    firmware_size_k;
    uint32_t    ow_expected_checksum;
    uint64_t    ow_expected_checksum64;
    uint32_t    nw_product_id;
    uint32_t    nw_subconfig_expected_checksum; // checksum of the system config section but without the firmware version and date
    const char *id_str;                         // BootstrapVersion of NKConfigurationInfo
    const char *nw_firmware_updater_name;
    const char *nw_openfirmware_name;
    const char *dppc_machine;
    const char *dppc_description;
    const char *rom_description;
} rom_info;

extern rom_info rom_identity[];

#endif /* ROM_IDENTITY_H */
