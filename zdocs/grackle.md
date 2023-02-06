The MPC106, codenamed "Grackle", is a PCI bridge/memory controller. Its predecessor was the MPC105, codenamed "Eagle". It replaced an Apple-made host bridge codenamed "Bandit", part number 343S1125.

Unlike the CPU, which generally runs in big-endian mode, the Grackle runs in little-endian mode in compliance with the PCI standard. This usually means that to get the result in the correct endian, the PowerPC must load and store byte-reversed inputs and results.

By default, Grackle operates on Address Map B.

CONFIG_ADDR can be found at any address in the range 0xFEC00000–0xFEDFFFFF, while CONFIG_DAT can be found at any address in the range 0xFEE00000–0xFEEFFFFF. The device trees also establish 0xFEF00000 as the 8259 interrupt acknowledgement register.

PCI Config addresses work as follows:

bus << 16 | device << 11 | function <<  8 | offset

# Revisions

Revisions under 4.0 could allow up to 75 MHz, whereas 4.0 and newer can allow up to 83 MHz.

# General Data

Vendor ID: 0x1057 (Motorola)
Device ID: 0x0002 (MPC106)

Within the Mac's own device tree, this is usually device 0.

It also spans for 0x7F000000 bytes starting from 0x80000000.

# Adding PCI devices to dingusppc

The dingusppc CLI can add known PCI devices to a PCI slot (A1, B1, C1).

Only the following device numbers connected to grackle are probed by Open Firmware:

- @0 used by pci [grackle]
- @c slot PERCH interrupt 0x1c
- @d slot A1 interrupt 0x17
- @e slot B1 interrupt 0x18
- @f slot C1 interrupt 0x19
- @10 used by mac-io [heathrow] which includes many devices with different interrupts
- @12 slot F1 interrupt 0x16 used by AtiRageGT or AtiRagePro

With minor additions to source code, dingusppc can add a known PCI device to any device number between @1 and @1f except for @10 and @12. A nvramrc patch can make Open Firmware probe the other device numbers. An OS might be able to probe these other device numbers even if they are not probed by Open Firmware.

A better approach may be to attach a PCI bridge to one of the supported slots. Then additional devices can be attached to the PCI bridge. For a PCI bridge, Open Firmware will probe device numbers between @0 and @f. Some nvramrc patches could maybe make Open Firmware probe the PCI bridge devices up to device number @1f. PCI bridges can be nested. In any case, each device number can have up to 8 functions numbered from 0 to 7.

The dingusppc CLI doesn't yet support adding devices to PCI bridges or adding function numbers 1 to 7.
