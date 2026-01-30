/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 divingkatae and maximum
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

/** @file Factory for creating different machines.

    Author: Max Poliakovski
 */

#include <devices/common/hwcomponent.h>
#include <devices/deviceregistry.h>
#include <devices/memctrl/memctrlbase.h>
#include <devices/sound/soundserver.h>
#include <loguru.hpp>
#include <machines/machinebase.h>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>
#include <machines/romidentity.h>
#include <memaccess.h>

#include <cinttypes>
#include <cstring>
#include <fstream>
#include <tuple>
#include <iostream>
#include <iomanip>
#include <map>
#include <string>
#include <vector>

using namespace std;

map<string, unique_ptr<BasicProperty>> gMachineSettings;

static const map<string, string> PropHelp = {
    {"rambank1_size",   "specifies RAM bank 1 size in MB"},
    {"rambank2_size",   "specifies RAM bank 2 size in MB"},
    {"rambank3_size",   "specifies RAM bank 3 size in MB"},
    {"rambank4_size",   "specifies RAM bank 4 size in MB"},
    {"rambank5_size",   "specifies RAM bank 5 size in MB"},
    {"rambank6_size",   "specifies RAM bank 6 size in MB"},
    {"rambank7_size",   "specifies RAM bank 7 size in MB"},
    {"rambank8_size",   "specifies RAM bank 8 size in MB"},
    {"rambank9_size",   "specifies RAM bank 9 size in MB"},
    {"rambank10_size",   "specifies RAM bank 10 size in MB"},
    {"rambank11_size",   "specifies RAM bank 11 size in MB"},
    {"rambank12_size",   "specifies RAM bank 12 size in MB"},
    {"rambank0_size",   "specifies onboard RAM bank size in MB"},
    {"gfxmem_size",     "specifies video memory size in MB"},
    {"fdd_img",         "specifies path to floppy disk image"},
    {"fdd_fmt",         "specifies floppy disk format"},
    {"fdd_wr_prot",     "toggles floppy disk's write protection"},
    {"hdd_img",         "specifies path(s) to hard disk image(s)"},
    {"hdd_img2",        "specifies path(s) to secondary hard disk image(s)"},
    {"cdr_config",      "CD-ROM device path in [bus]:[device#] format"},
    {"cdr_img",         "specifies path(s) to CD-ROM image(s)"},
    {"cdr_img2",        "specifies path(s) to secondary CD-ROM image(s)"},
    {"mon_id",          "specifies which monitor to emulate"},
    {"pci_GPU",         "specifies PCI device for Beige G3 grackle device @12"},
    {"pci_J12",         "insert a PCI device into 32-bit 66MHz slot J12"},
    {"pci_J11",         "insert a PCI device into 64-bit 33MHz slot J11"},
    {"pci_J10",         "insert a PCI device into 64-bit 33MHz slot J10"},
    {"pci_J9",          "insert a PCI device into 64-bit 33MHz slot J9"},
    {"pci_FireWire",    "insert a PCI device into PCI slot reserved for Yosemite FireWire"},
    {"pci_UltraATA",    "insert a PCI device into PCI slot reserved for Yosemite Ultra ATA"},
    {"pci_USB",         "insert a PCI device into PCI slot reserved for Yosemite USB"},
    {"pci_PERCH",       "insert a PCI device into PERCH slot"},
    {"pci_A1",          "insert a PCI device into A1 slot"},
    {"pci_B1",          "insert a PCI device into B1 slot"},
    {"pci_C1",          "insert a PCI device into C1 slot"},
    {"pci_E1",          "insert a PCI device into E1 slot"},
    {"pci_F1",          "insert a PCI device into F1 slot"},
    {"pci_D2",          "insert a PCI device into D2 slot"},
    {"pci_E2",          "insert a PCI device into E2 slot"},
    {"pci_F2",          "insert a PCI device into F2 slot"},
    {"vci_D",           "insert a VCI device 0x0D"},
    {"vci_E",           "insert a VCI device 0x0E"},
    {"serial_backend",  "specifies the backend for the serial port"},
    {"emmo",            "enables/disables factory HW tests during startup"},
    {"cpu",             "specifies CPU"},
    {"adb_devices",     "specifies which ADB device(s) to attach"},
    {"pds",             "specify device for the processsor direct slot"},
};

