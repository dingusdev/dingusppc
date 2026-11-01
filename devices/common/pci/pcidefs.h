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

#ifndef PCI_DEFS_H
#define PCI_DEFS_H

#include <cinttypes>

/** PCI device class/subclass/programming interface definitions. */
enum : uint8_t {
    DEVCLASS_UNCLASSIFIED       = 0x00, // unclassified/legacy devices
    DEVCLASS_MASS_STORAGE_CTRL  = 0x01, // mass storage controllers
        SUBCLASS_IDE_CTRL       = 0x01, // IDE controller
        PROGIF_BUS_MASTER       = 0x80, // bus master (DMA) capable
    DEVCLASS_DISPLAY_CTRL       = 0x03, // display controllers
        SUBCLASS_VGA_COMPAT     = 0x00, // VGA-compatible display controller
        SUBCLASS_NON_VGA        = 0x80, // non-VGA display controller
    DEVCLASS_BRIDGE_DEVICE      = 0x06, // various bridge devices
        SUBCLASS_HOST_BRIDGE    = 0x00, // host-to-PCI bridge
        SUBCLASS_PCI2PCI_BRIDGE = 0x04, // PCI-to-PCI bridge
    DEVCLASS_SERIAL_BUS_CTRL    = 0x0C, // serial bus controllers
        SUBCLASS_USB_CTRL       = 0x03, // Universal serial bus (USB) controller
        PROGIF_OHCI             = 0x10, // conforms to the OpenHCI specification
    DEVCLASS_UNASSIGNED         = 0xFF
};

/** Some common PCI device type definitions. */
enum : uint32_t {
    DEVTYPE_HOST2PCI_BRIDGE     = (DEVCLASS_BRIDGE_DEVICE << 24) |
                                  (SUBCLASS_HOST_BRIDGE << 16),
    DEVTYPE_PCI2PCI_BRIDGE      = (DEVCLASS_BRIDGE_DEVICE << 24) |
                                  (SUBCLASS_PCI2PCI_BRIDGE << 16),
    DEVTYPE_VGA_COMPAT_VIDCTRL  = (DEVCLASS_DISPLAY_CTRL << 24) |
                                  (SUBCLASS_VGA_COMPAT << 16),
    DEVTYPE_IDE_CTRL_BUS_MASTER = (DEVCLASS_MASS_STORAGE_CTRL << 24) |
                                  (SUBCLASS_IDE_CTRL << 16) | (PROGIF_BUS_MASTER << 8),
    DEVTYPE_USB_OHCI_CTRL       = (DEVCLASS_SERIAL_BUS_CTRL << 24) |
                                  (SUBCLASS_USB_CTRL << 16) | (PROGIF_OHCI << 8),
};

/** PCI Vendor IDs for devices used in Power Macintosh computers. */
enum : uint16_t {
    PCI_VENDOR_ATI              = 0x1002,
    PCI_VENDOR_DEC              = 0x1011,
    PCI_VENDOR_OPTi             = 0x1045,
    PCI_VENDOR_TI               = 0x104C, // common abbreviation for Texas Instruments
    PCI_VENDOR_MOTOROLA         = 0x1057,
    PCI_VENDOR_APPLE            = 0x106B,
    PCI_VENDOR_SILICON_IMAGE    = 0x1095,
    PCI_VENDOR_NVIDIA           = 0x10DE,
    PCI_VENDOR_IXMICRO          = 0x10E0, // also known as Integrated Micro Solutions
};

#endif // PCI_DEFS_H
