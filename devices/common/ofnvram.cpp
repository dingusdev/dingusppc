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

/** Utilities for working with Apple Open Firmware and CHRP NVRAM partitions. */

#include <devices/common/ofnvram.h>
#include <devices/common/nvram.h>
#include <endianswap.h>
#include <machines/machinebase.h>
#include <memaccess.h>

#include <cinttypes>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <tuple>

using namespace std;

static std::string hex2str(uint32_t n)
{
   std::stringstream ss;
   ss << setw(8) << setfill('0') << hex << n;
   return ss.str();
}

static uint32_t str2env(string& num_str) {
    try {
        return (uint32_t)stoul(num_str, NULL, 0);
    } catch (invalid_argument& exc) {
        try {
            string num_str2 = string("0x") + num_str;
            return std::stoul(num_str2, NULL, 0);
        } catch (invalid_argument& exc) {
            throw invalid_argument(string("Cannot convert ") + num_str);
        }
    }
}

bool OfConfigAppl::validate() {
    int             i;
    OfConfigHdrAppl hdr{};

    if (this->nvram_obj == nullptr)
        return false;

    // read OF partition header
    for (i = 0; i < sizeof(OfConfigHdrAppl); i++) {
        ((uint8_t*)&hdr)[i] = this->nvram_obj->read_byte(OF_NVRAM_OFFSET + i);
    }

    // validate partition signature and version
    if (BYTESWAP_16(hdr.sig) != OF_NVRAM_SIG || hdr.version != 5)
        return false;

    this->size = hdr.num_pages * 256;

    if (this->size != OF_CFG_SIZE)
        return false;

    // read the entire partition into the local buffer
    for (i = 0; i < this->size; i++) {
        this->buf[i] = this->nvram_obj->read_byte(OF_NVRAM_OFFSET + i);
    }

    // verify partition checksum
    if (this->checksum_partition() ^ 0xFFFFU)
        return false;

    return true;
}

uint16_t OfConfigAppl::checksum_partition() {
    uint32_t acc = 0;

    for (int i = 0; i < this->size; i += 2) {
        acc += READ_WORD_BE_A(&this->buf[i]);
    }

    return acc + (acc >> 16);
}

static const string flag_names[8] = {
    "little-endian?",
    "real-mode?",
    "auto-boot?",
    "diag-switch?",
    "fcode-debug?",
    "oem-banner?",
    "oem-logo?",
    "use-nvramrc?"
};

static const map<string, std::tuple<int, uint16_t>> of_vars = {
    // name,            type,               offset
    {"real-base",       {OF_VAR_TYPE_INT,   0x10}},
    {"real-size",       {OF_VAR_TYPE_INT,   0x14}},
    {"virt-base",       {OF_VAR_TYPE_INT,   0x18}},
    {"virt-size",       {OF_VAR_TYPE_INT,   0x1C}},
    {"load-base",       {OF_VAR_TYPE_INT,   0x20}},
    {"pci-probe-list",  {OF_VAR_TYPE_INT,   0x24}},
    {"screen-#columns", {OF_VAR_TYPE_INT,   0x28}},
    {"screen-#rows",    {OF_VAR_TYPE_INT,   0x2C}},
    {"selftest-#megs",  {OF_VAR_TYPE_INT,   0x30}},
    {"boot-device",     {OF_VAR_TYPE_STR,   0x34}},
    {"boot-file",       {OF_VAR_TYPE_STR,   0x38}},
    {"diag-device",     {OF_VAR_TYPE_STR,   0x3C}},
    {"diag-file",       {OF_VAR_TYPE_STR,   0x40}},
    {"input-device",    {OF_VAR_TYPE_STR,   0x44}},
    {"output-device",   {OF_VAR_TYPE_STR,   0x48}},
    {"oem-banner",      {OF_VAR_TYPE_STR,   0x4C}},
    {"oem-logo",        {OF_VAR_TYPE_STR,   0x50}},
    {"nvramrc",         {OF_VAR_TYPE_STR,   0x54}},
    {"boot-command",    {OF_VAR_TYPE_STR,   0x58}},
};