static uint32_t adler32(char *buf, size_t len) {
    uint32_t sum1 = 1;
    uint32_t sum2 = 0;
    while (len--) {
        sum1 = (sum1 + *(uint8_t*)buf++) % 65521;
        sum2 = (sum2 + sum1) % 65521;
    }
    return sum1 + 65536 * sum2;
}

static uint32_t oldworldchecksum(char *buf, size_t len) {
    uint32_t ck = 0;
    while (len) {
        ck += READ_WORD_BE_A(buf);
        buf += 2;
        len -= 2;
    }
    return ck;
}

bool MachineFactory::add(const string& machine_id, MachineDescription desc)
{
    if (get_registry().find(machine_id) != get_registry().end()) {
        return false;
    }

    get_registry()[machine_id] = desc;
    return true;
}

void MachineFactory::list_machines()
{
    cout << endl << "Supported machines:" << endl << endl;

    for (auto& m : get_registry()) {
        cout << setw(13) << right << m.first << "\t\t" << m.second.description << endl;
    }

    cout << endl;
}

void MachineFactory::create_device(string& dev_name, DeviceDescription& dev)
{
    for (auto& subdev_name : dev.subdev_list) {
        create_device(subdev_name, DeviceRegistry::get_descriptor(subdev_name));
    }

    gMachineObj->add_device(dev_name, dev.m_create_func());
}

int MachineFactory::create(string& mach_id)
{
    auto it = get_registry().find(mach_id);
    if (it == get_registry().end()) {
        LOG_F(ERROR, "Unknown machine id %s", mach_id.c_str());
        return -1;
    }

    LOG_F(INFO, "Initializing %s hardware...", it->second.description.c_str());

    // initialize global machine object
    gMachineObj.reset(new MachineBase(it->second.name));

    // create and register sound server
    gMachineObj->add_device("SoundServer", std::unique_ptr<SoundServer>(new SoundServer()));

    // recursively create device objects
    create_device(it->second.machine_root, DeviceRegistry::get_descriptor(it->second.machine_root));

    if (!gMachineObj->get_comp_by_name(it->second.machine_root)) {
        LOG_F(ERROR, "Machine initialization function failed!");
        gMachineObj->clear_devices();
        return -1;
    }

    // post-initialize all devices
    if (gMachineObj->postinit_devices()) {
        LOG_F(ERROR, "Could not post-initialize devices!");
        return -1;
    }

    LOG_F(INFO, "Initialization completed.");

    return 0;
}

void MachineFactory::list_properties()
{
    cout << endl;

    for (auto& mach : get_registry()) {
        cout << mach.second.description << " supported properties:" << endl << endl;

        list_device_settings(DeviceRegistry::get_descriptor(mach.second.machine_root));
    }

    cout << endl;
}

void MachineFactory::list_device_settings(DeviceDescription& dev)
{
    for (auto& d : dev.subdev_list) {
        list_device_settings(DeviceRegistry::get_descriptor(d));
    }

    print_settings(dev.properties);
}

void MachineFactory::print_settings(const PropMap& prop_map)
{
    string help;

    for (auto& p : prop_map) {
        if (PropHelp.find(p.first) != PropHelp.end())
            help = PropHelp.at(p.first);
        else
            help = "";

        cout << setw(13) << right << p.first << "\t\t" << help << endl;

        cout << setw(13) << "\t\t\t" "Valid values: ";

        switch(p.second->get_type()) {
        case PROP_TYPE_INTEGER:
            cout << dynamic_cast<IntProperty*>(p.second)->get_valid_values_as_str()
                << endl;
            break;
        case PROP_TYPE_STRING:
            cout << dynamic_cast<StrProperty*>(p.second)->get_valid_values_as_str()
                << endl;
            break;
        case PROP_TYPE_BINARY:
            cout << dynamic_cast<BinProperty*>(p.second)->get_valid_values_as_str()
                << endl;
            break;
        default:
            break;
        }
        cout << endl;
    }
}

