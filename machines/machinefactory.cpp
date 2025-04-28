/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-25 divingkatae and maximum
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

/**
    Power Macintosh ROM identification map.

    Maps rom info to machine name and description.
*/

typedef struct {
    uint32_t    firmware_version;
    uint32_t    firmware_size_k;
    uint32_t    ow_expected_checksum;
    uint32_t    nw_product_id;
    uint32_t    nw_subconfig_expected_checksum; // checksum of the system config section but without the firmware version and date
    const char *id_str;                         // Bootstrap string located at offset 0x30D064 (PCI Macs) or 0x30C064 (NuBus Macs)
    const char *nw_firmware_updater_name;
    const char *nw_openfirmware_name;
    const char *dppc_machine;
    const char *dppc_description;
    const char *rom_description;
} rom_info;

static rom_info rom_identity[] = {
    { 0x00696000,   64, 0x28ba61ce,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Macintosh 128K"                                    },
    { 0x00696000,   64, 0x28ba4e50,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Macintosh 512K"                                    },
    { 0x00756000,  128, 0x4d1eeee1,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "MacPlus v1"                                        },
    { 0x00756000,  128, 0x4d1eeae1,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "MacPlus v2"                                        },
    { 0x00756000,  128, 0x4d1f8172,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "MacPlus v3"                                        },
    { 0x01780000,  256, 0x97221136,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Mac II FDHD & IIx & IIcx"                          },
    { 0x01780000,  256, 0x9779d2c4,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "MacII (800k v2)"                                   },
    { 0x01780000,  256, 0x97851db6,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "MacII (800k v1)"                                   },
    { 0x02760000,  256, 0xb2e362a8,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Mac SE"                                            },
    { 0x02760000,  256, 0xb306e171,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Mac SE FDHD"                                       },
    { 0x02760000,  512, 0xa49f9914,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Classic (with XO ROMDisk)"                         },
    { 0x037a0000,  256, 0x96ca3846,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Mac Portable"                                      },
    { 0x037a11f1,  256, 0x96645f9c,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "PowerBook 100"                                     },
    { 0x067c10f1,  512, 0x368cadfe,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Mac IIci"                                          },
    { 0x067c11f2,  512, 0x4147dd77,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Mac IIfx"                                          },
    { 0x067c12f1,  512, 0x36b7fb6c,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Mac IIsi"                                          },
    { 0x067c13f1,  512, 0x350eacf0,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Mac LC"                                            },
    { 0x067c15f1, 1024, 0x420dbff3,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Quadra 700&900 & PB140&170"                        },
    { 0x067c16f1,  512, 0x3193670e,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Classic II"                                        },
    { 0x067c17f2, 1024, 0x3dc27823,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Quadra 950"                                        },
    { 0x067c18f1, 1024, 0xe33b2724,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Powerbook 160 & 165c & 180 & 180c"                 },
    { 0x067c19f2,  512, 0x35c28f5f,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Mac LCII"                                          },
    { 0x067c20f2, 1024, 0x4957eb49,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "MacIIvx & IIvi"                                    },
    { 0x067c21f5, 1024, 0xecfa989b,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Powerbook 210,230,250"                             },
    { 0x067c22f2, 1024, 0xec904829,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "LCIII (older)"                                     },
    { 0x067c22f3, 1024, 0xecbbc41c,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Mac LCIII"                                         },
    { 0x067c23f1, 1024, 0xf1a6f343,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Centris 610,650, Quadra 800"                       },
    { 0x067c23f2, 1024, 0xf1acad13,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Quadra 610,650,maybe 800"                          },
    { 0x067c24f2, 1024, 0xecd99dc0,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Color Classic"                                     },
    { 0x067c25f1, 1024, 0xede66cbd,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Color Classic II & LC 550 & Performa 275,550,560 & Macintosh TV"},
    { 0x067c26f1, 1024, 0xff7439ee,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Quadra 605"                                        },
    { 0x067c27f2, 1024, 0x0024d346,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Powerbook Duo 270"                                 },
    { 0x067c29f2, 1024, 0x015621d7,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Powerbook 280&280c"                                },
    { 0x067c30f1, 2048, 0xb6909089,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "PowerBook 520&520c&540&540c"                       },
    { 0x067c30f2, 2048, 0xb57687a5,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Pb550c"                                            },
    { 0x067c31f1, 1024, 0xfda22562,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Powerbook 150"                                     },
    { 0x067c32f1, 1024, 0x06684214,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Quadra 630"                                        },
    { 0x067c32f2, 1024, 0x064dc91d,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Performa 580 & 588"                                },
    { 0x077d10f3, 2048, 0x5bf10fd1,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Quadra 660av & 840av"                              },

    { 0x077d20f2, 4096, 0x9feb69b3,           0, 0,    "Boot PDM 601 1.0", 0, 0, "pm6100"  , "NuBus Power Mac"            , "Power Mac 6100 & 7100 & 8100"                      }, // Piltdown Man
    { 0x077d22f1, 4096, 0x9c7c98f7,           0, 0,    "Boot PDM 601 1.0", 0, 0, "pm9150"  , "NuBus Power Mac"            , "Workgroup Server 9150-80"                          }, // Piltdown Man
    { 0x077d23f1, 4096, 0x9b7a3aad,           0, 0,    "Boot PDM 601 1.1", 0, 0, "pm7100"  , "NuBus Power Mac"            , "Power Mac 7100 (newer)"                            }, // Piltdown Man
    { 0x077d25f1, 4096, 0x9b037f6f,           0, 0,    "Boot PDM 601 1.1", 0, 0, "pm9150"  , "NuBus Power Mac"            , "Workgroup Server 9150-120"                         }, // Piltdown Man
    { 0x077d26f1, 4096, 0x63abfd3f,           0, 0,    "Boot Cordyceps 6", 0, 0, "pm5200"  , "Power Mac 5200/6200 series" , "Power Mac & Performa 5200,5300,6200,6300"          }, // Cordyceps
    { 0x077d28a5, 4096, 0x67a1aa96,           0, 0,    "..0.....Boot TNT", 0, 0, nullptr   , nullptr                      , "TNT A5c1"                                          },
    { 0x077d28f1, 4096, 0x96cd923d,           0, 0,    "Boot TNT 0.1p..]", 0, 0, "pm7200"  , "Power Mac 7xxxx/8xxx series", "Power Mac 7200&7500&8500&9500 v1"                  }, // TNT Trinitrotoluene
    { 0x077d28f2, 4096, 0x9630c68b,           0, 0,    "Boot TNT 0.1p..]", 0, 0, "pm7200"  , "Power Mac 7xxxx/8xxx series", "Power Mac 7200&7500&8500&9500 v2, SuperMac S900"   }, // TNT Trinitrotoluene
    { 0x077d28f2, 4096, 0x962f6c13,           0, 0,    "Boot TNT 0.1p..]", 0, 0, nullptr   , "Apple Network Server series", "Apple Network Server 500"                          }, // TNT Trinitrotoluene // Shiner
    { 0x077d29f1, 4096, 0x6f5724c0,           0, 0,    "Boot Alchemy 0.1", 0, 0, "pm6400"  , "Performa 6400"              , "PM 5400, Performa 6400"                            }, // Alchemy
    { 0x077d2af2, 4096, 0x83c54f75,           0, 0,    "Boot PBX 603 0.0", 0, 0, "pb-preg3", "PowerBook Pre-G3"           , "Powerbook 2300 & PB5x0 PPC Upgrade"                },
    { 0x077d2bf1, 2048, 0x4d27039c,           0, 0,    nullptr           , 0, 0, nullptr   , nullptr                      , "Powerbook 190cs"                                   },
    { 0x077d2cc6, 4096, 0x2bf65931,           0, 0,    "Boot Pip 0.1p..]", 0, 0, "pippin"  , "Bandai Pippin"              , "Bandai Pippin (Kinka Dev)"                         },
    { 0x077d2cf2, 4096, 0x2bef21b7,           0, 0,    "Boot Pip 0.1p..]", 0, 0, "pippin"  , "Bandai Pippin"              , "Bandai Pippin (Kinka 1.0)"                         },
    { 0x077d2cf5, 4096, 0x3e10e14c,           0, 0,    "Boot Pip 0.1p..]", 0, 0, "pippin"  , "Bandai Pippin"              , "Bandai Pippin (Kinka 1.2)"                         },
    { 0x077d2cf8, 4096, 0x3e6b3ee4,           0, 0,    "Boot Pip 0.1p..]", 0, 0, "pippin"  , "Bandai Pippin"              , "Bandai Pippin (Kinka 1.3)"                         },
    { 0x077d32f3, 4096, 0x838c0831,           0, 0,    "Boot PBX 603 0.0", 0, 0, "pb-preg3", "PowerBook Pre-G3"           , "PowerBook 1400"                                    },
    { 0x077d32f3, 4096, 0x83a21950,           0, 0,    "Boot PBX 603 0.0", 0, 0, "pb-preg3", "PowerBook Pre-G3"           , "PowerBook 1400cs"                                  },
    { 0x077d34f2, 4096, 0x960e4be9,           0, 0,    "Boot TNT 0.1p..]", 0, 0, "pm7300"  , "Power Mac 7xxxx/8xxx series", "Power Mac 7300 & 7600 & 8600 & 9600 (v1)"          }, // TNT Trinitrotoluene
    { 0x077d34f5, 4096, 0x960fc647,           0, 0,    "Boot TNT 0.1p..]", 0, 0, "pm8600"  , "Power Mac 7xxxx/8xxx series", "Power Mac 8600 & 9600 (v2)"                        }, // TNT Trinitrotoluene
    { 0x077d35f2, 4096, 0x6e92fe08,           0, 0,    "Boot Gazelle 0.1", 0, 0, "pm6500"  , "Power Mac 6500"             , "Power Mac 6500, Twentieth Anniversary Macintosh"   }, // Gazelle
    { 0x077d36f1, 4096, 0x276ec1f1,           0, 0,    "Boot PSX 0.1p..]", 0, 0, nullptr   , nullptr                      , "PowerBook 2400, 2400c, 3400, 3400c"                }, // Comet, Hooper
    { 0x077d36f5, 4096, 0x2560f229,           0, 0,    "Boot PSX 0.1p..]", 0, 0, nullptr   , nullptr                      , "PowerBook G3 Kanga"                                },
    { 0x077d39b7, 4096, 0x4604518f,           0, 0,    "Boot PEX 0.1p..]", 0, 0, nullptr   , nullptr                      , "PowerExpress TriPEx"                               },
    { 0x077d39f1, 4096, 0x46001f1b,           0, 0,    "Boot PEX 0.1p..]", 0, 0, nullptr   , nullptr                      , "Power Express (9700 Prototype)"                    },
    { 0x077d3af2, 4096, 0x58f03416,           0, 0,    "Boot Zanzibar 0.", 0, 0, "pm4400"  , "Power Mac 4400/7220"        , "Motorola 4400, 7220"                               }, // Zanzibar
    { 0x077d40f2, 4096, 0x79d68d63,           0, 0,    "Boot Gossamer 0.", 0, 0, "pmg3dt"  , "Power Mac G3 Beige"         , "Power Mac G3 desktop"                              }, // Gossamer
    { 0x077d41f5, 4096, 0xcbb01212,           0, 0,    "Boot GRX 0.1p..]", 0, 0, "pbg3"    , "PowerBook G3 Wallstreet"    , "PowerBook G3 Wallstreet"                           },
    { 0x077d41f6, 4096, 0xb46ffb63,           0, 0,    "Boot GRX 0.1p..]", 0, 0, "pbg3"    , "PowerBook G3 Wallstreet"    , "PowerBook G3 Wallstreet PDQ"                       },
    { 0x077d45f1, 4096, 0x78fdb784,           0, 0,    "Boot Gossamer 0.", 0, 0, "pmg3dt"  , "Power Mac G3 Beige"         , "PowerMac G3 Minitower (beige 266MHz), Beige G3 233"}, // Gossamer
    { 0x077d45f2, 4096, 0x78f57389,           0, 0,    "Boot Gossamer 0.", 0, 0, "pmg3dt"  , "Power Mac G3 Beige"         , "Power Mac G3 (v3)"                                 }, // Gossamer
    { 0x077d45f3, 4096, 0x78e842a8,           0, 0,    "Boot Gossamer 0.", 0, 0, "pmg3dt"  , "Power Mac G3 Beige"         , "Power Mac G3 (v4)"                                 }, // Gossamer ?
    { 0x077d45f3, 4096, 0x78eb4234,           0, 0,    "Boot Gossamer 0.", 0, 0, "pmg3dt"  , "Power Mac G3 Beige"         , "Power Mac G3 (v4)"                                 }, // Gossamer Speedbump 9cc0e3e01bb02691b497d792ea3e9403

    { 0x077d44f1, 4096, 0xfd86d120,           0, 0,    "NewWorld v1.0.p.", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 1.1"                               }, // 1998-07-21 - Mac OS 8.1 (iMac, Rev A Bundle)
    { 0x077d44f3, 4096, 0xfd12b69e,           0, 0,    "NewWorld v1.0.p.", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 1.1.2"                             }, // 1998-08-27 - Mac OS 8.5 (Retail CD), iMac Update 1.0
    { 0x077d44f4, 4096, 0xfcaad843,           0, 0,    "NewWorld v1.0.p.", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 1.1.5"                             }, // 1998-09-19 - Mac OS 8.5 (iMac, Rev B Bundle)
    { 0x077d44f1, 4096, 0xd36ba902,           0, 0,    "NewWorld v1.0.p.", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 1.2"                               }, // 1998-12-03 - Power Macintosh G3 (Blue and White) Mac OS 8.5.1 Bundle, Macintosh Server G3 (Blue and White) Mac OS 8.5.1 Bundle
    { 0x077d44f1, 4096, 0xd377adb7,           0, 0,    "NewWorld v1.0.p.", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 1.2.1"                             }, // 1999-01-22 - Mac OS 8.5.1 (Colors iMac 266 MHz Bundle), iMac Update 1.1
    { 0x077d44b5, 4096, 0xc804f7f4,           0, 0,    "NewWorld v1.0.p.", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 1.4"                               }, // 1999-04-05 - Mac OS 8.6 (Retail CD), Mac OS 8.6 (Colors iMac 333 MHz Bundle), Power Macintosh G3 (Blue and White) Mac OS 8.6 Bundle
    { 0x077d44b5, 4096, 0xc7cb0323,           0, 0,    "NewWorld v1.0.p.", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 1.6"                               }, // 1999-05-14 - Macintosh PowerBook G3 Series 8.6 Bundle, Mac OS ROM Update 1.0
    { 0x077d44f1, 4096, 0xc75c6aab,           0, 0,    "NewWorld v1.0.p.", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 1.7.1"                             }, // 1999-08-23 - Mac OS 8.6 bundled on Power Mac G4 (PCI)
    { 0x077d44f1, 4096, 0xc753c667,           0, 0,    "NewWorld v1.0.p.", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 1.8.1"                             }, // 1999-08-28 - Mac OS 8.6 Power Mac G4 ROM 1.8.1 Update
    { 0x077d45f3, 4096, 0xcde9cda4,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 2.3.1"                             }, // 1999-09-13 - Mac OS 8.6 bundled on iMac (Slot Loading), iBook
    { 0x077d45f3, 4096, 0xce8a3b5c,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 2.5.1"                             }, // 1999-09-17 - Mac OS 8.6 bundled on Power Mac G4 (AGP)
    { 0x077d45f4, 4096, 0xce1fd217,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 3.0"                               }, // 1999-09-27 - Retail Mac OS 9.0 installed on Power Macintosh G3 (Blue and White), Retail Mac OS 9.0 installed on iMac, Mac OS 9.0 bundled on PowerBook G3 Bronze
    { 0x077d45f5, 4096, 0xce1cf7f7,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 3.1.1"                             }, // 1999-10-28 - Mac OS 9.0 bundled on iBook, Mac OS 9.0 bundled on Power Mac G4 (AGP Graphics):iMac (Slot-Loading)
    { 0x077d45f6, 4096, 0xb9eb8c3d,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 3.5"                               }, // 2000-01-29 - Mac OS 9.0.2 bundled on Power Mac G4 (AGP) and iBook, Mac OS 9.0.2 installed on PowerBook (FireWire)
    { 0x077d45f6, 4096, 0xb8c832f3,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 3.6"                               }, // 2000-02-17 - Mac OS 9.0.3 bundled with iMac (Slot Loading)
    { 0x077d45f6, 4096, 0xb8b2c971,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 3.7"                               }, // 2000-03-15 - 9.0.4 Retail CD
    { 0x077d45f6, 4096, 0xb8bea8b3,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 3.8"                               }, // 2000-05-22 - 9.0.4 Ethernet Update
    { 0x077d45f6, 4096, 0xc90b6289,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 4.6.1"                             }, // 2000-06-18 - Mac OS 9.04 Mac OS 9.0.4 bundled on iMac (Summer 2000), Power Mac G4 (Summer 2000)
    { 0x077d45f6, 4096, 0xc92f71d3,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 4.9.1"                             }, // 2000-06-28 - Mac OS 9.0.4 bundled on Power Mac G4 MP (Summer 2000) (CPU software 2.3), Power Mac G4 (Gigabit Ethernet)
    { 0x077d45f6, 4096, 0xc8e1be97,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 5.2.1"                             }, // 2000-07-12 - Mac OS 9.0.4 installed on Power Mac G4 Cube (CPU software 2.4)
    { 0x077d45f6, 4096, 0xce2a2a5b,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 5.3.1"                             }, // 2000-08-14 - Mac OS 9.0.4 bundled on iBook (Summer 2000) (CPU software 2.5)
    { 0x077d45f6, 4096, 0xce1b9fd2,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 5.5.1"                             }, // 2000-08-25 - Mac OS 9.0.4 from International G4 Cube Install CD
    { 0x077d45f6, 4096, 0xe20aa0d0,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 6.1"                               }, // 2000-11-03 - 9.1 Universal Update
    { 0x077d45f6, 4096, 0xeacb3ca4,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 6.7.1"                             }, // 2000-12-01 - Mac OS 9.1 installed on Power Mac G4 (Digital Audio)
    { 0x077d45f6, 4096, 0xea00f1b7,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 7.5.1"                             }, // 2001-02-07 - 9.1 iMac 2001
    { 0x077d45f6, 4096, 0xeece7cd0,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 7.8.1"                             }, // 2001-04-10 - bundled on iBook (Dual USB) (CPU Software 3.5)
    { 0x077d45f6, 4096, 0xeed28047,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 7.9.1"                             }, // 2001-04-24 - Mac OS 9.1 bundled on PowerBook G4
    { 0x077d45f6, 4096, 0xee6bc7d9,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 8.0"                               }, //            - Mac OS 9.2 Power Mac G4 Install CD
    { 0x077d45f6, 4096, 0xed7f9fc2,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 8.3.1"                             }, // 2001-07-18 - Mac OS 9.2 installed on iMac (Summer 2001), Mac OS 9.2 installed on Power Mac G4 (QuickSilver)
    { 0x077d45f6, 4096, 0xed26a1ef,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 8.4"                               }, // 2001-07-30 - Mac OS 9.2.1 Update CD
    { 0x077d45f6, 4096, 0xec849611,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 8.6.1"                             }, // 2001-09-25 - Mac OS 9.2.1 bundled on iBook G3 (Late 2001)
    { 0x077d45f6, 4096, 0xecc44a65,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 8.7"                               }, // 2001-11-07 - Mac OS 9.2.2 Update SMI
    { 0x077d45f6, 4096, 0xec96aeb6,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 8.8"                               }, // 2001-11-26 - Mac OS 9.2.2 Update CD
    { 0x077d45f6, 4096, 0xec93ab73,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 8.9.1"                             }, // 2001-12-11 - Mac OS 9.2.2 bundled on iBook (CPU Software 4.4)
    { 0x077d45f6, 4096, 0xec86128e,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 9.0.1"                             }, // 2001-12-19 - Mac OS 9.2.2 bundled on  iMac (2001)
    { 0x077d45f6, 4096, 0xecef6af1,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 9.1.1"                             }, // 2002-04-08 - Mac OS 9.2.2 bundled on iMac G4
    { 0x077d45f6, 4096, 0xecc6f29a,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 9.2.1"                             }, // 2002-04-17 - Mac OS 9.2.2 bundled on eMac (CPU Software 4.9)
    { 0x077d45f6, 4096, 0xecd3453f,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 9.3.1"                             }, // 2002-04-18 - Mac OS 9.2.2 bundled on PowerBook G4 (CPU Software 5.0)
    { 0x077d45f6, 4096, 0xecaf0460,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 9.5.1"                             }, // 2002-07-18 - Mac OS 9.2.2 bundled on iMac (17" Flat Panel) (CPU Software 5.3)
    { 0x077d45f6, 4096, 0xecbd9bd2,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 9.6.1"                             }, // 2002-09-03 - Mac OS 9.2.2 (CPU Software 5.4)
    { 0x077d45f6, 4096, 0xecb7c4f9,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 9.7.1"                             }, // 2002-10-11 - Mac OS 9.2.2 bundled on PowerBook (Titanium, 1GHz)
    { 0x077d45f6, 4096, 0xecb96443,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 9.8.1"                             }, // 2003-01-10 - Mac OS 9.2.2
    { 0x077d45f6, 4096, 0xecb8e951,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 10.1.1"                            }, // 2003-03-17 - Mac OS 9.2.2 bundled on eMac 800MHz (CPU Software 5.7)
    { 0x077d45f6, 4096, 0xecb73ad5,           0, 0,    "NewWorld v1.0...", 0, 0, nullptr   , "NewWorld Mac"               , "Mac OS ROM file 10.2.1"                            }, // 2003-04-03 - Mac OS 9.2.2 Retail International CD

    {     0x10f1, 1024, 0,        0,          0, 0, nullptr       , nullptr    , nullptr   , nullptr                      , "PowerBook G3 Lombard"                              }, // PowerBook1,1
    {     0x11f4, 1024, 0,        0,          0, 0, nullptr       , nullptr    , "pmg3nw"  , "Power Mac Yosemite"         , "Power Mac B&W G3"                                  }, // PowerMac1,1
    {     0x13f2, 1024, 0,        0,          0, 0, nullptr       , nullptr    , "imacg3"  , "iMac G3 Bondi"              , "iMac (233 MHz) (Bondi Blue)"                       }, // iMac,1
    {     0x13f3, 1024, 0,        0,          0, 0, nullptr       , nullptr    , "imacg3"  , "iMac G3 Tray Loading"       , "iMac (266,333 MHz) (Tray Loading)"                 }, // iMac,1

    {          0, 1024, 0, 0x008100, 0x266f2e55, 0, "Kihei"       , "P7"       , nullptr   , nullptr                      , "iMac G3 (Slot Loading)"                            }, // PowerMac2,1 // 2001-09-14 419f1
    {          0, 1024, 0, 0x008100, 0x55402f54, 0, "Kihei"       , "P7"       , nullptr   , nullptr                      , "iMac G3 (Slot Loading)"                            }, // PowerMac2,1
    {          0, 1024, 0, 0x008100, 0xf88e2d56, 0, "P7"          , "P7"       , nullptr   , nullptr                      , "iMac G3 (Slot Loading)"                            }, // PowerMac2,1
    {          0, 1024, 0, 0x008200, 0x141d2d96, 0, "P51"         , "P51"      , nullptr   , nullptr                      , "iMac G3 (Summer 2000)"                             }, // PowerMac2,2
    {          0, 1024, 0, 0x008200, 0x41ef2e95, 0, "Perigee"     , "P51"      , nullptr   , nullptr                      , "iMac G3 (Summer 2000)"                             }, // PowerMac2,2
    {          0, 1024, 0, 0x008201, 0x4a862e17, 0, "P51_15"      , "P51"      , nullptr   , nullptr                      , "iMac G3 (Summer 2000)"                             }, // PowerMac2,2
    {          0, 1024, 0, 0x008201, 0x78582f16, 0, "Perigee_15"  , "P51"      , nullptr   , nullptr                      , "iMac G3 (Summer 2000)"                             }, // PowerMac2,2
    {          0, 1024, 0, 0x010100,          0, 0, nullptr       , "P52"      , nullptr   , nullptr                      , "iMac G3 (2001)"                                    }, // PowerMac4,1
    {          0, 1024, 0, 0x010101, 0x9a7a2c2c, 0, "P52"         , nullptr    , nullptr   , nullptr                      , "iMac G3 (2001)"                                    }, // PowerMac4,1
    {          0, 1024, 0, 0x010101, 0xc84c2d2b, 0, "Apogee"      , nullptr    , nullptr   , nullptr                      , "iMac G3 (2001)"                                    }, // PowerMac4,1
    {          0, 1024, 0, 0x010200, 0xe27f2d68, 0, "Tessera"     , "P80"      , nullptr   , nullptr                      , "iMac G4 (Flat Panel)"                              }, // PowerMac4,2
    {          0, 1024, 0, 0x010202, 0xc32928ab, 0, "P80"         , nullptr    , nullptr   , nullptr                      , "iMac G4 (Flat Panel)"                              }, // PowerMac4,2
    {          0, 1024, 0, 0x010202, 0xe3512d6a, 0, "Insp"        , nullptr    , nullptr   , nullptr                      , "iMac G4 (Flat Panel)"                              }, // PowerMac4,2
    {          0, 1024, 0, 0x010202, 0xfaf12da6, 0, "P80|Insp"    , nullptr    , nullptr   , nullptr                      , "iMac G4 (Flat Panel)"                              }, // PowerMac4,2 // 2002-04-08 440f1
    {          0, 1024, 0, 0x010203,          0, 0, nullptr       , "P80"      , nullptr   , nullptr                      , "iMac G4 (Flat Panel)"                              }, // PowerMac4,2
    {          0, 1024, 0, 0x010300, 0xe27f2d68, 0, "Infinity"    , nullptr    , nullptr   , nullptr                      , nullptr                                             }, // PowerMac4,3
    {          0, 1024, 0, 0x010400, 0xa0972cec, 0, "Beyond"      , "P62"      , nullptr   , nullptr                      , "eMac G4"                                           }, // PowerMac4,4
    {          0, 1024, 0, 0x010400, 0xa7cd2b85, 0, "P62"         , "P62"      , nullptr   , nullptr                      , "eMac G4"                                           }, // PowerMac4,4
    {          0, 1024, 0, 0x010400, 0xe72d2d73, 0, "NorthnLites" , "P62"      , nullptr   , nullptr                      , "eMac G4"                                           }, // PowerMac4,4
    {          0, 1024, 0, 0x010401,          0, 0, nullptr       , "P86"      , nullptr   , nullptr                      , "eMac G4"                                           }, // PowerMac4,4
    {          0, 1024, 0, 0x010402,          0, 0, nullptr       , "P86"      , nullptr   , nullptr                      , "eMac G4"                                           }, // PowerMac4,4
    {          0, 1024, 0, 0x010500, 0xa90624c6, 0, "P79"         , "P79"      , nullptr   , nullptr                      , "iMac G4 17 inch (Flat Panel)"                      }, // PowerMac4,5
    {          0, 1024, 0, 0x010500, 0xf1332daa, 0, "Taliesin"    , "P79"      , nullptr   , nullptr                      , "iMac G4 17 inch (Flat Panel)"                      }, // PowerMac4,5
    {          0, 1024, 0, 0x010500, 0xd6d825c5, 0, "P79|Taliesin", "P79"      , nullptr   , nullptr                      , "iMac G4 17 inch (Flat Panel)"                      }, // PowerMac4,5 // 2002-07-23 445f3
    {          0, 1024, 0, 0x018101,          0, 0, nullptr       , "Q26"      , nullptr   , nullptr                      , "iMac G4/1.0 17 inch (Flat Panel)"                  }, // PowerMac6,1
    {          0, 1024, 0, 0x018102, 0xcd1f2ca7, 0, "P87"         , "Q26"      , nullptr   , nullptr                      , "iMac G4/1.0 17 inch (Flat Panel)"                  }, // PowerMac6,1
    {          0, 1024, 0, 0x018301,          0, 0, nullptr       , "Q59"      , nullptr   , nullptr                      , "iMac G4/1.0 (Flat Panel - USB 2.0)"                }, // PowerMac6,3
    {          0, 1024, 0, 0x018401,          0, 0, nullptr       , "Q86"      , nullptr   , nullptr                      , "eMac G4 (2005)"                                    }, // PowerMac6,4
    {          0, 1024, 0, 0x018402,          0, 0, nullptr       , "Q86"      , nullptr   , nullptr                      , "eMac G4 (2005)"                                    }, // PowerMac6,4
    {          0, 1024, 0, 0x018403,          0, 0, nullptr       , "Q86"      , nullptr   , nullptr                      , "eMac G4 (2005)"                                    }, // PowerMac6,4
    {          0, 1024, 0, 0x020101, 0xfcaf4eb7, 0, "Q45"         , "Q45"      , nullptr   , nullptr                      , "iMac G5"                                           }, // PowerMac8,1
    {          0, 1024, 0, 0x020101, 0xfd1f4eb8, 0, "Q45"         , "Q45"      , nullptr   , nullptr                      , "iMac G5"                                           }, // PowerMac8,1
    {          0, 1024, 0, 0x020102,          0, 0, nullptr       , "Q45"      , nullptr   , nullptr                      , "iMac G5"                                           }, // PowerMac8,1
    {          0, 1024, 0, 0x020109, 0x00064ebf, 0, "Q45p"        , nullptr    , nullptr   , nullptr                      , "iMac G5"                                           }, // PowerMac8,1
    {          0, 1024, 0, 0x020109, 0x00764ec0, 0, "Q45p"        , nullptr    , nullptr   , nullptr                      , "iMac G5"                                           }, // PowerMac8,1
    {          0, 1024, 0, 0x020109, 0x24372c87, 0, "Q45p"        , nullptr    , nullptr   , nullptr                      , "iMac G5"                                           }, // PowerMac8,1
    {          0, 1024, 0, 0x020109, 0x24932c8b, 0, "Q45p"        , nullptr    , nullptr   , nullptr                      , "iMac G5"                                           }, // PowerMac8,1
    {          0, 1024, 0, 0x020201,          0, 0, nullptr       , "Q45C"     , nullptr   , nullptr                      , "iMac G5 (Ambient Light Sensor)"                    }, // PowerMac8,2
    {          0, 1024, 0, 0x020f01, 0x20ef2c7f, 0, "Q45xa"       , nullptr    , nullptr   , nullptr                      , nullptr                                             }, // PowerMac8,15
    {          0, 1024, 0, 0x020f01, 0x214b2c83, 0, "Q45xa"       , nullptr    , nullptr   , nullptr                      , nullptr                                             }, // PowerMac8,15
    {          0, 1024, 0, 0x020f01, 0xeace56ae, 0, "Neoa"        , nullptr    , nullptr   , nullptr                      , nullptr                                             }, // PowerMac8,15
    {          0, 1024, 0, 0x020f02, 0x21582c80, 0, "Q45xb"       , nullptr    , nullptr   , nullptr                      , nullptr                                             }, // PowerMac8,15
    {          0, 1024, 0, 0x020f02, 0x21b42c84, 0, "Q45xb"       , nullptr    , nullptr   , nullptr                      , nullptr                                             }, // PowerMac8,15
    {          0, 1024, 0, 0x020f02, 0xf92756d2, 0, "Neob"        , nullptr    , nullptr   , nullptr                      , nullptr                                             }, // PowerMac8,15
    {          0, 1024, 0, 0x020f03, 0x21c12c81, 0, "Q45xc"       , nullptr    , nullptr   , nullptr                      , nullptr                                             }, // PowerMac8,15
    {          0, 1024, 0, 0x020f03, 0x221d2c85, 0, "Q45xc"       , nullptr    , nullptr   , nullptr                      , nullptr                                             }, // PowerMac8,15
    {          0, 1024, 0, 0x020f03, 0x9e7f55ef, 0, "Neoc"        , nullptr    , nullptr   , nullptr                      , nullptr                                             }, // PowerMac8,15
    {          0, 1024, 0, 0x028101, 0xf4dc2533, 0, nullptr       , "Q88"      , nullptr   , nullptr                      , "Mac mini G4"                                       }, // PowerMac10,1 // 2005-03-23 489f4
    {          0, 1024, 0, 0x028201, 0xf4dc2533, 0, nullptr       , "Q88"      , nullptr   , nullptr                      , "Mac mini G4 1.5GHz Radeon 9200"                    }, // PowerMac10,2 // 2005-07-12 494f1
    {          0, 1024, 0, 0x030101,          0, 0, nullptr       , "M23"      , nullptr   , nullptr                      , "iMac G5 (iSight)"                                  }, // PowerMac12,1
    {          0, 1024, 0, 0x108100, 0x71fd2fc9, 0, "P1"          , "P1"       , nullptr   , nullptr                      , "iBook G3 (Original/Clamshell)"                     }, // PowerBook2,1
    {          0, 1024, 0, 0x108100, 0x9fcf30c8, 0, "P1"          , "P1"       , nullptr   , nullptr                      , "iBook G3 (Original/Clamshell)"                     }, // PowerBook2,1
    {          0, 1024, 0, 0x108100, 0xcea031c7, 0, "P1"          , "P1"       , nullptr   , nullptr                      , "iBook G3 (Original/Clamshell)"                     }, // PowerBook2,1
    {          0, 1024, 0, 0x108101, 0x72902fcb, 0, "P1_05"       , "P1"       , nullptr   , nullptr                      , "iBook G3 (Original/Clamshell)"                     }, // PowerBook2,1
    {          0, 1024, 0, 0x108101, 0xa06230ca, 0, "P1_05"       , "P1"       , nullptr   , nullptr                      , "iBook G3 (Original/Clamshell)"                     }, // PowerBook2,1 // 2001-03-20 417f4
    {          0, 1024, 0, 0x108101, 0xcf3331c9, 0, "P1_05"       , "P1"       , nullptr   , nullptr                      , "iBook G3 (Original/Clamshell)"                     }, // PowerBook2,1
    {          0, 1024, 0, 0x108102, 0x7de22ffd, 0, "P1_1"        , "P1"       , nullptr   , nullptr                      , "iBook G3 (Original/Clamshell)"                     }, // PowerBook2,1
    {          0, 1024, 0, 0x108102, 0xabb430fc, 0, "P1_1"        , "P1"       , nullptr   , nullptr                      , "iBook G3 (Original/Clamshell)"                     }, // PowerBook2,1
    {          0, 1024, 0, 0x108102, 0xda8531fb, 0, "P1_1"        , "P1"       , nullptr   , nullptr                      , "iBook G3 (Original/Clamshell)"                     }, // PowerBook2,1
    {          0, 1024, 0, 0x108200, 0x7bdc2fd9, 0, "P1_5"        , "P1_5"     , nullptr   , nullptr                      , "iBook G3 366 MHz CD (Firewire/Clamshell)"          }, // PowerBook2,2
    {          0, 1024, 0, 0x108200, 0xa9ae30d8, 0, "Midway"      , "P1_5"     , nullptr   , nullptr                      , "iBook G3 366 MHz CD (Firewire/Clamshell)"          }, // PowerBook2,2
    {          0, 1024, 0, 0x108201, 0x9745301a, 0, "P1_5DVD"     , "P1_5"     , nullptr   , nullptr                      , "iBook G3 466 MHz DVD (Firewire/Clamshell)"         }, // PowerBook2,2
    {          0, 1024, 0, 0x108201, 0xc5173119, 0, "MidwayDVD"   , "P1_5"     , nullptr   , nullptr                      , "iBook G3 466 MHz DVD (Firewire/Clamshell)"         }, // PowerBook2,2
    {          0, 1024, 0, 0x110100, 0x5f1c2fe5, 0, "Marble"      , "P29"      , nullptr   , nullptr                      , "iBook G3 (Dual USB Snow)"                          }, // PowerBook4,1
    {          0, 1024, 0, 0x110100, 0x69e42f6e, 0, "P29"         , "P29"      , nullptr   , nullptr                      , "iBook G3 (Dual USB Snow)"                          }, // PowerBook4,1
    {          0, 1024, 0, 0x110100, 0x97b6306d, 0, "Marble"      , "P29"      , nullptr   , nullptr                      , "iBook G3 (Dual USB Snow)"                          }, // PowerBook4,1
    {          0, 1024, 0, 0x110101, 0x44852fa6, 0, "MarbleLite"  , nullptr    , nullptr   , nullptr                      , "iBook G3 (Dual USB Snow)"                          }, // PowerBook4,1
    {          0, 1024, 0, 0x110102, 0x60192fe8, 0, "MarbleFat"   , "P29"      , nullptr   , nullptr                      , "iBook G3 (Dual USB Snow)"                          }, // PowerBook4,1
    {          0, 1024, 0, 0x110102, 0x6b1d2f73, 0, "P29Fat"      , "P29"      , nullptr   , nullptr                      , "iBook G3 (Dual USB Snow)"                          }, // PowerBook4,1
    {          0, 1024, 0, 0x110102, 0x98ef3072, 0, "MarbleFat"   , "P29"      , nullptr   , nullptr                      , "iBook G3 (Dual USB Snow)"                          }, // PowerBook4,1
    {          0, 1024, 0, 0x110103, 0x58762f44, 0, "P29fat100"   , "P29"      , nullptr   , nullptr                      , "iBook G3 (Dual USB Snow)"                          }, // PowerBook4,1
    {          0, 1024, 0, 0x110103, 0x86483043, 0, "Mrblfat100"  , "P29"      , nullptr   , nullptr                      , "iBook G3 (Dual USB Snow)"                          }, // PowerBook4,1
    {          0, 1024, 0, 0x110200, 0x4c372fb6, 0, "Diesel"      , "P54"      , nullptr   , nullptr                      , "iBook G3/600 14-Inch (Early 2002 Snow)"            }, // PowerBook4,2
    {          0, 1024, 0, 0x110200, 0x573b2f41, 0, "P54"         , "P54"      , nullptr   , nullptr                      , "iBook G3/600 14-Inch (Early 2002 Snow)"            }, // PowerBook4,2
    {          0, 1024, 0, 0x110300, 0x58e12d5c, 0, "P72"         , "P72"      , nullptr   , nullptr                      , "iBook G3 (Snow)"                                   }, // PowerBook4,3 // 2002-11-11 454f1
    {          0, 1024, 0, 0x110300, 0xb98a30be, 0, "Nectr"       , "P72"      , nullptr   , nullptr                      , "iBook G3 (Snow)"                                   }, // PowerBook4,3
    {          0, 1024, 0, 0x110301, 0x594a2d5d, 0, "P73"         , "P73"      , nullptr   , nullptr                      , "iBook G3 (Snow)"                                   }, // PowerBook4,3
    {          0, 1024, 0, 0x110302, 0x59b32d5e, 0, "P72x"        , "P73"      , nullptr   , nullptr                      , "iBook G3 (Snow)"                                   }, // PowerBook4,3 // 2003-03-15 464f1
    {          0, 1024, 0, 0x110302, 0x3eb82d1d, 0, "P72x"        , "P73"      , nullptr   , nullptr                      , "iBook G3 (Snow)"                                   }, // PowerBook4,3
    {          0, 1024, 0, 0x110303, 0x3f212d1e, 0, "P73x"        , "P73"      , nullptr   , nullptr                      , "iBook G3 (Snow)"                                   }, // PowerBook4,3
    {          0, 1024, 0, 0x118101,          0, 0, nullptr       , "P99"      , nullptr   , nullptr                      , "PowerBook G4 (Aluminum)"                           }, // PowerBook6,1
    {          0, 1024, 0, 0x118202,          0, 0, nullptr       , "Q54"      , nullptr   , nullptr                      , "PowerBook G4 1.0 12 inch (DVI - Aluminum)"         }, // PowerBook6,2
    {          0, 1024, 0, 0x118302,          0, 0, nullptr       , "P72D"     , nullptr   , nullptr                      , "iBook G4 (Original - Opaque)"                      }, // PowerBook6,3
    {          0, 1024, 0, 0x11830c,          0, 0, nullptr       , "P73D"     , nullptr   , nullptr                      , "iBook G4 (Original - Opaque)"                      }, // PowerBook6,3
    {          0, 1024, 0, 0x118402,          0, 0, nullptr       , "Q54A"     , nullptr   , nullptr                      , "PowerBook G4 1.33 12 inch (Aluminum)"              }, // PowerBook6,4
    {          0, 1024, 0, 0x118502, 0x033929a6, 0, nullptr       , "Q72"      , nullptr   , nullptr                      , "iBook G4 (Early 2004)"                             }, // PowerBook6,5 // 2004-04-06 485f0
    {          0, 1024, 0, 0x118504,          0, 0, nullptr       , "Q72A"     , nullptr   , nullptr                      , "iBook G4"                                          }, // PowerBook6,5
    {          0, 1024, 0, 0x118509,          0, 0, nullptr       , "Q73"      , nullptr   , nullptr                      , "iBook G4"                                          }, // PowerBook6,5
    {          0, 1024, 0, 0x11850b, 0x067f29b0, 0, nullptr       , "Q73A"     , nullptr   , nullptr                      , "iBook G4"                                          }, // PowerBook6,5 // 2004-09-23 487f1
    {          0, 1024, 0, 0x118603,          0, 0, nullptr       , "U210"     , nullptr   , nullptr                      , nullptr                                             }, // PowerBook6,6
    {          0, 1024, 0, 0x118701,          0, 0, nullptr       , "Q72B"     , nullptr   , nullptr                      , "iBook G4 12-Inch (Mid-2005 - Opaque)"              }, // PowerBook6,7
    {          0, 1024, 0, 0x118709,          0, 0, nullptr       , "Q73B"     , nullptr   , nullptr                      , "iBook G4 12-Inch (Mid-2005 - Opaque)"              }, // PowerBook6,7
    {          0, 1024, 0, 0x11870c,          0, 0, nullptr       , "Q73B-Best", nullptr   , nullptr                      , "iBook G4 12-Inch (Mid-2005 - Opaque)"              }, // PowerBook6,7
    {          0, 1024, 0, 0x118801,          0, 0, nullptr       , "Q54B"     , nullptr   , nullptr                      , "PowerBook G4 1.5 12 inch (Aluminum)"               }, // PowerBook6,8
    {          0, 1024, 0, 0x20c100, 0x85e72bd1, 0, "P5"          , "P5"       , nullptr   , nullptr                      , "Power Mac G4 (AGP Graphics) Sawtooth"              }, // PowerMac3,1
    {          0, 1024, 0, 0x20c100, 0xb3b92cd0, 0, "Sawtooth"    , "P5"       , nullptr   , nullptr                      , "Power Mac G4 (AGP Graphics) Sawtooth"              }, // PowerMac3,1
    {          0, 1024, 0, 0x20c100, 0xe28a2dcf, 0, "Sawtooth"    , "P5"       , nullptr   , nullptr                      , "Power Mac G4 (AGP Graphics) Sawtooth"              }, // PowerMac3,1
    {          0, 1024, 0, 0x20c101, 0x8d142cda, 0, "Mystic"      , "P5"       , nullptr   , nullptr                      , "Power Mac G4 (AGP Graphics) Sawtooth"              }, // PowerMac3,1 // 2000-02-17 324f1
    {          0, 1024, 0, 0x20c101, 0x5e432bdb, 0, "Mystic"      , "P5"       , nullptr   , nullptr                      , "Power Mac G4 (AGP Graphics) Sawtooth"              }, // PowerMac3,1 // 2001-10-11 428f1
    {          0, 1024, 0, 0x20c101, 0x30712adc, 0, "P10"         , "P5"       , nullptr   , nullptr                      , "Power Mac G4 (AGP Graphics) Sawtooth"              }, // PowerMac3,1
    {          0, 1024, 0, 0x20c300, 0x66752b5c, 0, "P15"         , "P5"       , nullptr   , nullptr                      , "Power Macintosh Mac G4 (Gigabit)"                  }, // PowerMac3,3
    {          0, 1024, 0, 0x20c300, 0x94472c5b, 0, "Clockwork"   , "P5"       , nullptr   , nullptr                      , "Power Macintosh Mac G4 (Gigabit)"                  }, // PowerMac3,3
    {          0, 1024, 0, 0x20c400, 0x47fe2da3, 0, "P21"         , "P21"      , nullptr   , nullptr                      , "Power Mac G4 (Digital Audio)"                      }, // PowerMac3,4
    {          0, 1024, 0, 0x20c400, 0x75d02ea2, 0, "Tangent"     , "P21"      , nullptr   , nullptr                      , "Power Mac G4 (Digital Audio)"                      }, // PowerMac3,4 // 2001-10-11 428f1
    {          0, 1024, 0, 0x20c400, 0x6ea22e91, 0, "P21|Tangent" , "P21"      , nullptr   , nullptr                      , "Power Mac G4 (Digital Audio)"                      }, // PowerMac3,4 // 2000-12-04 410f1
    {          0, 1024, 0, 0x20c500, 0x4b5e2dab, 0, "P57"         , "P57"      , nullptr   , nullptr                      , "Power Mac G4 Quicksilver"                          }, // PowerMac3,5
    {          0, 1024, 0, 0x20c500, 0x75d02ea2, 0, "NiChrome"    , "P57"      , nullptr   , nullptr                      , "Power Mac G4 Quicksilver"                          }, // PowerMac3,5
    {          0, 1024, 0, 0x20c500, 0x79302eaa, 0, "NiChrome"    , "P57"      , nullptr   , nullptr                      , "Power Mac G4 Quicksilver"                          }, // PowerMac3,5 // 2001-08-16 425f1
    {          0, 1024, 0, 0x20c600, 0x6e5a2d67, 0, "P58_133"     , "P58"      , nullptr   , nullptr                      , "Power Mac G4 (Mirrored Drive Doors)"               }, // PowerMac3,6 // 2002-09-30 448f2
    {          0, 1024, 0, 0x20c600, 0x79302eaa, 0, "Moj"         , "P58"      , nullptr   , nullptr                      , "Power Mac G4 (Mirrored Drive Doors)"               }, // PowerMac3,6
    {          0, 1024, 0, 0x20c601, 0x20df2ca4, 0, "P58_167"     , "P58"      , nullptr   , nullptr                      , "Power Mac G4 (Mirrored Drive Doors)"               }, // PowerMac3,6
    {          0, 1024, 0, 0x20c602, 0x6f2c2d69, 0, nullptr       , "P58"      , nullptr   , nullptr                      , "Power Mac G4 (FW 800)"                             }, // PowerMac3,6 // 2003-01-15 457f1
    {          0, 1024, 0, 0x20c603, 0x21b12ca6, 0, nullptr       , "P58"      , nullptr   , nullptr                      , "Power Mac G4 (FW 800)"                             }, // PowerMac3,6 // 2003-02-20 460f1
    {          0, 1024, 0, 0x214100, 0x4af52b1c, 0, "P9"          , "P9"       , nullptr   , nullptr                      , "Power Mac G4 Cube"                                 }, // PowerMac5,1
    {          0, 1024, 0, 0x214100, 0x78c72c1b, 0, "Trinity"     , "P9"       , nullptr   , nullptr                      , "Power Mac G4 Cube"                                 }, // PowerMac5,1 // 2000-07-10 332f1 // 2001-09-14 419f1
    {          0, 1024, 0, 0x214100, 0x78c72c1b, 0, "Kubrick"     , "P9"       , nullptr   , nullptr                      , "Power Mac G4 Cube"                                 }, // PowerMac5,1 // 2000-07-10 332f1 // 2001-09-14 419f1
    {          0, 1024, 0, 0x214100, 0x8cab2cd9, 0, "Kubrick"     , "P9"       , nullptr   , nullptr                      , "Power Mac G4 Cube"                                 }, // PowerMac5,1
    {          0, 1024, 0, 0x21c200, 0x25142c89, 0, "Q37high"     , "Q37"      , nullptr   , nullptr                      , "Power Mac G5 1.6 (PCI)"                            }, // PowerMac7,2
    {          0, 1024, 0, 0x21c200, 0x25702c8d, 0, "Q37high"     , "Q37"      , nullptr   , nullptr                      , "Power Mac G5 1.6 (PCI)"                            }, // PowerMac7,2
    {          0, 1024, 0, 0x21c200, 0xa5b7555f, 0, "Q37high"     , "Q37"      , nullptr   , nullptr                      , "Power Mac G5 1.6 (PCI)"                            }, // PowerMac7,2
    {          0, 1024, 0, 0x21c201, 0x336d2cad, 0, "Q37med"      , "Q37"      , nullptr   , nullptr                      , "Power Mac G5 1.6 (PCI)"                            }, // PowerMac7,2
    {          0, 1024, 0, 0x21c201, 0x33c92cb1, 0, "Q37med"      , "Q37"      , nullptr   , nullptr                      , "Power Mac G5 1.6 (PCI)"                            }, // PowerMac7,2
    {          0, 1024, 0, 0x21c201, 0x514d2dbb, 0, "P76"         , "Q37"      , nullptr   , nullptr                      , "Power Mac G5 1.6 (PCI)"                            }, // PowerMac7,2
    {          0, 1024, 0, 0x21c201, 0xb4105583, 0, "Q37med"      , "Q37"      , nullptr   , nullptr                      , "Power Mac G5 1.6 (PCI)"                            }, // PowerMac7,2
    {          0, 1024, 0, 0x21c202, 0x596854a0, 0, "Q37low"      , "Q37low"   , nullptr   , nullptr                      , "Power Mac G5 1.6 (PCI)"                            }, // PowerMac7,2
    {          0, 1024, 0, 0x21c202, 0xd8c62bca, 0, "Q37low"      , "Q37low"   , nullptr   , nullptr                      , "Power Mac G5 1.6 (PCI)"                            }, // PowerMac7,2 // 2004-09-21 515f2
    {          0, 1024, 0, 0x21c203, 0x343f2caf, 0, "Q37A"        , "Q37low"   , nullptr   , nullptr                      , "Power Mac G5 1.6 (PCI)"                            }, // PowerMac7,2
    {          0, 1024, 0, 0x21c203, 0x349b2cb3, 0, "Q37A"        , "Q37low"   , nullptr   , nullptr                      , "Power Mac G5 1.6 (PCI)"                            }, // PowerMac7,2
    {          0, 1024, 0, 0x21c203, 0xb4e25585, 0, "Q37A"        , "Q37low"   , nullptr   , nullptr                      , "Power Mac G5 1.6 (PCI)"                            }, // PowerMac7,2
    {          0, 1024, 0, 0x21c204, 0xa8955568, 0, "Q37C"        , "Q77hi"    , nullptr   , nullptr                      , "Power Mac G5 1.6 (PCI)"                            }, // PowerMac7,2
    {          0, 1024, 0, 0x21c204, 0xa930556d, 0, "Q37C"        , "Q77hi"    , nullptr   , nullptr                      , "Power Mac G5 1.6 (PCI)"                            }, // PowerMac7,2
    {          0, 1024, 0, 0x21c301, 0xb6cf558d, 0, "Q77best"     , "Q77hi"    , nullptr   , nullptr                      , "Power Macintosh G5 Dual Processor"                 }, // PowerMac7,3
    {          0, 1024, 0, 0x21c301, 0xb76a5592, 0, "Q77best"     , "Q77hi"    , nullptr   , nullptr                      , "Power Macintosh G5 Dual Processor"                 }, // PowerMac7,3
    {          0, 1024, 0, 0x21c302, 0xa7635563, 0, "Q77mid"      , "Q77"      , nullptr   , nullptr                      , "Power Macintosh G5 Dual Processor"                 }, // PowerMac7,3
    {          0, 1024, 0, 0x21c303, 0xb5bc5587, 0, "Q77good"     , "Q77good"  , nullptr   , nullptr                      , "Power Macintosh G5 Dual Processor"                 }, // PowerMac7,3
    {          0, 1024, 0, 0x21c304, 0xb80a5590, 0, "Q77better"   , "Q77better", nullptr   , nullptr                      , "Power Macintosh G5 Dual Processor"                 }, // PowerMac7,3
    {          0, 1024, 0, 0x21c304, 0xb8a55595, 0, "Q77better"   , "Q77better", nullptr   , nullptr                      , "Power Macintosh G5 Dual Processor"                 }, // PowerMac7,3
    {          0, 1024, 0, 0x21c305,          0, 0, nullptr       , "M18wl"    , nullptr   , nullptr                      , "Power Macintosh G5 Dual Processor"                 }, // PowerMac7,3
    {          0, 1024, 0, 0x21c306,          0, 0, nullptr       , "Q87good"  , nullptr   , nullptr                      , "Power Macintosh G5 Dual Processor"                 }, // PowerMac7,3
    {          0, 1024, 0, 0x21c307,          0, 0, nullptr       , "Q77better", nullptr   , nullptr                      , "Power Macintosh G5 Dual Processor"                 }, // PowerMac7,3
    {          0, 1024, 0, 0x21c308,          0, 0, nullptr       , "Q77hi"    , nullptr   , nullptr                      , "Power Macintosh G5 Dual Processor"                 }, // PowerMac7,3
    {          0, 1024, 0, 0x224102,          0, 0, nullptr       , "Q78"      , nullptr   , nullptr                      , "Power Macintosh G5 1.8 (PCI)"                      }, // PowerMac9,1
    {          0, 1024, 0, 0x224108, 0x84d94d6f, 0, "Q78EVT"      , nullptr    , nullptr   , nullptr                      , "Power Macintosh G5 1.8 (PCI)"                      }, // PowerMac9,1
    {          0, 1024, 0, 0x224109, 0x82b74d9f, 0, "Q78p"        , nullptr    , nullptr   , nullptr                      , "Power Macintosh G5 1.8 (PCI)"                      }, // PowerMac9,1
    {          0, 1024, 0, 0x224109, 0x86774da9, 0, "Q78p"        , nullptr    , nullptr   , nullptr                      , "Power Macintosh G5 1.8 (PCI)"                      }, // PowerMac9,1
    {          0, 1024, 0, 0x22c101,          0, 0, nullptr       , "M18"      , nullptr   , nullptr                      , nullptr                                             }, // PowerMac11,1
    {          0, 1024, 0, 0x22c102,          0, 0, nullptr       , "M20wl"    , nullptr   , nullptr                      , nullptr                                             }, // PowerMac11,1
    {          0, 1024, 0, 0x22c201,          0, 0, nullptr       , "Q63Proto" , nullptr   , nullptr                      , "Power Mac G5 Quad Core Proto"                      }, // PowerMac11,2
    {          0, 1024, 0, 0x22c202, 0xb7fe51fc, 0, nullptr       , "Q63"      , nullptr   , nullptr                      , "Power Mac G5 Quad Core"                            }, // PowerMac11,2 // 2005-09-30 527f1
    {          0, 1024, 0, 0x30c100, 0x0c653168, 0, "P8"          , "P8"       , nullptr   , nullptr                      , "PowerBook G3 (FireWire) Pismo"                     }, // PowerBook3,1
    {          0, 1024, 0, 0x30c100, 0x3a373267, 0, "Pismo"       , "P8"       , nullptr   , nullptr                      , "PowerBook G3 (FireWire) Pismo"                     }, // PowerBook3,1 // 2001-03-21 418f5
    {          0, 1024, 0, 0x30c100, 0x69083366, 0, "Pismo"       , "P8"       , nullptr   , nullptr                      , "PowerBook G3 (FireWire) Pismo"                     }, // PowerBook3,1
    {          0, 1024, 0, 0x30c1ff, 0xcb8e3457, 0, "Pismo66"     , nullptr    , nullptr   , nullptr                      , "PowerBook G3 (FireWire) Pismo"                     }, // PowerBook3,1
    {          0, 1024, 0, 0x30c200,          0, 0, nullptr       , "P12"      , nullptr   , nullptr                      , "PowerBook G4 (Original - Titanium)"                }, // PowerBook3,2
    {          0, 1024, 0, 0x30c201, 0x33b22dc6, 0, "P12"         , "P12"      , nullptr   , nullptr                      , "PowerBook G4 (Original - Titanium)"                }, // PowerBook3,2
    {          0, 1024, 0, 0x30c201, 0x61842ec5, 0, "Mercury"     , "P12"      , nullptr   , nullptr                      , "PowerBook G4 (Original - Titanium)"                }, // PowerBook3,2
    {          0, 1024, 0, 0x30c300, 0x3e4f2dd5, 0, "P25_100"     , "P25"      , nullptr   , nullptr                      , "PowerBook G4 (Gigabit - Titanium)"                 }, // PowerBook3,3
    {          0, 1024, 0, 0x30c300, 0x63762eca, 0, "Onyx"        , "P25"      , nullptr   , nullptr                      , "PowerBook G4 (Gigabit - Titanium)"                 }, // PowerBook3,3
    {          0, 1024, 0, 0x30c300, 0x6c212ed4, 0, "Onix100"     , "P25"      , nullptr   , nullptr                      , "PowerBook G4 (Gigabit - Titanium)"                 }, // PowerBook3,3
    {          0, 1024, 0, 0x30c301, 0x4c2b2df8, 0, "P25"         , "P25"      , nullptr   , nullptr                      , "PowerBook G4 (Gigabit - Titanium)"                 }, // PowerBook3,3
    {          0, 1024, 0, 0x30c301, 0x79fd2ef7, 0, "Onix"        , "P25"      , nullptr   , nullptr                      , "PowerBook G4 (Gigabit - Titanium)"                 }, // PowerBook3,3
    {          0, 1024, 0, 0x30c302, 0x7e6c2f0a, 0, "OnixStar"    , nullptr    , nullptr   , nullptr                      , "PowerBook G4 (Gigabit - Titanium)"                 }, // PowerBook3,3
    {          0, 1024, 0, 0x30c400, 0x7a002ef7, 0, "Ivry"        , "P59"      , nullptr   , nullptr                      , "PowerBook G4 (DVI - Titanium)"                     }, // PowerBook3,4
    {          0, 1024, 0, 0x30c400, 0x91952f11, 0, "P59_667"     , "P59"      , nullptr   , nullptr                      , "PowerBook G4 (DVI - Titanium)"                     }, // PowerBook3,4
    {          0, 1024, 0, 0x30c402, 0x92672f13, 0, "P59_800"     , nullptr    , nullptr   , nullptr                      , "PowerBook G4 (DVI - Titanium)"                     }, // PowerBook3,4
    {          0, 1024, 0, 0x30c403, 0x92d02f14, 0, "P59_DVT"     , nullptr    , nullptr   , nullptr                      , "PowerBook G4 (DVI - Titanium)"                     }, // PowerBook3,4
    {          0, 1024, 0, 0x30c404, 0x93392f15, 0, "P59_DualFan" , "P59DF"    , nullptr   , nullptr                      , "PowerBook G4 (DVI - Titanium)"                     }, // PowerBook3,4
    {          0, 1024, 0, 0x30c500,          0, 0, nullptr       , "P88"      , nullptr   , nullptr                      , "PowerBook G4 (Titanum)"                            }, // PowerBook3,5
    {          0, 1024, 0, 0x30c501,          0, 0, nullptr       , "P881G"    , nullptr   , nullptr                      , "PowerBook G4 (Titanum)"                            }, // PowerBook3,5
    {          0, 1024, 0, 0x314100, 0x6ece5388, 0, "P84i"        , nullptr    , nullptr   , nullptr                      , "PowerBook G4 1.0 17 inch (Aluminum)"               }, // PowerBook5,1
    {          0, 1024, 0, 0x314103, 0x5c13264b, 0, nullptr       , "P84"      , nullptr   , nullptr                      , "PowerBook G4 1.0 17 inch (Aluminum)"               }, // PowerBook5,1 // 2003-02-18 462f1
    {          0, 1024, 0, 0x314202,          0, 0, nullptr       , "Q16-EVT"  , nullptr   , nullptr                      , "PowerBook G4 15 inch (FW 800 - Aluminum)"          }, // PowerBook5,2
    {          0, 1024, 0, 0x314301,          0, 0, nullptr       , "Q41"      , nullptr   , nullptr                      , "PowerBook G4 1.33 17 inch (Aluminum)"              }, // PowerBook5,3
    {          0, 1024, 0, 0x314401,          0, 0, nullptr       , "Q16A"     , nullptr   , nullptr                      , "PowerBook G4 15 inch (Aluminum)"                   }, // PowerBook5,4
    {          0, 1024, 0, 0x314501,          0, 0, nullptr       , "Q41A"     , nullptr   , nullptr                      , "PowerBook G4 1.5 17 inch (Aluminum)"               }, // PowerBook5,5
    {          0, 1024, 0, 0x314601,          0, 0, nullptr       , "Q16B"     , nullptr   , nullptr                      , "PowerBook G4 15 inch (Aluminum)"                   }, // PowerBook5,6
    {          0, 1024, 0, 0x314701,          0, 0, nullptr       , "Q41B"     , nullptr   , nullptr                      , "PowerBook G4 1.67 17 inch (Aluminum)"              }, // PowerBook5,7
    {          0, 1024, 0, 0x314801, 0x35c72568, 0, nullptr       , "Q16C"     , nullptr   , nullptr                      , "PowerBook G4 DLSD"                                 }, // PowerBook5,8 // 2005-09-22 495f3
    {          0, 1024, 0, 0x314801,          0, 0, nullptr       , "Q41C"     , nullptr   , nullptr                      , "PowerBook G4 DLSD"                                 }, // PowerBook5,8
    {          0, 1024, 0, 0x314802,          0, 0, nullptr       , "Q16CBest" , nullptr   , nullptr                      , "PowerBook G4 DLSD"                                 }, // PowerBook5,8
    {          0, 1024, 0, 0x314901, 0x35c72568, 0, nullptr       , "Q41C"     , nullptr   , nullptr                      , "PowerBook G4 1.67 17 inch (DLSD/HiRes - Aluminum)" }, // PowerBook5,9 // 2005-10-05 496f0
    {          0, 1024, 0, 0x318100, 0x6ca8272f, 0, "P99"         , nullptr    , nullptr   , nullptr                      , "PowerBook G4 867 12 inch (Aluminum)"               }, // PowerBook6,1
    {          0, 1024, 0, 0x31c101, 0xbb11558a, 0, "Q51p"        , "Q51"      , nullptr   , nullptr                      , nullptr                                             }, // PowerBook7,1
    {          0, 1024, 0, 0x31c101, 0xbc5d558d, 0, "Q51p"        , "Q51"      , nullptr   , nullptr                      , nullptr                                             }, // PowerBook7,1
    {          0, 1024, 0, 0x31c201, 0xbb11558a, 0, "Q43p"        , "Q43"      , nullptr   , nullptr                      , nullptr                                             }, // PowerBook7,2
    {          0, 1024, 0, 0x31c201, 0xbc5d558d, 0, "Q43p"        , "Q43"      , nullptr   , nullptr                      , nullptr                                             }, // PowerBook7,2
    {          0, 1024, 0, 0x380101,          0, 0, nullptr       , "T3"       , nullptr   , nullptr                      , nullptr                                             }, // PowerBook32,1
    {          0, 1024, 0, 0x380201,          0, 0, nullptr       , "M22"      , nullptr   , nullptr                      , nullptr                                             }, // PowerBook32,2
    {          0, 1024, 0, 0x404100, 0x268f2cab, 0, "P69"         , "P69"      , nullptr   , nullptr                      , "Xserve G4 1.0 GHz"                                 }, // RackMac1,1
    {          0, 1024, 0, 0x404200,          0, 0, nullptr       , "Q28"      , nullptr   , nullptr                      , "Xserve G4 1.33 GHz (Slot Load)"                    }, // RackMac1,2 // 465f3
    {          0, 1024, 0, 0x40c100, 0xc2f855a9, 0, "Q42"         , "Q42"      , nullptr   , nullptr                      , "Xserve G5 (PCI-X)"                                 }, // RackMac3,1
    {          0, 1024, 0, 0x40c101, 0xc28755a8, 0, "Q42B"        , "Q42"      , nullptr   , nullptr                      , "Xserve G5 (PCI-X)"                                 }, // RackMac3,1
    {          0, 1024, 0, 0x414101,          0, 0, nullptr       , "Q42C"     , nullptr   , nullptr                      , nullptr                                             }, // RackMac5,1

    { 0 }
};

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
    for (auto& dev_name : it->second.devices) {
        create_device(dev_name, DeviceRegistry::get_descriptor(dev_name));
    }

    if (it->second.init_func(mach_id)) {
        LOG_F(ERROR, "Machine initialization function failed!");
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

        print_settings(mach.second.settings);

        for (auto& d : mach.second.devices) {
            list_device_settings(DeviceRegistry::get_descriptor(d));
        }
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
        auto props = it->second.settings;

        gMachineSettings.clear();

        register_settings(props);

        std::cout << endl << "Machine " << id << " Settings" << endl
                   << std::string(36, '-') << endl;
        for (auto& p : props) {
            std::cout << std::setw(24) << std::right << p.first << " : "
                      << gMachineSettings[p.first]->get_string() << endl;
        }

        for (auto& dev : it->second.devices) {
            register_device_settings(dev);
        }
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
        minor_version = READ_WORD_BE_A(&rom_data[0x12]);
        firmware_version = (major_version << 16) | minor_version;
        ow_checksum_calculated = oldworldchecksum(&rom_data[4], std::min(rom_size - 4, (size_t)0x2ffffc));
        ow_checksum_stored = READ_DWORD_BE_A(&rom_data[0]);
        if (ow_checksum_calculated != ow_checksum_stored)
            snprintf(expected_ow, sizeof(expected_ow), " (expected 0x%04x)", ow_checksum_stored);

        if (firmware_version > 0x077d10f3) {
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
            LOG_F(INFO, "    ROM Version: %04x.%04x", major_version, minor_version);
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