const OfConfigImpl::config_dict& OfConfigAppl::get_config_vars() {
    this->_config_vars.clear();

    if (!this->validate())
        return _config_vars;

    uint8_t of_flags = this->buf[12];

    // populate flags
    for (int i = 0; i < 8; i++) {
        _config_vars.push_back(std::make_pair(flag_names[i],
                             ((of_flags << i) & 0x80) ? "true" : "false"));
    }

    // populate the remaining variables
    for (auto& var : of_vars) {
        auto& name   = var.first;
        auto  type   = std::get<0>(var.second);
        auto  offset = std::get<1>(var.second);

        switch (type) {
        case OF_VAR_TYPE_INT:
            _config_vars.push_back(std::make_pair(name,
                hex2str(READ_DWORD_BE_A(&this->buf[offset]))));
            break;
        case OF_VAR_TYPE_STR:
            uint16_t str_offset = READ_WORD_BE_A(&this->buf[offset]) - OF_NVRAM_OFFSET;
            uint16_t str_len    = READ_WORD_BE_A(&this->buf[offset+2]);

            if ((str_offset + str_len) > OF_CFG_SIZE) {
                cout << "string property too long - skip it" << endl;
                break;
            }

            char prop_val[OF_CFG_SIZE] = "";
            memcpy(prop_val, &this->buf[str_offset], str_len);
            prop_val[str_len] = '\0';

            _config_vars.push_back(std::make_pair(name, prop_val));
        }
    }

    return _config_vars;
}

void OfConfigAppl::update_partition() {
    // set checksum in the header to zero
    this->buf[4] = 0;
    this->buf[5] = 0;

    // calculate new checksum
    uint16_t checksum = this->checksum_partition();
    checksum = checksum ? ~checksum : 0xFFFFU;

    // stuff checksum into the header
    WRITE_WORD_BE_A(&this->buf[4], checksum);

    // write the entire partition back to NVRAM
    for (int i = 0; i < this->size; i++) {
        this->nvram_obj->write_byte(OF_NVRAM_OFFSET + i, this->buf[i]);
    }
}

bool OfConfigAppl::set_var(std::string& var_name, std::string& value) {
    int i, flag;

    if (!this->validate())
        return false;

    // check if the user tries to change a flag
    for (i = 0; i < 8; i++) {
        if (var_name == flag_names[i]) {
            if (value == "true")
                flag = 1;
            else if (value == "false")
                flag = 0;
            else {
                cout << "Invalid property value: " << value << endl;
                return false;
            }
            uint8_t flag_bit = 0x80U >> i;
            uint8_t of_flags = this->buf[12];

            if (flag)
                of_flags |= flag_bit;
            else
                of_flags &= ~flag_bit;

            this->buf[12] = of_flags;
            this->update_partition();
            return true;
        }
    }

    // see if one of the standard properties should be changed
    if (of_vars.find(var_name) == of_vars.end()) {
        cout << "Attempt to change unknown variable " << var_name << endl;
        return false;
    }

    auto type   = std::get<0>(of_vars.at(var_name));
    auto offset = std::get<1>(of_vars.at(var_name));

    if (type == OF_VAR_TYPE_INT) {
        uint32_t num;
        try {
            num = str2env(value);
        } catch (invalid_argument& exc) {
            cout << exc.what() << endl;
            return false;
        }
        WRITE_DWORD_BE_A(&this->buf[offset], num);
        this->update_partition();
        cout << " ok" << endl; // mimic Forth
    } else {
        uint16_t str_offset = READ_WORD_BE_A(&this->buf[offset]);
        uint16_t str_len    = READ_WORD_BE_A(&this->buf[offset+2]);

        OfConfigHdrAppl *hdr = (OfConfigHdrAppl *)&this->buf[0];
        uint16_t here = READ_WORD_BE_A(&hdr->here);
        uint16_t top  = READ_WORD_BE_A(&hdr->top);

        // check if there is enough space in the heap for the new string
        // the heap is grown down from offset 0x2000 and cannot be lower than here (0x185c)
        uint16_t new_top = top + str_len - value.length();
        if (new_top < here) {
            cout << "No room in the heap!" << endl;
            return false;
        }

        // remove the old string
        std::memmove(&this->buf[top + str_len - OF_NVRAM_OFFSET],
            &this->buf[top - OF_NVRAM_OFFSET], str_offset - top);

        for (auto& var : of_vars) {
            auto type   = std::get<0>(var.second);
            auto offset = std::get<1>(var.second);
            if (type == OF_VAR_TYPE_STR) {
                uint16_t i_str_offset = READ_WORD_BE_A(&this->buf[offset]);
                if (i_str_offset < str_offset) {
                    WRITE_WORD_BE_A(&this->buf[offset], i_str_offset + str_len);
                }
            }
        }
        top = new_top;

        // copy new string into NVRAM buffer char by char
        i = 0;
        for(char& ch : value) {
            this->buf[top + i - OF_NVRAM_OFFSET] = ch == '\x0A' ? '\x0D' : ch;
            i++;
        }

        // stuff new values into the variable state
        WRITE_WORD_BE_A(&this->buf[offset+0], top);
        WRITE_WORD_BE_A(&this->buf[offset+2], value.length());

        // update partition header
        WRITE_WORD_BE_A(&hdr->top, top);

        // update physical NVRAM
        this->update_partition();
    }

    return true;
}