void MachineFactory::register_device_settings(const std::string& name)
{
    auto dev = DeviceRegistry::get_descriptor(name);
    for (auto& d : dev.subdev_list) {
        register_device_settings(d);
    }

    register_settings(dev.properties);
    if (!dev.properties.empty()) {
        std::cout << "Device " << name << " Settings" << endl
                  << std::string(36, '-') << endl;
        for (auto& p : dev.properties) {
            std::cout << std::setw(24) << std::right << p.first << " : "
                      << gMachineSettings[p.first]->get_string() << endl;
        }
    }
}

int MachineFactory::register_machine_settings(const std::string& id)
{
    auto it = get_registry().find(id);
    if (it != get_registry().end()) {
        gMachineSettings.clear();

        register_device_settings(it->second.machine_root);
    } else {
        LOG_F(ERROR, "Unknown machine id %s", id.c_str());
        return -1;
    }

    return 0;
}

void MachineFactory::register_settings(const PropMap& props) {
    for (auto& p : props) {
        if (gMachineSettings.count(p.first)) {
            // This is a hack. Need to implement hierarchical paths and per device properties.
            LOG_F(ERROR, "Duplicate setting \"%s\".", p.first.c_str());
        }
        else {
            auto clone = p.second->clone();
            auto override_value = get_setting_value(p.first);
            if (override_value) {
                clone->set_string(*override_value);
            }
            gMachineSettings[p.first] = std::unique_ptr<BasicProperty>(clone);
        }
    }

}

size_t MachineFactory::read_boot_rom(string& rom_filepath, char *rom_data)
{
    ifstream rom_file;
    size_t file_size;

    rom_file.open(rom_filepath, ios::in | ios::binary);
    if (rom_file.fail()) {
        LOG_F(ERROR, "Could not open the specified ROM file.");
        file_size = 0;
        goto bail_out;
    }

    rom_file.seekg(0, rom_file.end);
    file_size = rom_file.tellg();
    if (file_size < 64 * 1024 || file_size > 4 * 1024 * 1024) {
        LOG_F(ERROR, "Unexpected ROM file size: %zu bytes. Expected size is 1 or 4 megabytes.", file_size);
        file_size = 0;
        goto bail_out;
    }

    if (rom_data) {
        rom_file.seekg(0, ios::beg);
        rom_file.read(rom_data, file_size);
    }

bail_out:
    rom_file.close();

    return file_size;
}

