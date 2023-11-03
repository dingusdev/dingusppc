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

/** Apple/CHRP Open Firmware NVRAM partition definitions. */

#ifndef OF_NVRAM_H
#define OF_NVRAM_H

#include <cinttypes>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class NVram;

/** ========== Apple Open Firmware 1.x/2.x partition definitions. ========== */
#define OF_NVRAM_OFFSET 0x1800
#define OF_NVRAM_SIG    0x1275
#define OF_CFG_SIZE     0x800

// OF Variable types
enum {
    OF_VAR_TYPE_INT = 1, // integer
    OF_VAR_TYPE_STR = 2, // string
    OF_VAR_TYPE_FLG = 3, // boolean flag
};

typedef struct {
    uint16_t    sig;        // >nv.1275     partition signature (= 0x1275)
    uint8_t     version;    // >nv.version  header version (= 5)
    uint8_t     num_pages;  // >nv.pages    number of memory pages (= 8 pages 0x100 bytes each)
    uint16_t    checksum;   // >nv.checksum partition checksum
    uint16_t    here;       // >nv.here     offset to the next free byte (offset of after last string length; = 0x185c)
    uint16_t    top;        // >nv.top      offset to the last free byte (offset of string with lowest offset; < 0x2000)
} OfConfigHdrAppl;

/** ================== CHRP NVRAM partition definitions. ================== */

/** CHRP NVRAM partition signatures. */
enum {
    NVRAM_SIG_OF_CFG    = 0x50, // of-config (unused in Power Macintosh)
    NVRAM_SIG_VPD       = 0x52, // of-vpd (unused in Power Macintosh)
    NVRAM_SIG_DIAG      = 0x5F, // diagnostics partition
    NVRAM_SIG_OF_ENV    = 0x70, // common partition with OF environment vars
    NVRAM_SIG_ERR_LOG   = 0x72, // post-err-log
    NVRAM_SIG_FREE      = 0x7F, // free space
    NVRAM_SIG_MAC_OS    = 0xA0, // APL,MacOS75
};

typedef struct {
    uint8_t     sig;        // partition signature, must be 0x70
    uint8_t     checksum;   // partition header checksum (sig, length & name)
    uint16_t    length;     // partition length in 16-byte blocks
    char        name[12];   // null-terminated partition name
} OfConfigHdrChrp;

//  - interface
class OfConfigImpl {
public:
    using config_dict = std::vector<std::pair<std::string, std::string>>;

    virtual ~OfConfigImpl() = default;
    virtual bool validate() = 0;
    virtual const config_dict& get_config_vars() = 0;
    virtual bool set_var(std::string& name, std::string& val) = 0;
};

// Old World implementation
class OfConfigAppl : public OfConfigImpl {
public:
    OfConfigAppl(NVram* nvram_obj) { this->nvram_obj = nvram_obj; };
    ~OfConfigAppl() = default;

    bool validate();
    const config_dict& get_config_vars();
    bool set_var(std::string& var_name, std::string& value);

protected:
    uint16_t    checksum_partition();
    void        update_partition();

private:
    NVram*      nvram_obj = nullptr;
    uint8_t     buf[OF_CFG_SIZE];
    int         size = 0;
    config_dict _config_vars;
};

// New World implementation
class OfConfigChrp : public OfConfigImpl {
public:
    OfConfigChrp(NVram* nvram_obj) { this->nvram_obj = nvram_obj; };
    ~OfConfigChrp() = default;

    bool validate();
    const config_dict& get_config_vars();
    bool set_var(std::string& var_name, std::string& value);

protected:
    uint8_t checksum_hdr(const uint8_t* data);
    bool    update_partition();

private:
    NVram*      nvram_obj = nullptr;
    uint8_t     buf[4096];
    unsigned    data_offset = 0; // offset to the OF config data
    unsigned    data_length = 0; // length of the OF config data
    config_dict _config_vars;
};

class OfConfigUtils {
public:
    OfConfigUtils()  = default;
    ~OfConfigUtils() = default;

    int  init();
    void printenv();
    void setenv(std::string var_name, std::string value);

protected:
    bool open_container();

private:
    std::unique_ptr<OfConfigImpl>   cfg_impl  = nullptr;
    NVram*                          nvram_obj = nullptr;
};

#endif // OF_NVRAM_H