uint8_t OfConfigChrp::checksum_hdr(const uint8_t* data)
{
    uint16_t sum = data[0];

    for (int i = 2; i < 16; i++) {
        sum += data[i];
        if (sum >= 256)
            sum = (sum + 1) & 0xFFU;
    }

    return sum;
}

bool OfConfigChrp::validate()
{
    int     i, pos, len;
    uint8_t sig;
    bool    wip = true;
    bool    of_part_found = false;

    // search the entire 8KB NVRAM for CHRP OF config partition.
    // Bail out if an unknown partition or free space is encountered.
    // Skip over known partitions.
    for (pos = 0; wip && pos < 8192;) {
        sig = this->nvram_obj->read_byte(pos);
        switch (sig) {
        case NVRAM_SIG_OF_ENV:
            of_part_found = true;
            // fall-through
        case NVRAM_SIG_FREE:
            wip = false;
            break;
        case NVRAM_SIG_VPD:
        case NVRAM_SIG_DIAG:
        case NVRAM_SIG_OF_CFG:
        case NVRAM_SIG_MAC_OS:
        case NVRAM_SIG_ERR_LOG:
            // skip valid partitions we're not interested in
            len = (this->nvram_obj->read_byte(pos + 2) << 8) |
                   this->nvram_obj->read_byte(pos + 3);
            if (!len || (len * 16) >= 8192)
                break;
            pos += len * 16;
            break;
        default:
            wip = false;
        }
    }

    if (!of_part_found)
        return false;

    OfConfigHdrChrp hdr{};

    // read OF partition header
    for (i = 0; i < sizeof(OfConfigHdrChrp); i++) {
        ((uint8_t*)&hdr)[i] = this->nvram_obj->read_byte(pos + i);
    }

    len = BYTESWAP_16(hdr.length) * 16;

    // sanity checks
    if (hdr.sig != NVRAM_SIG_OF_ENV || len < 16 || len > (4096 + sizeof(OfConfigHdrChrp)))
        return false;

    // calculate partition header checksum
    uint8_t chk_sum = this->checksum_hdr((uint8_t*)&hdr);

    if (chk_sum != hdr.checksum)
        return false;

    len -= sizeof(OfConfigHdrChrp);
    pos += sizeof(OfConfigHdrChrp);

    this->data_offset = pos;

    // read the entire partition into the local buffer
    for (i = 0; i < len; i++) {
        this->buf[i] = this->nvram_obj->read_byte(pos + i);
    }

    return true;
}

const OfConfigImpl::config_dict& OfConfigChrp::get_config_vars() {
    int len;

    this->_config_vars.clear();

    this->data_length = 0;

    if (!this->validate())
        return _config_vars;

    for (int pos = 0; pos < 4096;) {
        char *pname = (char *)&this->buf[pos];
        bool got_name = false;

        // scan property name until '=' is encountered
        // or max length is reached
        for (len = 0; ; pos++, len++) {
            if (len >= 32) {
                cout << "name > 31 chars" << endl;
                break;
            }
            if (pos >= 4096) {
                cout << "no = sign before end of partition" << endl;
                break;
            }
            if (pname[len] == '=') {
                if (len) {
                    got_name = true;
                }
                else {
                    cout << "got = sign but no name" << endl;
                }
                break;
            }
            if (pname[len] == '\0') {
                if (len) {
                    cout << "no = sign before termminating null" << endl;
                }
                else {
                    // empty property name -> free space reached
                }
                break;
            }
        }

        if (!got_name) {
            break;
        }

        char prop_name[32];
        memcpy(prop_name, pname, len);
        prop_name[len] = '\0';

        pos++; // skip past '='
        char *pval = (char *)&this->buf[pos];

        // determine property value length
        for (len = 0; pos < 4096; pos++, len++) {
            if (pval[len] == '\0')
                break;
        }

        // ensure each property value is null-terminated
        if (pos >= 4096) {
            cout << "ran off partition end" << endl;
            break;
        }

        this->_config_vars.push_back(std::make_pair(prop_name, pval));
        pos++; // skip past null terminator

        this->data_length = pos; // point to after null
    }

    //cout << "Read " << this->data_length << " bytes from nvram." << endl;
    return this->_config_vars;
}