string MachineFactory::machine_name_from_rom(char *rom_data, size_t rom_size) {
    uint32_t date = 0;
    uint16_t major_version = 0;
    uint16_t minor_version = 0;
    uint32_t firmware_version = 0;
    uint32_t nw_product_id = 0;
    uint32_t ow_checksum_stored          = 0; uint32_t ow_checksum_calculated          = 0;
    uint32_t nw_start_checksum_stored    = 0; uint32_t nw_start_checksum_calculated    = 0;
    uint32_t nw_config_checksum_stored   = 0; uint32_t nw_config_checksum_calculated   = 0;
    uint32_t nw_recovery_checksum_stored = 0; uint32_t nw_recovery_checksum_calculated = 0;
    uint32_t nw_romimage_checksum_stored = 0; uint32_t nw_romimage_checksum_calculated = 0;
    uint16_t nw_config_signature = 0;
    bool has_nw_config = false;
    bool is_nw = false;
    uint32_t nw_subconfig_checksum_calculated = 0;

    char expected_ow[24];
    char expected_start[24];
    char expected_config[24];
    char expected_recovery[24];
    char expected_romimage[24];
    auto checksum_verbosity = loguru::Verbosity_INFO;
    expected_ow[0] = expected_start[0] = expected_config[0] = expected_recovery[0] = expected_romimage[0] = 0;

    uint32_t config_info_offset;
    char rom_id_str[17];
    rom_id_str[0] = '\0';

    int match_pass;
    int num_matches = 0;
    int best_match_count = 0;
    string machine_name = "";

    bool print_all_info = false;

    /* read firmware version from file */
    date = READ_DWORD_BE_A(&rom_data[8]);
    nw_config_signature = READ_WORD_BE_A(&rom_data[0x3f00]);
    has_nw_config = nw_config_signature == 0xc99c || nw_config_signature == 0xc03c;
    if (has_nw_config || (date > 0x19990000 && date < 0x20060000)) {
        is_nw = true;
        firmware_version = READ_DWORD_BE_A(&rom_data[4]);
        {
            nw_recovery_checksum_calculated = adler32(&rom_data[0x8000], 0x77ffc);
            nw_recovery_checksum_stored = READ_DWORD_BE_A(&rom_data[0x7fffc]);
            nw_romimage_checksum_calculated = adler32(&rom_data[0x80000], 0x7fffc);
            nw_romimage_checksum_stored = READ_DWORD_BE_A(&rom_data[0xffffc]);
        }

        if (has_nw_config) {
            nw_start_checksum_calculated = adler32(&rom_data[0], 0x3efc);
            nw_start_checksum_stored = READ_DWORD_BE_A(&rom_data[0x3efc]);
            nw_config_checksum_calculated = adler32(&rom_data[0x3f00], 0x7c);
            nw_config_checksum_stored = READ_DWORD_BE_A(&rom_data[0x3f7c]);
            nw_subconfig_checksum_calculated = adler32(&rom_data[0x3f0c], 0x70);
            nw_product_id = (READ_WORD_BE_A(&rom_data[0x3f02]) << 8) | rom_data[0x3f13];
        }
        else {
            firmware_version &= 0xffff; // the upper 2 bytes might be a machine type: 0=iMac, 1=PowerMac, 2=PowerBook
            nw_start_checksum_calculated = adler32(&rom_data[0], 0x3ffc);
            nw_start_checksum_stored = READ_DWORD_BE_A(&rom_data[0x3ffc]);
            nw_config_checksum_calculated = 0;
            nw_config_checksum_stored = 0;
            nw_subconfig_checksum_calculated = 0;
            nw_product_id = 0;
        }
        if (nw_start_checksum_calculated != nw_start_checksum_stored)
            snprintf(expected_start, sizeof(expected_start), " (expected 0x%04x)", nw_start_checksum_stored);
        if (nw_config_checksum_calculated != nw_config_checksum_stored)
            snprintf(expected_config, sizeof(expected_config), " (expected 0x%04x)", nw_config_checksum_stored);
        if (nw_recovery_checksum_calculated != nw_recovery_checksum_stored)
            snprintf(expected_recovery, sizeof(expected_recovery), " (expected 0x%04x)", nw_recovery_checksum_stored);
        if (nw_romimage_checksum_calculated != nw_romimage_checksum_stored)
            snprintf(expected_romimage, sizeof(expected_romimage), " (expected 0x%04x)", nw_romimage_checksum_stored);
    }
    else {
        date = 0;
        major_version = READ_WORD_BE_A(&rom_data[8]);
        minor_version = 0;
        if (uint8_t(major_version) >= 0x7A)
            minor_version = READ_WORD_BE_A(&rom_data[0x12]);
        firmware_version = (major_version << 16) | minor_version;
        ow_checksum_calculated = oldworldchecksum(&rom_data[4], std::min(rom_size - 4, (size_t)0x2ffffc));
        ow_checksum_stored = READ_DWORD_BE_A(&rom_data[0]);
        if (ow_checksum_calculated != ow_checksum_stored)
            snprintf(expected_ow, sizeof(expected_ow), " (expected 0x%08x)", ow_checksum_stored);

        if (rom_size >= 0x400000) {
            /* read ConfigInfo offset from file */
            config_info_offset = READ_DWORD_BE_A(&rom_data[0x300080]);

            /* read ConfigInfo.BootstrapVersion field as C string */
            memcpy(rom_id_str, &rom_data[0x300064 + config_info_offset], 16);
            rom_id_str[16] = 0;
            for (int i = 0; i < 16; i++)
                if (rom_id_str[i] < ' ' || rom_id_str[i] > '~')
                    rom_id_str[i] = '.';
        }
    }


    for (match_pass = 0; match_pass < 2; match_pass++) {
        int match_index = 0;
        for (rom_info *info = &rom_identity[0]; info->firmware_size_k; info++) {
            if (
                (info->firmware_version && info->firmware_version == firmware_version) ||
                (info->nw_product_id    && info->nw_product_id    == nw_product_id   )
            ) {
                int match_count = 1
                    + (info->ow_expected_checksum
                        && info->ow_expected_checksum == ow_checksum_stored)
                    + (info->ow_expected_checksum
                        && info->ow_expected_checksum == ow_checksum_calculated)
                    + (info->nw_subconfig_expected_checksum
                        && info->nw_subconfig_expected_checksum == nw_subconfig_checksum_calculated)
                    + (info->id_str && strcmp(rom_id_str, info->id_str) == 0)
                    ;

                if (!match_pass) {
                    if (match_count >= best_match_count) {
                        if (match_count > best_match_count) {
                            best_match_count = match_count;
                            num_matches = 0;
                        }
                        num_matches++;
                    }
                } else {
                    if (num_matches == 0) {
                        LOG_F(ERROR, "Unknown ROM");
                        print_all_info = true;
                        break;
                    }

                    if (match_count == best_match_count) {
                        match_index++;
                        LOG_F(INFO, "Found match (%d/%d):", match_index, num_matches);
                        if (info->rom_description)
                            LOG_F(INFO, "    ROM description: %s", info->rom_description);
                        if (info->dppc_description)
                            LOG_F(INFO, "    Machine identified from ROM: %s", info->dppc_description);
                        if (
                            info->nw_firmware_updater_name && info->nw_openfirmware_name &&
                            strcmp(info->nw_firmware_updater_name, info->nw_openfirmware_name) == 0
                        ) {
                            LOG_F(INFO, "    Code Name: %s", info->nw_firmware_updater_name);
                        } else {
                            if (info->nw_firmware_updater_name)
                                LOG_F(INFO, "    Code Name (from Firmware Updater): %s", info->nw_firmware_updater_name);
                            if (info->nw_openfirmware_name)
                                LOG_F(INFO, "    Code Name (from Open Firmware): %s", info->nw_openfirmware_name);
                        }
                        if (info->nw_product_id) {
                            LOG_F(INFO, "    Product ID: 0x%04x.%02x = %s%d,%d",
                                nw_product_id >> 8, nw_product_id & 0xff,
                                (nw_product_id >> 20) == 0 ? "PowerMac" :
                                (nw_product_id >> 20) == 1 ? "PowerBook" :
                                (nw_product_id >> 20) == 2 ? "PowerMac" :
                                (nw_product_id >> 20) == 3 ? "PowerBook" :
                                (nw_product_id >> 20) == 4 ? "RackMac" : "???",
                                (nw_product_id >> 14) & 31,
                                (nw_product_id >>  8) & 31
                            );
                        }
                        if (info->nw_subconfig_expected_checksum) {
                            LOG_F(INFO, "    Config Checksum: 0x%08x", nw_subconfig_checksum_calculated);
                        }
                        if (rom_size != info->firmware_size_k * 1024) {
                            LOG_F(ERROR, "    Unexpected ROM file size: %zu bytes. Expected size is %d %s.",
                                rom_size,
                                info->firmware_size_k & 0x3ff ?
                                    info->firmware_size_k :
                                    info->firmware_size_k / 1024,
                                info->firmware_size_k & 0x3ff ? "kiB" : "MiB"
                            );
                        }
                        if (info->dppc_machine) {
                            if (machine_name.empty()) {
                                machine_name = info->dppc_machine;
                            }
                        } else
                            LOG_F(ERROR, "    This ROM is not supported.");
                    }
                } // if match_pass
            } // if match
        } // for rom_info
    } // for match_pass

    if (1 || print_all_info) {
        if (is_nw) {
            LOG_F(INFO, "Info from ROM:");
            LOG_F(INFO, "    ROM Date: %04x-%02x-%02x", date >> 16, (date >> 8) & 0xff, date & 0xff);
            if (firmware_version < 0xffff)
                LOG_F(INFO, "    ROM Version: %x.%03x", (firmware_version >> 12) & 15, firmware_version & 0xfff);
            else
                LOG_F(INFO, "    ROM Version: %x.%x.%03x",
                    firmware_version >> 16, (firmware_version >> 12) & 15, firmware_version & 0xfff);
            if (has_nw_config) {
                LOG_F(INFO, "    Product ID: 0x%04x.%02x 0x%08x = %s%d,%d",
                    nw_product_id >> 8, nw_product_id & 0xff,
                    nw_subconfig_checksum_calculated,
                    (nw_product_id >> 20) == 0 ? "PowerMac" :
                    (nw_product_id >> 20) == 1 ? "PowerBook" :
                    (nw_product_id >> 20) == 2 ? "PowerMac" :
                    (nw_product_id >> 20) == 3 ? "PowerBook" :
                    (nw_product_id >> 20) == 4 ? "RackMac" : "???",
                    (nw_product_id >> 14) & 31,
                    (nw_product_id >>  8) & 31
                );
            }
        } else {
            if (uint8_t(major_version) >= 0x7A)
                LOG_F(INFO, "    ROM Version: %04x.%04x", major_version, minor_version);
            else
                LOG_F(INFO, "    ROM Version: %04x", major_version);
            if (rom_id_str[0])
                LOG_F(INFO, "    ConfigInfo.BootstrapVersion: \"%s\"", rom_id_str);
        }
    }

    if (expected_ow[0] || expected_start[0] || expected_config[0] || expected_recovery[0] || expected_romimage[0])
        checksum_verbosity = loguru::Verbosity_ERROR;

    if (1 || print_all_info || checksum_verbosity != loguru::Verbosity_INFO) {
        if (is_nw) {
            if (has_nw_config) {
                VLOG_F(checksum_verbosity, "    ROM Checksums: 0x%08x%s, 0x%08x%s, 0x%08x%s, 0x%08x%s",
                    nw_start_checksum_calculated, expected_start,
                    nw_config_checksum_calculated, expected_config,
                    nw_recovery_checksum_calculated, expected_recovery,
                    nw_romimage_checksum_calculated, expected_romimage
                );
            }
            else {
                VLOG_F(checksum_verbosity, "    ROM Checksums: 0x%08x%s, 0x%08x%s, 0x%08x%s",
                    nw_start_checksum_calculated, expected_start,
                    nw_recovery_checksum_calculated, expected_recovery,
                    nw_romimage_checksum_calculated, expected_romimage
                );
            }
        }
        else {
            VLOG_F(checksum_verbosity, "    ROM Checksum: 0x%08x%s",
                ow_checksum_calculated, expected_ow
            );
        }
    }

    return machine_name;
}

