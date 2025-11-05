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

#include <machines/romidentity.h>

rom_info rom_identity[] = {

// 68K ROMs
#define rom(version, size_k, checksum, dppc_mach, dppc_desc, rom_desc)      \
    { .firmware_version     = version,                                      \
      .firmware_size_k      = size_k,                                       \
      .ow_expected_checksum = checksum,                                     \
      .dppc_machine         = dppc_mach,                                    \
      .dppc_description     = dppc_desc,                                    \
      .rom_description      = rom_desc },

    rom( 0x00690000,   64, 0x28ba61ce, nullptr   , nullptr, "Macintosh 128K"                                                 )
    rom( 0x00690000,   64, 0x28ba4e50, nullptr   , nullptr, "Macintosh 512K"                                                 )
    rom( 0x00750000,  128, 0x4d1eeee1, nullptr   , nullptr, "MacPlus v1"                                                     )
    rom( 0x00750000,  128, 0x4d1eeae1, nullptr   , nullptr, "MacPlus v2"                                                     )
    rom( 0x00750000,  128, 0x4d1f8172, nullptr   , nullptr, "MacPlus v3"                                                     )
    rom( 0x01780000,  256, 0x97221136, nullptr   , nullptr, "Mac II FDHD & IIx & IIcx"                                       )
    rom( 0x01780000,  256, 0x9779d2c4, nullptr   , nullptr, "MacII (800k v2)"                                                )
    rom( 0x01780000,  256, 0x97851db6, nullptr   , nullptr, "MacII (800k v1)"                                                )
    rom( 0x02760000,  256, 0xb2e362a8, nullptr   , nullptr, "Mac SE"                                                         )
    rom( 0x02760000,  256, 0xb306e171, nullptr   , nullptr, "Mac SE FDHD"                                                    )
    rom( 0x02760000,  512, 0xa49f9914, nullptr   , nullptr, "Classic (with XO ROMDisk)"                                      )
    rom( 0x037a0000,  256, 0x96ca3846, nullptr   , nullptr, "Mac Portable"                                                   )
    rom( 0x037a11f1,  256, 0x96645f9c, nullptr   , nullptr, "PowerBook 100"                                                  )
    rom( 0x067c10f1,  512, 0x368cadfe, nullptr   , nullptr, "Mac IIci"                                                       )
    rom( 0x067c11f2,  512, 0x4147dd77, nullptr   , nullptr, "Mac IIfx"                                                       )
    rom( 0x067c12f1,  512, 0x36b7fb6c, nullptr   , nullptr, "Mac IIsi"                                                       )
    rom( 0x067c13f1,  512, 0x350eacf0, nullptr   , nullptr, "Mac LC"                                                         )
    rom( 0x067c15f1, 1024, 0x420dbff3, nullptr   , nullptr, "Quadra 700&900 & PB140&170"                                     )
    rom( 0x067c16f1,  512, 0x3193670e, nullptr   , nullptr, "Classic II"                                                     )
    rom( 0x067c17f2, 1024, 0x3dc27823, nullptr   , nullptr, "Quadra 950"                                                     )
    rom( 0x067c18f1, 1024, 0xe33b2724, nullptr   , nullptr, "Powerbook 160 & 165c & 180 & 180c"                              )
    rom( 0x067c19f2,  512, 0x35c28f5f, nullptr   , nullptr, "Mac LCII"                                                       )
    rom( 0x067c20f2, 1024, 0x4957eb49, nullptr   , nullptr, "MacIIvx & IIvi"                                                 )
    rom( 0x067c21f5, 1024, 0xecfa989b, nullptr   , nullptr, "Powerbook 210,230,250"                                          )
    rom( 0x067c22f2, 1024, 0xec904829, nullptr   , nullptr, "LCIII (older)"                                                  )
    rom( 0x067c22f3, 1024, 0xecbbc41c, nullptr   , nullptr, "Mac LCIII"                                                      )
    rom( 0x067c23f1, 1024, 0xf1a6f343, nullptr   , nullptr, "Centris 610,650, Quadra 800"                                    )
    rom( 0x067c23f2, 1024, 0xf1acad13, nullptr   , nullptr, "Quadra 610,650,maybe 800"                                       )
    rom( 0x067c24f2, 1024, 0xecd99dc0, nullptr   , nullptr, "Color Classic"                                                  )
    rom( 0x067c25f1, 1024, 0xede66cbd, nullptr   , nullptr, "Color Classic II & LC 550 & Performa 275,550,560 & Macintosh TV")
    rom( 0x067c26f1, 1024, 0xff7439ee, nullptr   , nullptr, "Quadra 605"                                                     )
    rom( 0x067c27f2, 1024, 0x0024d346, nullptr   , nullptr, "Powerbook Duo 270"                                              )
    rom( 0x067c29f2, 1024, 0x015621d7, nullptr   , nullptr, "Powerbook 280&280c"                                             )
    rom( 0x067c30f1, 2048, 0xb6909089, nullptr   , nullptr, "PowerBook 520&520c&540&540c"                                    )
    rom( 0x067c30f2, 2048, 0xb57687a5, nullptr   , nullptr, "Pb550c"                                                         )
    rom( 0x067c31f1, 1024, 0xfda22562, nullptr   , nullptr, "Powerbook 150"                                                  )
    rom( 0x067c32f1, 1024, 0x06684214, nullptr   , nullptr, "Quadra 630"                                                     )
    rom( 0x067c32f2, 1024, 0x064dc91d, nullptr   , nullptr, "Performa 580 & 588"                                             )
    rom( 0x077d10f3, 2048, 0x5bf10fd1, nullptr   , nullptr, "Quadra 660av & 840av"                                           )
    rom( 0x077d2bf1, 2048, 0x4d27039c, nullptr   , nullptr, "Powerbook 190cs"                                                )

#undef rom

// PPC Old World ROMs
#define rom(version, checksum, checksum64, id, dppc_mach, dppc_desc, rom_desc) \
    { .firmware_version       = version,                                       \
      .firmware_size_k        = 4096,                                          \
      .ow_expected_checksum   = checksum,                                      \
      .ow_expected_checksum64 = checksum64,                                    \
      .id_str                 = id,                                            \
      .dppc_machine           = dppc_mach,                                     \
      .dppc_description       = dppc_desc,                                     \
      .rom_description        = rom_desc },

    rom( 0x077d20f2, 0x9feb69b3, 0xdedc602cfc3b9221, "Boot PDM 601 1.0", "pm6100"  , "NuBus Power Mac"            , "Power Mac 6100 & 7100 & 8100"                      )
    rom( 0x077d22f1, 0x9c7c98f7, 0xe9220fe5992ffdf2, "Boot PDM 601 1.0", "pm9150"  , "NuBus Power Mac"            , "Workgroup Server 9150-80"                          )
    rom( 0x077d23f1, 0x9b7a3aad, 0xed510ac937721e25, "Boot PDM 601 1.1", "pm7100"  , "NuBus Power Mac"            , "Power Mac 7100 (newer)"                            )
    rom( 0x077d25f1, 0x9b037f6f, 0xd20052bd25d88bd6, "Boot PDM 601 1.1", "pm9150"  , "NuBus Power Mac"            , "Workgroup Server 9150-120"                         )
    rom( 0x077d26f1, 0x63abfd3f, 0xc5421fbaff3c5a9d, "Boot Cordyceps 6", "pm5200"  , "Power Mac 5200/6200 series" , "Power Mac & Performa 5200,5300,6200,6300"          )
    rom( 0x077d28a5, 0x67a1aa96, 0x48877909cdce7677, "..0.....Boot TNT", nullptr   , nullptr                      , "TNT A5c1"                                          )
    rom( 0x077d28f1, 0x96cd923d, 0xc241cd82bf90797a, "Boot TNT 0.1p..]", "pm7200"  , "Power Mac 7xxxx/8xxx series", "Power Mac 7200&7500&8500&9500 v1"                  )
    rom( 0x077d28f2, 0x9630c68b, 0x4db4a42fea3b53b3, "Boot TNT 0.1p..]", "pm7200"  , "Power Mac 7xxxx/8xxx series", "Power Mac 7200&7500&8500&9500 v2, SuperMac S900"   )
    rom( 0x077d28f2, 0x962f6c13, 0xd540b3dd5bcf9caa, "Boot TNT 0.1p..]", nullptr   , "Apple Network Server series", "Apple Network Server 500"                          )
    rom( 0x077d29f1, 0x6f5724c0, 0x6703442013f443d8, "Boot Alchemy 0.1", "pm6400"  , "Performa 6400"              , "PM 5400, Performa 6400"                            )
    rom( 0x077d2af2, 0x83c54f75, 0xc8b9658674ebb5ba, "Boot PBX 603 0.0", "pb-preg3", "PowerBook Pre-G3"           , "Powerbook 2300 & PB5x0 PPC Upgrade"                )
    rom( 0x077d2cc6, 0x2bf65931, 0x2b807a9748e78318, "Boot Pip 0.1p..]", "pippin"  , "Bandai Pippin"              , "Bandai Pippin (Kinka Dev)"                         )
    rom( 0x077d2cf2, 0x2bef21b7, 0x0bf68e274dc0720d, "Boot Pip 0.1p..]", "pippin"  , "Bandai Pippin"              , "Bandai Pippin (Kinka 1.0)"                         )
    rom( 0x077d2cf5, 0x3e10e14c, 0xdced07b8dfabe232, "Boot Pip 0.1p..]", "pippin"  , "Bandai Pippin"              , "Bandai Pippin (Kinka 1.2)"                         )
    rom( 0x077d2cf8, 0x3e6b3ee4, 0xf1c39bbb1627982b, "Boot Pip 0.1p..]", "pippin"  , "Bandai Pippin"              , "Bandai Pippin (Kinka 1.3)"                         )
    rom( 0x077d32f3, 0x838c0831, 0xb99cdb0cd1d6e068, "Boot PBX 603 0.0", "pb-preg3", "PowerBook Pre-G3"           , "PowerBook 1400"                                    )
    rom( 0x077d32f3, 0x83a21950, 0xb470fc5d287e39c1, "Boot PBX 603 0.0", "pb-preg3", "PowerBook Pre-G3"           , "PowerBook 1400cs"                                  )
    rom( 0x077d34f2, 0x960e4be9, 0x949c2c56d07516b7, "Boot TNT 0.1p..]", "pm7300"  , "Power Mac 7xxxx/8xxx series", "Power Mac 7300 & 7600 & 8600 & 9600 (v1)"          )
    rom( 0x077d34f5, 0x960fc647, 0x013bb98cd4ad16c4, "Boot TNT 0.1p..]", "pm8600"  , "Power Mac 7xxxx/8xxx series", "Power Mac 8600 & 9600 (v2)"                        )
    rom( 0x077d35f2, 0x6e92fe08, 0xc784f8035da2d93a, "Boot Gazelle 0.1", "pm6500"  , "Power Mac 6500"             , "Power Mac 6500, Twentieth Anniversary Macintosh"   )
    rom( 0x077d36f1, 0x276ec1f1, 0x05f6c10c61a3760d, "Boot PSX 0.1p..]", nullptr   , nullptr                      , "PowerBook 2400, 2400c, 3400, 3400c"                )
    rom( 0x077d36f5, 0x2560f229, 0xafabd023ad5fc165, "Boot PSX 0.1p..]", nullptr   , nullptr                      , "PowerBook G3 Kanga"                                )
    rom( 0x077d39b7, 0x4604518f, 0xefbc0e0922ebe984, "Boot PEX 0.1p..]", nullptr   , nullptr                      , "PowerExpress TriPEx"                               )
    rom( 0x077d39f1, 0x46001f1b, 0,                  "Boot PEX 0.1p..]", nullptr   , nullptr                      , "Power Express (9700 Prototype)"                    )
    rom( 0x077d3af2, 0x58f03416, 0x5208e606711dd704, "Boot Zanzibar 0.", "pm4400"  , "Power Mac 4400/7220"        , "Motorola 4400, 7220"                               )
    rom( 0x077d40f2, 0x79d68d63, 0x32d284c61fd4f342, "Boot Gossamer 0.", "pmg3dt"  , "Power Mac G3 Beige"         , "Power Mac G3 desktop"                              )
    rom( 0x077d41f5, 0xcbb01212, 0x833504c219032ad5, "Boot GRX 0.1p..]", "pbg3"    , "PowerBook G3 Wallstreet"    , "PowerBook G3 Wallstreet"                           )
    rom( 0x077d41f6, 0xb46ffb63, 0x69e8e6201908f35f, "Boot GRX 0.1p..]", "pbg3"    , "PowerBook G3 Wallstreet"    , "PowerBook G3 Wallstreet PDQ"                       )
    rom( 0x077d45f1, 0x78fdb784, 0xe8cd35c33d1c0b0b, "Boot Gossamer 0.", "pmg3dt"  , "Power Mac G3 Beige"         , "PowerMac G3 Minitower (beige 266MHz), Beige G3 233")
    rom( 0x077d45f2, 0x78f57389, 0x7b8375af2e19914d, "Boot Gossamer 0.", "pmg3dt"  , "Power Mac G3 Beige"         , "Power Mac G3 (v3)"                                 )
    rom( 0x077d45f3, 0x78e842a8, 0x7ce12c8aef3312fe, "Boot Gossamer 0.", "pmg3dt"  , "Power Mac G3 Beige"         , "Power Mac G3 (v4)"                                 )
    rom( 0x077d45f3, 0x78eb4234, 0,                  "Boot Gossamer 0.", "pmg3dt"  , "Power Mac G3 Beige"         , "Power Mac G3 (v4) (no public dump)"                )

#undef rom

// PPC Mac OS ROM files
#define rom(version, checksum, checksum64, id, dppc_mach, rom_desc)         \
    { .firmware_version       = version,                                    \
      .firmware_size_k        = 4096,                                       \
      .ow_expected_checksum   = checksum,                                   \
      .ow_expected_checksum64 = checksum64,                                 \
      .id_str                 = id,                                         \
      .dppc_machine           = dppc_mach,                                  \
      .dppc_description       = "NewWorld Mac",                             \
      .rom_description        = rom_desc },

    rom( 0x077d44f1, 0xfd86d120, 0x465bf79ac11efadb, "NewWorld v1.0.p.", nullptr   , "Mac OS ROM file 1.1"    ) // 1998-07-21 - Mac OS 8.1 (iMac, Rev A Bundle)
    rom( 0x077d44f3, 0xfd12b69e, 0xd3df605d1d1c72c3, "NewWorld v1.0.p.", nullptr   , "Mac OS ROM file 1.1.2"  ) // 1998-08-27 - Mac OS 8.5 (Retail CD), iMac Update 1.0
    rom( 0x077d44f4, 0xfcaad843, 0x9525d4d735b28e12, "NewWorld v1.0.p.", nullptr   , "Mac OS ROM file 1.1.5"  ) // 1998-09-19 - Mac OS 8.5 (iMac, Rev B Bundle)
    rom( 0x077d44f1, 0xd36ba902, 0x5cba340b0eeb2a67, "NewWorld v1.0.p.", nullptr   , "Mac OS ROM file 1.2"    ) // 1998-12-03 - Power Macintosh G3 (Blue and White) Mac OS 8.5.1 Bundle, Macintosh Server G3 (Blue and White) Mac OS 8.5.1 Bundle
    rom( 0x077d44f1, 0xd377adb7, 0x1051c5a4583a46b2, "NewWorld v1.0.p.", nullptr   , "Mac OS ROM file 1.2.1"  ) // 1999-01-22 - Mac OS 8.5.1 (Colors iMac 266 MHz Bundle), iMac Update 1.1
    rom( 0x077d44b5, 0xc804f7f4, 0xc456af44bf15c59a, "NewWorld v1.0.p.", nullptr   , "Mac OS ROM file 1.4"    ) // 1999-04-05 - Mac OS 8.6 (Retail CD), Mac OS 8.6 (Colors iMac 333 MHz Bundle), Power Macintosh G3 (Blue and White) Mac OS 8.6 Bundle
    rom( 0x077d44b5, 0xc7cb0323, 0xfd4d237a53b399ad, "NewWorld v1.0.p.", nullptr   , "Mac OS ROM file 1.6"    ) // 1999-05-14 - Macintosh PowerBook G3 Series 8.6 Bundle, Mac OS ROM Update 1.0
    rom( 0x077d44f1, 0xc75c6aab, 0xd9d8584aebaed648, "NewWorld v1.0.p.", nullptr   , "Mac OS ROM file 1.7.1"  ) // 1999-08-23 - Mac OS 8.6 bundled on Power Mac G4 (PCI)
    rom( 0x077d44f1, 0xc753c667, 0x5716eaf58bd3dda1, "NewWorld v1.0.p.", nullptr   , "Mac OS ROM file 1.8.1"  ) // 1999-08-28 - Mac OS 8.6 Power Mac G4 ROM 1.8.1 Update
    rom( 0x077d45f3, 0xcde9cda4, 0xe672c2885b5af6d0, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 2.3.1"  ) // 1999-09-13 - Mac OS 8.6 bundled on iMac (Slot Loading), iBook
    rom( 0x077d45f3, 0xce8a3b5c, 0xf0d9214daef11693, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 2.5.1"  ) // 1999-09-17 - Mac OS 8.6 bundled on Power Mac G4 (AGP)
    rom( 0x077d45f4, 0xce1fd217, 0x6e49bdb3f55e4583, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 3.0"    ) // 1999-09-27 - Retail Mac OS 9.0 installed on Power Macintosh G3 (Blue and White), Retail Mac OS 9.0 installed on iMac, Mac OS 9.0 bundled on PowerBook G3 Bronze
    rom( 0x077d45f5, 0xce1cf7f7, 0x6a8d3345ced59eef, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 3.1.1"  ) // 1999-10-28 - Mac OS 9.0 bundled on iBook, Mac OS 9.0 bundled on Power Mac G4 (AGP Graphics):iMac (Slot-Loading)
    rom( 0x077d45f6, 0xb9eb8c3d, 0x37ff1fc591c27899, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 3.5"    ) // 2000-01-29 - Mac OS 9.0.2 bundled on Power Mac G4 (AGP) and iBook, Mac OS 9.0.2 installed on PowerBook (FireWire)
    rom( 0x077d45f6, 0xb8c832f3, 0x68991dd330bdbf29, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 3.6"    ) // 2000-02-17 - Mac OS 9.0.3 bundled with iMac (Slot Loading)
    rom( 0x077d45f6, 0xb8b2c971, 0x3c28e7cb0cdf7247, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 3.7"    ) // 2000-03-15 - 9.0.4 Retail CD
    rom( 0x077d45f6, 0xb8bea8b3, 0x1f3fed032df9277a, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 3.8"    ) // 2000-05-22 - 9.0.4 Ethernet Update
    rom( 0x077d45f6, 0xc90b6289, 0xa63fc25ec62408b0, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 4.6.1"  ) // 2000-06-18 - Mac OS 9.0.4 Mac OS 9.0.4 bundled on iMac (Summer 2000), Power Mac G4 (Summer 2000)
    rom( 0x077d45f6, 0xc92f71d3, 0xf71fd99f08063d40, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 4.9.1"  ) // 2000-06-28 - Mac OS 9.0.4 bundled on Power Mac G4 MP (Summer 2000) (CPU software 2.3), Power Mac G4 (Gigabit Ethernet)
    rom( 0x077d45f6, 0xc8e1be97, 0xd666e7791a0313b8, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 5.2.1"  ) // 2000-07-12 - Mac OS 9.0.4 installed on Power Mac G4 Cube (CPU software 2.4)
    rom( 0x077d45f6, 0xce2a2a5b, 0x64278ec15492fff5, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 5.3.1"  ) // 2000-08-14 - Mac OS 9.0.4 bundled on iBook (Summer 2000) (CPU software 2.5)
    rom( 0x077d45f6, 0xce1b9fd2, 0x613a847c5a93f1f7, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 5.5.1"  ) // 2000-08-25 - Mac OS 9.0.4 from International G4 Cube Install CD
    rom( 0x077d45f6, 0xe20aa0d0, 0x19846d88d890eafe, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 6.1"    ) // 2000-11-03 - 9.1 Universal Update
    rom( 0x077d45f6, 0xeacb3ca4, 0x8e6380cf499094b3, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 6.7.1"  ) // 2000-12-01 - Mac OS 9.1 installed on Power Mac G4 (Digital Audio)
    rom( 0x077d45f6, 0xea00f1b7, 0x794eda200c4ee2c9, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 7.5.1"  ) // 2001-02-07 - 9.1 iMac 2001
    rom( 0x077d45f6, 0xeece7cd0, 0xbf44006b576a9770, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 7.8.1"  ) // 2001-04-10 - bundled on iBook (Dual USB) (CPU Software 3.5)
    rom( 0x077d45f6, 0xeed28047, 0xac8a8f4cec6d8d3b, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 7.9.1"  ) // 2001-04-24 - Mac OS 9.1 bundled on PowerBook G4
    rom( 0x077d45f6, 0xee6bc7d9, 0x71ea5cd85625c3a2, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 8.0"    ) // 2001-06-07 - Mac OS 9.2 Power Mac G4 Install CD
    rom( 0x077d45f6, 0xed7f9fc2, 0x27ab69c9f21df9d1, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 8.3.1"  ) // 2001-07-18 - Mac OS 9.2 installed on iMac (Summer 2001), Mac OS 9.2 installed on Power Mac G4 (QuickSilver)
    rom( 0x077d45f6, 0xed26a1ef, 0xa2a603b52bf6017b, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 8.4"    ) // 2001-07-30 - Mac OS 9.2.1 Update CD
    rom( 0x077d45f6, 0xec849611, 0x7840123158b8943c, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 8.6.1"  ) // 2001-09-25 - Mac OS 9.2.1 bundled on iBook G3 (Late 2001)
    rom( 0x077d45f6, 0xecc44a65, 0xbed3a4b747213d92, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 8.7"    ) // 2001-11-07 - Mac OS 9.2.2 Update SMI
    rom( 0x077d45f6, 0xec96aeb6, 0x5c3097a900eebbe5, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 8.8"    ) // 2001-11-26 - Mac OS 9.2.2 Update CD
    rom( 0x077d45f6, 0xec93ab73, 0x995426703e59abff, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 8.9.1"  ) // 2001-12-11 - Mac OS 9.2.2 bundled on iBook (CPU Software 4.4)
    rom( 0x077d45f6, 0xec86128e, 0x380ad7bdcaa89dc7, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 9.0.1"  ) // 2001-12-19 - Mac OS 9.2.2 bundled on  iMac (2001)
    rom( 0x077d45f6, 0xecef6af1, 0xcf149c7394b8c437, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 9.1.1"  ) // 2002-04-08 - Mac OS 9.2.2 bundled on iMac G4
    rom( 0x077d45f6, 0xecc6f29a, 0xabd8266a6d349401, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 9.2.1"  ) // 2002-04-17 - Mac OS 9.2.2 bundled on eMac (CPU Software 4.9)
    rom( 0x077d45f6, 0xecd3453f, 0xcdf9eb8fe542da0e, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 9.3.1"  ) // 2002-04-18 - Mac OS 9.2.2 bundled on PowerBook G4 (CPU Software 5.0)
    rom( 0x077d45f6, 0xecaf0460, 0x22cde263f891fa1b, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 9.5.1"  ) // 2002-07-18 - Mac OS 9.2.2 bundled on iMac (17" Flat Panel) (CPU Software 5.3)
    rom( 0x077d45f6, 0xecbd9bd2, 0xec3ca440cd55c910, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 9.6.1"  ) // 2002-09-03 - Mac OS 9.2.2 (CPU Software 5.4)
    rom( 0x077d45f6, 0xecb7c4f9, 0x0e6d36f70dba2607, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 9.7.1"  ) // 2002-10-11 - Mac OS 9.2.2 bundled on PowerBook (Titanium, 1GHz)
    rom( 0x077d45f6, 0xecb96443, 0x1de4e772a4c70d9e, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 9.8.1"  ) // 2003-01-10 - Mac OS 9.2.2
    rom( 0x077d45f6, 0xecb8e951, 0x618a1da98364bf3f, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 10.1.1" ) // 2003-03-17 - Mac OS 9.2.2 bundled on eMac 800MHz (CPU Software 5.7)
    rom( 0x077d45f6, 0xecb73ad5, 0xe8bf5f2e952e87c0, "NewWorld v1.0...", nullptr   , "Mac OS ROM file 10.2.1" ) // 2003-04-03 - Mac OS 9.2.2 Retail International CD

#undef rom

// PPC First New World ROMs
#define rom(version, dppc_mach, dppc_desc, rom_desc) \
    { .firmware_version       = version,             \
      .firmware_size_k        = 1024,                \
      .dppc_machine           = dppc_mach,           \
      .dppc_description       = dppc_desc,           \
      .rom_description        = rom_desc },

    rom( 0x10f1, "pbg3lb"  , "PowerBook G3 Lombard" , "PowerBook G3 Lombard"              ) // PowerBook1,1 // 1999-04-06 3.1.0f1
    rom( 0x11f4, "pmg3nw"  , "Power Mac Yosemite"   , "Power Mac B&W G3"                  ) // PowerMac1,1  // 1999-04-09 3.1.1f4
    rom( 0x12f2, "pmyikes" , "Power Mac G4 Yikes"   , "Power Mac G4 Yikes"                ) // PowerMac1,2  // 1999-08-19 3.1.2f2
    rom( 0x13f2, "imacg3"  , "iMac G3 Bondi"        , "iMac (233 MHz) (Bondi Blue)"       ) // iMac,1       // 1999-04-23 3.1.3f2
    rom( 0x13f3, "imacg3"  , "iMac G3 Tray Loading" , "iMac (266,333 MHz) (Tray Loading)" ) // iMac,1       // 1999-07-16 3.1.3f3

#undef rom

// PPC New World ROMs with config
#define rom(product_id, subconfig_checksum, nw_updater_name, nw_of_name, dppc_mach, dppc_desc)  \
    { .firmware_size_k                  = 1024,                                                 \
      .nw_product_id                    = product_id,                                           \
      .nw_subconfig_expected_checksum   = subconfig_checksum,                                   \
      .nw_firmware_updater_name         = nw_updater_name,                                      \
      .nw_openfirmware_name             = nw_of_name,                                           \
      .dppc_machine                     = dppc_mach,                                            \
      .dppc_description                 = dppc_desc },

    rom( 0x008100, 0x266f2e55, "Kihei"       , "P7"       , nullptr  , "iMac G3 (Slot Loading)"                            ) // PowerMac2,1 // 2001-09-14 419f1
    rom( 0x008100, 0x55402f54, "Kihei"       , "P7"       , nullptr  , "iMac G3 (Slot Loading)"                            ) // PowerMac2,1
    rom( 0x008100, 0xf88e2d56, "P7"          , "P7"       , nullptr  , "iMac G3 (Slot Loading)"                            ) // PowerMac2,1
    rom( 0x008200, 0x141d2d96, "P51"         , "P51"      , nullptr  , "iMac G3 (Summer 2000)"                             ) // PowerMac2,2
    rom( 0x008200, 0x41ef2e95, "Perigee"     , "P51"      , nullptr  , "iMac G3 (Summer 2000)"                             ) // PowerMac2,2
    rom( 0x008201, 0x4a862e17, "P51_15"      , "P51"      , nullptr  , "iMac G3 (Summer 2000)"                             ) // PowerMac2,2
    rom( 0x008201, 0x78582f16, "Perigee_15"  , "P51"      , nullptr  , "iMac G3 (Summer 2000)"                             ) // PowerMac2,2
    rom( 0x010100,          0, nullptr       , "P52"      , nullptr  , "iMac G3 (2001)"                                    ) // PowerMac4,1
    rom( 0x010101, 0x9a7a2c2c, "P52"         , nullptr    , nullptr  , "iMac G3 (2001)"                                    ) // PowerMac4,1
    rom( 0x010101, 0xc84c2d2b, "Apogee"      , nullptr    , nullptr  , "iMac G3 (2001)"                                    ) // PowerMac4,1
    rom( 0x010200, 0xe27f2d68, "Tessera"     , "P80"      , nullptr  , "iMac G4 (Flat Panel)"                              ) // PowerMac4,2
    rom( 0x010202, 0xc32928ab, "P80"         , nullptr    , nullptr  , "iMac G4 (Flat Panel)"                              ) // PowerMac4,2
    rom( 0x010202, 0xe3512d6a, "Insp"        , nullptr    , nullptr  , "iMac G4 (Flat Panel)"                              ) // PowerMac4,2
    rom( 0x010202, 0xfaf12da6, "P80|Insp"    , nullptr    , nullptr  , "iMac G4 (Flat Panel)"                              ) // PowerMac4,2 // 2002-04-08 440f1 // 2002-05-17 441f1
    rom( 0x010203,          0, nullptr       , "P80"      , nullptr  , "iMac G4 (Flat Panel)"                              ) // PowerMac4,2
    rom( 0x010300, 0xe27f2d68, "Infinity"    , nullptr    , nullptr  , nullptr                                             ) // PowerMac4,3
    rom( 0x010400, 0xa0972cec, "Beyond"      , "P62"      , nullptr  , "eMac G4"                                           ) // PowerMac4,4
    rom( 0x010400, 0xa7cd2b85, "P62"         , "P62"      , nullptr  , "eMac G4"                                           ) // PowerMac4,4
    rom( 0x010400, 0xe72d2d73, "NorthnLites" , "P62"      , nullptr  , "eMac G4"                                           ) // PowerMac4,4
    rom( 0x010401,          0, nullptr       , "P86"      , nullptr  , "eMac G4"                                           ) // PowerMac4,4
    rom( 0x010402,          0, nullptr       , "P86"      , nullptr  , "eMac G4"                                           ) // PowerMac4,4
    rom( 0x010500, 0xa90624c6, "P79"         , "P79"      , nullptr  , "iMac G4 17 inch (Flat Panel)"                      ) // PowerMac4,5
    rom( 0x010500, 0xf1332daa, "Taliesin"    , "P79"      , nullptr  , "iMac G4 17 inch (Flat Panel)"                      ) // PowerMac4,5
    rom( 0x010500, 0xd6d825c5, "P79|Taliesin", "P79"      , nullptr  , "iMac G4 17 inch (Flat Panel)"                      ) // PowerMac4,5 // 2002-07-23 445f3
    rom( 0x018101, 0xacd324d3, nullptr       , "Q26"      , nullptr  , "iMac G4/1.0 17 inch (Flat Panel)"                  ) // PowerMac6,1 // 2003-01-13 458f1
    rom( 0x018102, 0xcd1f2ca7, "P87"         , "Q26"      , nullptr  , "iMac G4/1.0 17 inch (Flat Panel)"                  ) // PowerMac6,1
    rom( 0x018301,          0, nullptr       , "Q59"      , nullptr  , "iMac G4/1.0 (Flat Panel - USB 2.0)"                ) // PowerMac6,3
    rom( 0x018401,          0, nullptr       , "Q86"      , nullptr  , "eMac G4 (2005)"                                    ) // PowerMac6,4
    rom( 0x018402,          0, nullptr       , "Q86"      , nullptr  , "eMac G4 (2005)"                                    ) // PowerMac6,4
    rom( 0x018403,          0, nullptr       , "Q86"      , nullptr  , "eMac G4 (2005)"                                    ) // PowerMac6,4
    rom( 0x020101, 0xfcaf4eb7, "Q45"         , "Q45"      , nullptr  , "iMac G5"                                           ) // PowerMac8,1
    rom( 0x020101, 0xfd1f4eb8, "Q45"         , "Q45"      , nullptr  , "iMac G5"                                           ) // PowerMac8,1
    rom( 0x020102,          0, nullptr       , "Q45"      , nullptr  , "iMac G5"                                           ) // PowerMac8,1
    rom( 0x020109, 0x00064ebf, "Q45p"        , nullptr    , nullptr  , "iMac G5"                                           ) // PowerMac8,1
    rom( 0x020109, 0x00764ec0, "Q45p"        , nullptr    , nullptr  , "iMac G5"                                           ) // PowerMac8,1
    rom( 0x020109, 0x24372c87, "Q45p"        , nullptr    , nullptr  , "iMac G5"                                           ) // PowerMac8,1
    rom( 0x020109, 0x24932c8b, "Q45p"        , nullptr    , nullptr  , "iMac G5"                                           ) // PowerMac8,1
    rom( 0x020201,          0, nullptr       , "Q45C"     , nullptr  , "iMac G5 (Ambient Light Sensor)"                    ) // PowerMac8,2
    rom( 0x020f01, 0x20ef2c7f, "Q45xa"       , nullptr    , nullptr  , nullptr                                             ) // PowerMac8,15
    rom( 0x020f01, 0x214b2c83, "Q45xa"       , nullptr    , nullptr  , nullptr                                             ) // PowerMac8,15
    rom( 0x020f01, 0xeace56ae, "Neoa"        , nullptr    , nullptr  , nullptr                                             ) // PowerMac8,15
    rom( 0x020f02, 0x21582c80, "Q45xb"       , nullptr    , nullptr  , nullptr                                             ) // PowerMac8,15
    rom( 0x020f02, 0x21b42c84, "Q45xb"       , nullptr    , nullptr  , nullptr                                             ) // PowerMac8,15
    rom( 0x020f02, 0xf92756d2, "Neob"        , nullptr    , nullptr  , nullptr                                             ) // PowerMac8,15
    rom( 0x020f03, 0x21c12c81, "Q45xc"       , nullptr    , nullptr  , nullptr                                             ) // PowerMac8,15
    rom( 0x020f03, 0x221d2c85, "Q45xc"       , nullptr    , nullptr  , nullptr                                             ) // PowerMac8,15
    rom( 0x020f03, 0x9e7f55ef, "Neoc"        , nullptr    , nullptr  , nullptr                                             ) // PowerMac8,15
    rom( 0x028101, 0xf4dc2533, nullptr       , "Q88"      , nullptr  , "Mac mini G4"                                       ) // PowerMac10,1 // 2005-03-23 489f4
    rom( 0x028201, 0xf4dc2533, nullptr       , "Q88"      , nullptr  , "Mac mini G4 1.5GHz Radeon 9200"                    ) // PowerMac10,2 // 2005-07-12 494f1
    rom( 0x030101,          0, nullptr       , "M23"      , nullptr  , "iMac G5 (iSight)"                                  ) // PowerMac12,1
    rom( 0x108100, 0x71fd2fc9, "P1"          , "P1"       , nullptr  , "iBook G3 (Original/Clamshell)"                     ) // PowerBook2,1
    rom( 0x108100, 0x9fcf30c8, "P1"          , "P1"       , nullptr  , "iBook G3 (Original/Clamshell)"                     ) // PowerBook2,1
    rom( 0x108100, 0xcea031c7, "P1"          , "P1"       , nullptr  , "iBook G3 (Original/Clamshell)"                     ) // PowerBook2,1
    rom( 0x108101, 0x72902fcb, "P1_05"       , "P1"       , nullptr  , "iBook G3 (Original/Clamshell)"                     ) // PowerBook2,1
    rom( 0x108101, 0xa06230ca, "P1_05"       , "P1"       , nullptr  , "iBook G3 (Original/Clamshell)"                     ) // PowerBook2,1 // 2001-03-20 417f4
    rom( 0x108101, 0xcf3331c9, "P1_05"       , "P1"       , nullptr  , "iBook G3 (Original/Clamshell)"                     ) // PowerBook2,1
    rom( 0x108102, 0x7de22ffd, "P1_1"        , "P1"       , nullptr  , "iBook G3 (Original/Clamshell)"                     ) // PowerBook2,1
    rom( 0x108102, 0xabb430fc, "P1_1"        , "P1"       , nullptr  , "iBook G3 (Original/Clamshell)"                     ) // PowerBook2,1
    rom( 0x108102, 0xda8531fb, "P1_1"        , "P1"       , nullptr  , "iBook G3 (Original/Clamshell)"                     ) // PowerBook2,1
    rom( 0x108200, 0x7bdc2fd9, "P1_5"        , "P1_5"     , nullptr  , "iBook G3 366 MHz CD (Firewire/Clamshell)"          ) // PowerBook2,2
    rom( 0x108200, 0xa9ae30d8, "Midway"      , "P1_5"     , nullptr  , "iBook G3 366 MHz CD (Firewire/Clamshell)"          ) // PowerBook2,2
    rom( 0x108201, 0x9745301a, "P1_5DVD"     , "P1_5"     , nullptr  , "iBook G3 466 MHz DVD (Firewire/Clamshell)"         ) // PowerBook2,2
    rom( 0x108201, 0xc5173119, "MidwayDVD"   , "P1_5"     , nullptr  , "iBook G3 466 MHz DVD (Firewire/Clamshell)"         ) // PowerBook2,2
    rom( 0x110100, 0x5f1c2fe5, "Marble"      , "P29"      , nullptr  , "iBook G3 (Dual USB Snow)"                          ) // PowerBook4,1
    rom( 0x110100, 0x69e42f6e, "P29"         , "P29"      , nullptr  , "iBook G3 (Dual USB Snow)"                          ) // PowerBook4,1
    rom( 0x110100, 0x97b6306d, "Marble"      , "P29"      , nullptr  , "iBook G3 (Dual USB Snow)"                          ) // PowerBook4,1
    rom( 0x110101, 0x44852fa6, "MarbleLite"  , nullptr    , nullptr  , "iBook G3 (Dual USB Snow)"                          ) // PowerBook4,1
    rom( 0x110102, 0x60192fe8, "MarbleFat"   , "P29"      , nullptr  , "iBook G3 (Dual USB Snow)"                          ) // PowerBook4,1
    rom( 0x110102, 0x6b1d2f73, "P29Fat"      , "P29"      , nullptr  , "iBook G3 (Dual USB Snow)"                          ) // PowerBook4,1
    rom( 0x110102, 0x98ef3072, "MarbleFat"   , "P29"      , nullptr  , "iBook G3 (Dual USB Snow)"                          ) // PowerBook4,1
    rom( 0x110103, 0x58762f44, "P29fat100"   , "P29"      , nullptr  , "iBook G3 (Dual USB Snow)"                          ) // PowerBook4,1
    rom( 0x110103, 0x86483043, "Mrblfat100"  , "P29"      , nullptr  , "iBook G3/600 (Late 2001 Snow)"                     ) // PowerBook4,1 // 2001-09-12 427f1
    rom( 0x110200, 0x4c372fb6, "Diesel"      , "P54"      , nullptr  , "iBook G3/600 14-Inch (Early 2002 Snow)"            ) // PowerBook4,2
    rom( 0x110200, 0x573b2f41, "P54"         , "P54"      , nullptr  , "iBook G3/600 14-Inch (Early 2002 Snow)"            ) // PowerBook4,2
    rom( 0x110200, 0x850d3040, "P54|Diesel"  , "P54"      , nullptr  , "iBook G3/600 14-Inch (Early 2002 Snow)"            ) // PowerBook4,2 // 2001-12-06 432f1
    rom( 0x110300, 0x58e12d5c, "P72"         , "P72"      , nullptr  , "iBook G3 (Snow)"                                   ) // PowerBook4,3 // 2002-11-11 454f1
    rom( 0x110300, 0xb98a30be, "Nectr"       , "P72"      , nullptr  , "iBook G3 (Snow)"                                   ) // PowerBook4,3
    rom( 0x110301, 0x594a2d5d, "P73"         , "P73"      , nullptr  , "iBook G3 (Snow)"                                   ) // PowerBook4,3
    rom( 0x110302, 0x59b32d5e, "P72x"        , "P73"      , nullptr  , "iBook G3 (Snow)"                                   ) // PowerBook4,3 // 2003-03-15 464f1
    rom( 0x110302, 0x3eb82d1d, "P72x"        , "P73"      , nullptr  , "iBook G3 (Snow)"                                   ) // PowerBook4,3
    rom( 0x110303, 0x3f212d1e, "P73x"        , "P73"      , nullptr  , "iBook G3 (Snow)"                                   ) // PowerBook4,3
    rom( 0x118101,          0, nullptr       , "P99"      , nullptr  , "PowerBook G4 (Aluminum)"                           ) // PowerBook6,1
    rom( 0x118202,          0, nullptr       , "Q54"      , nullptr  , "PowerBook G4 1.0 12 inch (DVI - Aluminum)"         ) // PowerBook6,2
    rom( 0x118302,          0, nullptr       , "P72D"     , nullptr  , "iBook G4 (Original - Opaque)"                      ) // PowerBook6,3
    rom( 0x11830c,          0, nullptr       , "P73D"     , nullptr  , "iBook G4 (Original - Opaque)"                      ) // PowerBook6,3
    rom( 0x118402,          0, nullptr       , "Q54A"     , nullptr  , "PowerBook G4 1.33 12 inch (Aluminum)"              ) // PowerBook6,4
    rom( 0x118502, 0x033929a6, nullptr       , "Q72"      , nullptr  , "iBook G4 (Early 2004)"                             ) // PowerBook6,5 // 2004-04-06 485f0
    rom( 0x118504,          0, nullptr       , "Q72A"     , nullptr  , "iBook G4"                                          ) // PowerBook6,5
    rom( 0x118509,          0, nullptr       , "Q73"      , nullptr  , "iBook G4"                                          ) // PowerBook6,5
    rom( 0x11850b, 0x067f29b0, nullptr       , "Q73A"     , nullptr  , "iBook G4"                                          ) // PowerBook6,5 // 2004-09-23 487f1
    rom( 0x118603,          0, nullptr       , "U210"     , nullptr  , nullptr                                             ) // PowerBook6,6
    rom( 0x118701,          0, nullptr       , "Q72B"     , nullptr  , "iBook G4 12-Inch (Mid-2005 - Opaque)"              ) // PowerBook6,7
    rom( 0x118709,          0, nullptr       , "Q73B"     , nullptr  , "iBook G4 12-Inch (Mid-2005 - Opaque)"              ) // PowerBook6,7
    rom( 0x11870c, 0x091929b6, nullptr       , "Q73B-Best", nullptr  , "iBook G4 14-Inch (Mid-2005 - Opaque)"              ) // PowerBook6,7 // 2005-07-05 493f0
    rom( 0x118801,          0, nullptr       , "Q54B"     , nullptr  , "PowerBook G4 1.5 12 inch (Aluminum)"               ) // PowerBook6,8
    rom( 0x20c100, 0x85e72bd1, "P5"          , "P5"       , nullptr  , "Power Mac G4 (AGP Graphics) Sawtooth"              ) // PowerMac3,1
    rom( 0x20c100, 0xb3b92cd0, "Sawtooth"    , "P5"       , nullptr  , "Power Mac G4 (AGP Graphics) Sawtooth"              ) // PowerMac3,1
    rom( 0x20c100, 0xe28a2dcf, "Sawtooth"    , "P5"       , nullptr  , "Power Mac G4 (AGP Graphics) Sawtooth"              ) // PowerMac3,1
    rom( 0x20c101, 0x8d142cda, "Mystic"      , "P5"       , nullptr  , "Power Mac G4 (AGP Graphics) Sawtooth"              ) // PowerMac3,1 // 2000-02-17 324f1
    rom( 0x20c101, 0x5e432bdb, "Mystic"      , "P5"       , nullptr  , "Power Mac G4 (AGP Graphics) Sawtooth"              ) // PowerMac3,1 // 2001-10-11 428f1
    rom( 0x20c101, 0x30712adc, "P10"         , "P5"       , nullptr  , "Power Mac G4 (AGP Graphics) Sawtooth"              ) // PowerMac3,1
    rom( 0x20c300, 0x66752b5c, "P15"         , "P5"       , nullptr  , "Power Macintosh Mac G4 (Gigabit)"                  ) // PowerMac3,3
    rom( 0x20c300, 0x94472c5b, "Clockwork"   , "P5"       , nullptr  , "Power Macintosh Mac G4 (Gigabit)"                  ) // PowerMac3,3
    rom( 0x20c400, 0x47fe2da3, "P21"         , "P21"      , nullptr  , "Power Mac G4 (Digital Audio)"                      ) // PowerMac3,4
    rom( 0x20c400, 0x75d02ea2, "Tangent"     , "P21"      , nullptr  , "Power Mac G4 (Digital Audio)"                      ) // PowerMac3,4 // 2001-10-11 428f1
    rom( 0x20c400, 0x6ea22e91, "P21|Tangent" , "P21"      , nullptr  , "Power Mac G4 (Digital Audio)"                      ) // PowerMac3,4 // 2000-12-04 410f1
    rom( 0x20c500, 0x4b5e2dab, "P57"         , "P57"      , nullptr  , "Power Mac G4 Quicksilver"                          ) // PowerMac3,5
    rom( 0x20c500, 0x75d02ea2, "NiChrome"    , "P57"      , nullptr  , "Power Mac G4 Quicksilver"                          ) // PowerMac3,5
    rom( 0x20c500, 0x79302eaa, "NiChrome"    , "P57"      , nullptr  , "Power Mac G4 Quicksilver"                          ) // PowerMac3,5 // 2001-08-16 425f1
    rom( 0x20c600, 0x6e5a2d67, "P58_133"     , "P58"      , nullptr  , "Power Mac G4 (Mirrored Drive Doors)"               ) // PowerMac3,6 // 2002-09-30 448f2
    rom( 0x20c600, 0x79302eaa, "Moj"         , "P58"      , nullptr  , "Power Mac G4 (Mirrored Drive Doors)"               ) // PowerMac3,6
    rom( 0x20c601, 0x20df2ca4, "P58_167"     , "P58"      , nullptr  , "Power Mac G4 (Mirrored Drive Doors)"               ) // PowerMac3,6
    rom( 0x20c602, 0x6f2c2d69, nullptr       , "P58"      , nullptr  , "Power Mac G4 (FW 800)"                             ) // PowerMac3,6 // 2003-01-15 457f1
    rom( 0x20c603, 0x21b12ca6, nullptr       , "P58"      , nullptr  , "Power Mac G4 (FW 800)"                             ) // PowerMac3,6 // 2003-02-20 460f1
    rom( 0x214100, 0x4af52b1c, "P9"          , "P9"       , nullptr  , "Power Mac G4 Cube"                                 ) // PowerMac5,1
    rom( 0x214100, 0x78c72c1b, "Trinity"     , "P9"       , nullptr  , "Power Mac G4 Cube"                                 ) // PowerMac5,1 // 2000-07-10 332f1 // 2001-09-14 419f1
    rom( 0x214100, 0x78c72c1b, "Kubrick"     , "P9"       , nullptr  , "Power Mac G4 Cube"                                 ) // PowerMac5,1 // 2000-07-10 332f1 // 2001-09-14 419f1
    rom( 0x214100, 0x8cab2cd9, "Kubrick"     , "P9"       , nullptr  , "Power Mac G4 Cube"                                 ) // PowerMac5,1
    rom( 0x21c200, 0x25142c89, "Q37high"     , "Q37"      , nullptr  , "Power Mac G5 1.6 (PCI)"                            ) // PowerMac7,2
    rom( 0x21c200, 0x25702c8d, "Q37high"     , "Q37"      , nullptr  , "Power Mac G5 1.6 (PCI)"                            ) // PowerMac7,2
    rom( 0x21c200, 0xa5b7555f, "Q37high"     , "Q37"      , nullptr  , "Power Mac G5 1.6 (PCI)"                            ) // PowerMac7,2
    rom( 0x21c201, 0x336d2cad, "Q37med"      , "Q37"      , nullptr  , "Power Mac G5 1.6 (PCI)"                            ) // PowerMac7,2
    rom( 0x21c201, 0x33c92cb1, "Q37med"      , "Q37"      , nullptr  , "Power Mac G5 1.6 (PCI)"                            ) // PowerMac7,2
    rom( 0x21c201, 0x514d2dbb, "P76"         , "Q37"      , nullptr  , "Power Mac G5 1.6 (PCI)"                            ) // PowerMac7,2
    rom( 0x21c201, 0xb4105583, "Q37med"      , "Q37"      , nullptr  , "Power Mac G5 1.6 (PCI)"                            ) // PowerMac7,2
    rom( 0x21c202, 0x596854a0, "Q37low"      , "Q37low"   , nullptr  , "Power Mac G5 1.6 (PCI)"                            ) // PowerMac7,2
    rom( 0x21c202, 0xd8c62bca, "Q37low"      , "Q37low"   , nullptr  , "Power Mac G5 1.6 (PCI)"                            ) // PowerMac7,2 // 2004-09-21 515f2
    rom( 0x21c203, 0x343f2caf, "Q37A"        , "Q37low"   , nullptr  , "Power Mac G5 1.6 (PCI)"                            ) // PowerMac7,2
    rom( 0x21c203, 0x349b2cb3, "Q37A"        , "Q37low"   , nullptr  , "Power Mac G5 1.6 (PCI)"                            ) // PowerMac7,2
    rom( 0x21c203, 0xb4e25585, "Q37A"        , "Q37low"   , nullptr  , "Power Mac G5 1.6 (PCI)"                            ) // PowerMac7,2
    rom( 0x21c204, 0xa8955568, "Q37C"        , "Q77hi"    , nullptr  , "Power Mac G5 1.6 (PCI)"                            ) // PowerMac7,2
    rom( 0x21c204, 0xa930556d, "Q37C"        , "Q77hi"    , nullptr  , "Power Mac G5 1.6 (PCI)"                            ) // PowerMac7,2
    rom( 0x21c301, 0xb6cf558d, "Q77best"     , "Q77hi"    , nullptr  , "Power Macintosh G5 Dual Processor"                 ) // PowerMac7,3
    rom( 0x21c301, 0xb76a5592, "Q77best"     , "Q77hi"    , nullptr  , "Power Macintosh G5 Dual Processor"                 ) // PowerMac7,3
    rom( 0x21c302, 0xa7635563, "Q77mid"      , "Q77"      , nullptr  , "Power Macintosh G5 Dual Processor"                 ) // PowerMac7,3
    rom( 0x21c303, 0xb5bc5587, "Q77good"     , "Q77good"  , nullptr  , "Power Macintosh G5 Dual Processor"                 ) // PowerMac7,3
    rom( 0x21c304, 0xb80a5590, "Q77better"   , "Q77better", nullptr  , "Power Macintosh G5 Dual Processor"                 ) // PowerMac7,3
    rom( 0x21c304, 0xb8a55595, "Q77better"   , "Q77better", nullptr  , "Power Macintosh G5 Dual Processor"                 ) // PowerMac7,3
    rom( 0x21c305,          0, nullptr       , "M18wl"    , nullptr  , "Power Macintosh G5 Dual Processor"                 ) // PowerMac7,3
    rom( 0x21c306,          0, nullptr       , "Q87good"  , nullptr  , "Power Macintosh G5 Dual Processor"                 ) // PowerMac7,3
    rom( 0x21c307,          0, nullptr       , "Q77better", nullptr  , "Power Macintosh G5 Dual Processor"                 ) // PowerMac7,3
    rom( 0x21c308,          0, nullptr       , "Q77hi"    , nullptr  , "Power Macintosh G5 Dual Processor"                 ) // PowerMac7,3
    rom( 0x224102,          0, nullptr       , "Q78"      , nullptr  , "Power Macintosh G5 1.8 (PCI)"                      ) // PowerMac9,1
    rom( 0x224108, 0x84d94d6f, "Q78EVT"      , nullptr    , nullptr  , "Power Macintosh G5 1.8 (PCI)"                      ) // PowerMac9,1
    rom( 0x224109, 0x82b74d9f, "Q78p"        , nullptr    , nullptr  , "Power Macintosh G5 1.8 (PCI)"                      ) // PowerMac9,1
    rom( 0x224109, 0x86774da9, "Q78p"        , nullptr    , nullptr  , "Power Macintosh G5 1.8 (PCI)"                      ) // PowerMac9,1
    rom( 0x22c101,          0, nullptr       , "M18"      , nullptr  , nullptr                                             ) // PowerMac11,1
    rom( 0x22c102,          0, nullptr       , "M20wl"    , nullptr  , nullptr                                             ) // PowerMac11,1
    rom( 0x22c201,          0, nullptr       , "Q63Proto" , nullptr  , "Power Mac G5 Quad Core Proto"                      ) // PowerMac11,2
    rom( 0x22c202, 0xb7fe51fc, nullptr       , "Q63"      , nullptr  , "Power Mac G5 Quad Core"                            ) // PowerMac11,2 // 2005-09-30 527f1
    rom( 0x30c100, 0x0c653168, "P8"          , "P8"       , nullptr  , "PowerBook G3 (FireWire) Pismo"                     ) // PowerBook3,1
    rom( 0x30c100, 0x3a373267, "Pismo"       , "P8"       , nullptr  , "PowerBook G3 (FireWire) Pismo"                     ) // PowerBook3,1 // 2001-03-21 418f5
    rom( 0x30c100, 0x69083366, "Pismo"       , "P8"       , nullptr  , "PowerBook G3 (FireWire) Pismo"                     ) // PowerBook3,1
    rom( 0x30c1ff, 0xcb8e3457, "Pismo66"     , nullptr    , nullptr  , "PowerBook G3 (FireWire) Pismo"                     ) // PowerBook3,1
    rom( 0x30c200,          0, nullptr       , "P12"      , nullptr  , "PowerBook G4 (Original - Titanium)"                ) // PowerBook3,2
    rom( 0x30c201, 0x33b22dc6, "P12"         , "P12"      , nullptr  , "PowerBook G4 (Original - Titanium)"                ) // PowerBook3,2
    rom( 0x30c201, 0x61842ec5, "Mercury"     , "P12"      , nullptr  , "PowerBook G4 (Original - Titanium)"                ) // PowerBook3,2
    rom( 0x30c300, 0x3e4f2dd5, "P25_100"     , "P25"      , nullptr  , "PowerBook G4 (Gigabit - Titanium)"                 ) // PowerBook3,3
    rom( 0x30c300, 0x63762eca, "Onyx"        , "P25"      , nullptr  , "PowerBook G4 (Gigabit - Titanium)"                 ) // PowerBook3,3
    rom( 0x30c300, 0x6c212ed4, "Onix100"     , "P25"      , nullptr  , "PowerBook G4 (Gigabit - Titanium)"                 ) // PowerBook3,3
    rom( 0x30c301, 0x4c2b2df8, "P25"         , "P25"      , nullptr  , "PowerBook G4 (Gigabit - Titanium)"                 ) // PowerBook3,3
    rom( 0x30c301, 0x79fd2ef7, "Onix"        , "P25"      , nullptr  , "PowerBook G4 (Gigabit - Titanium)"                 ) // PowerBook3,3
    rom( 0x30c302, 0x7e6c2f0a, "OnixStar"    , nullptr    , nullptr  , "PowerBook G4 (Gigabit - Titanium)"                 ) // PowerBook3,3
    rom( 0x30c400, 0x7a002ef7, "Ivry"        , "P59"      , nullptr  , "PowerBook G4 (DVI - Titanium)"                     ) // PowerBook3,4
    rom( 0x30c400, 0x91952f11, "P59_667"     , "P59"      , nullptr  , "PowerBook G4 (DVI - Titanium)"                     ) // PowerBook3,4
    rom( 0x30c402, 0x92672f13, "P59_800"     , nullptr    , nullptr  , "PowerBook G4 (DVI - Titanium)"                     ) // PowerBook3,4
    rom( 0x30c403, 0x92d02f14, "P59_DVT"     , nullptr    , nullptr  , "PowerBook G4 (DVI - Titanium)"                     ) // PowerBook3,4
    rom( 0x30c404, 0x93392f15, "P59_DualFan" , "P59DF"    , nullptr  , "PowerBook G4 (DVI - Titanium)"                     ) // PowerBook3,4
    rom( 0x30c500,          0, nullptr       , "P88"      , nullptr  , "PowerBook G4 (Titanum)"                            ) // PowerBook3,5
    rom( 0x30c501,          0, nullptr       , "P881G"    , nullptr  , "PowerBook G4 (Titanum)"                            ) // PowerBook3,5
    rom( 0x314100, 0x6ece5388, "P84i"        , nullptr    , nullptr  , "PowerBook G4 1.0 17 inch (Aluminum)"               ) // PowerBook5,1
    rom( 0x314103, 0x5c13264b, nullptr       , "P84"      , nullptr  , "PowerBook G4 1.0 17 inch (Aluminum)"               ) // PowerBook5,1 // 2003-02-18 462f1
    rom( 0x314202, 0xf60a284b, nullptr       , "Q16-EVT"  , nullptr  , "PowerBook G4 15 inch (FW 800 - Aluminum)"          ) // PowerBook5,2 // 2003-09-04 471f1
    rom( 0x314301,          0, nullptr       , "Q41"      , nullptr  , "PowerBook G4 1.33 17 inch (Aluminum)"              ) // PowerBook5,3
    rom( 0x314401,          0, nullptr       , "Q16A"     , nullptr  , "PowerBook G4 15 inch (Aluminum)"                   ) // PowerBook5,4
    rom( 0x314501,          0, nullptr       , "Q41A"     , nullptr  , "PowerBook G4 1.5 17 inch (Aluminum)"               ) // PowerBook5,5
    rom( 0x314601,          0, nullptr       , "Q16B"     , nullptr  , "PowerBook G4 15 inch (Aluminum)"                   ) // PowerBook5,6
    rom( 0x314701,          0, nullptr       , "Q41B"     , nullptr  , "PowerBook G4 1.67 17 inch (Aluminum)"              ) // PowerBook5,7
    rom( 0x314801, 0x35c72568, nullptr       , "Q16C"     , nullptr  , "PowerBook G4 DLSD"                                 ) // PowerBook5,8 // 2005-09-22 495f3
    rom( 0x314801,          0, nullptr       , "Q41C"     , nullptr  , "PowerBook G4 DLSD"                                 ) // PowerBook5,8
    rom( 0x314802,          0, nullptr       , "Q16CBest" , nullptr  , "PowerBook G4 DLSD"                                 ) // PowerBook5,8
    rom( 0x314901, 0x35c72568, nullptr       , "Q41C"     , nullptr  , "PowerBook G4 1.67 17 inch (DLSD/HiRes - Aluminum)" ) // PowerBook5,9 // 2005-10-05 496f0
    rom( 0x318100, 0x6ca8272f, "P99"         , nullptr    , nullptr  , "PowerBook G4 867 12 inch (Aluminum)"               ) // PowerBook6,1
    rom( 0x31c101, 0xbb11558a, "Q51p"        , "Q51"      , nullptr  , nullptr                                             ) // PowerBook7,1
    rom( 0x31c101, 0xbc5d558d, "Q51p"        , "Q51"      , nullptr  , nullptr                                             ) // PowerBook7,1
    rom( 0x31c201, 0xbb11558a, "Q43p"        , "Q43"      , nullptr  , nullptr                                             ) // PowerBook7,2
    rom( 0x31c201, 0xbc5d558d, "Q43p"        , "Q43"      , nullptr  , nullptr                                             ) // PowerBook7,2
    rom( 0x380101,          0, nullptr       , "T3"       , nullptr  , nullptr                                             ) // PowerBook32,1
    rom( 0x380201,          0, nullptr       , "M22"      , nullptr  , nullptr                                             ) // PowerBook32,2
    rom( 0x404100, 0x268f2cab, "P69"         , "P69"      , nullptr  , "Xserve G4 1.0 GHz"                                 ) // RackMac1,1
    rom( 0x404200,          0, nullptr       , "Q28"      , nullptr  , "Xserve G4 1.33 GHz (Slot Load)"                    ) // RackMac1,2 // 465f3
    rom( 0x40c100, 0xc2f855a9, "Q42"         , "Q42"      , nullptr  , "Xserve G5 (PCI-X)"                                 ) // RackMac3,1
    rom( 0x40c101, 0xc28755a8, "Q42B"        , "Q42"      , nullptr  , "Xserve G5 (PCI-X)"                                 ) // RackMac3,1
    rom( 0x414101,          0, nullptr       , "Q42C"     , nullptr  , nullptr                                             ) // RackMac5,1

#undef rom
    { 0 }
};