bool OfConfigChrp::update_partition() {
    unsigned pos = 0;

    memset(this->buf, 0, 4096);

    for (auto& var : this->_config_vars) {
        if ((pos + var.first.length() + var.second.length() + 2) > 4096) {
            cout << "No room in the partition!" << endl;
            return false;
        }
        memcpy(&this->buf[pos], var.first.c_str(), var.first.length());
        pos += var.first.length();
        this->buf[pos++] = '=';
        memcpy(&this->buf[pos], var.second.c_str(), var.second.length());
        pos += var.second.length();
        this->buf[pos++] = '\0';
    }

    // write the entire partition back to NVRAM
    for (int i = 0; i < 4096; i++) {
        this->nvram_obj->write_byte(this->data_offset + i, this->buf[i]);
    }

    //cout << "Wrote " << pos << " bytes to nvram." << endl;
    return true;
}

bool OfConfigChrp::set_var(std::string& var_name, std::string& value) {
    if (!this->validate())
        return false;

    // see if we're about to change a flag
    if (var_name.back() == '?') {
        if (value != "true" && value != "false") {
            cout << "Flag value can be 'true' or 'false'" << endl;
            return false;
        }
    }

    unsigned free_space = 4096 - this->data_length;
    bool found = false;

    // see if the user tries to change an existing property
    for (auto& var : this->_config_vars) {
        if (var.first == var_name) {
            found = true;

            if (value.length() > var.second.length()) {
                if ((value.length() - var.second.length()) > free_space) {
                    cout << "No room for updated nvram variable!" << endl;
                    return false;
                }
            }

            var.second = value;
            break;
        }
    }

    if (!found) {
        if ((var_name.length() + value.length() + 2) > free_space) {
            cout << "No room for new nvram variable!" << endl;
            return false;
        }
        this->_config_vars.push_back(std::make_pair(var_name, value));
    }

    return this->update_partition();
}

int OfConfigUtils::init()
{
    this->nvram_obj = dynamic_cast<NVram*>(gMachineObj->get_comp_by_name("NVRAM"));
    return this->nvram_obj == nullptr;
}

bool OfConfigUtils::open_container() {
    OfConfigImpl*   cfg_obj;

    if (this->cfg_impl == nullptr) {
        cfg_obj = new OfConfigAppl(this->nvram_obj);
        if (cfg_obj->validate()) {
            this->cfg_impl = std::unique_ptr<OfConfigImpl>(cfg_obj);
            return true;
        } else {
            delete(cfg_obj);

            cfg_obj = new OfConfigChrp(this->nvram_obj);
            if (cfg_obj->validate()) {
                this->cfg_impl = std::unique_ptr<OfConfigImpl>(cfg_obj);
                return true;
            } else {
                delete(cfg_obj);
            }
        }
    } else {
        return this->cfg_impl->validate();
    }

    cout << "No valid Open Firmware partition found!" << endl;

    return false;
}

static std::string ReplaceAll(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

void OfConfigUtils::printenv() {
    if (!this->open_container())
        return;

    OfConfigImpl::config_dict vars = this->cfg_impl->get_config_vars();

    for (auto& var : vars) {
        std::string val = var.second;
        ReplaceAll(val, "\r\n", "\n");
        ReplaceAll(val, "\r", "\n");
        ReplaceAll(val, "\n", "\n                                  "); // 34 spaces
        cout << setw(34) << left << var.first << val << endl; // name column has width 34
    }
}

void OfConfigUtils::setenv(string var_name, string value)
{
    if (!this->open_container())
        return;

    OfConfigImpl::config_dict vars = this->cfg_impl->get_config_vars();

    if (!this->cfg_impl->set_var(var_name, value)) {
        cout << " Please try again" << endl;
    } else {
        cout << " ok" << endl; // mimic Forth
    }
}