/* Read ROM file content and transfer it to the dedicated ROM region */
int MachineFactory::load_boot_rom(char *rom_data, size_t rom_size) {
    int      result = 0;
    uint32_t rom_load_addr;
    //AddressMapEntry *rom_reg;

    if (rom_size == 0x400000UL) { // Old World ROMs
        rom_load_addr = 0xFFC00000UL;
    } else if (rom_size == 0x100000UL) { // New World ROMs
        rom_load_addr = 0xFFF00000UL;
    } else {
        LOG_F(ERROR, "Unexpected ROM File size: %zu bytes.", rom_size);
        result = -1;
    }

    if (!result) {
        MemCtrlBase* mem_ctrl = dynamic_cast<MemCtrlBase*>(
            gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));

        if ((/*rom_reg = */mem_ctrl->find_rom_region())) {
            mem_ctrl->set_data(rom_load_addr, (uint8_t*)rom_data, (uint32_t)rom_size);
        } else {
            LOG_F(ERROR, "Could not locate physical ROM region!");
            result = -1;
        }
    }

    return result;
}

int MachineFactory::create_machine_for_id(string& id, char *rom_data, size_t rom_size) {
    if (MachineFactory::create(id) < 0) {
        return -1;
    }
    if (load_boot_rom(rom_data, rom_size) < 0) {
        return -1;
    }
    return 0;
}

GetSettingValueFunc MachineFactory::get_setting_value;
