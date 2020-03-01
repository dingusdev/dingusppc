The MPC106, codenamed "Grackle", is a memory controller and PCI host bridge. Its predecessor was the MPC105, codenamed "Bandit".

Unlike the CPU, the Grackle runs in little-endian mode in compliance with the PCI standard. This usually means that to get the result in the correct endian, the PowerPC must load and store byte-reversed inputs and results.

By default, Grackle operates on Address Map B.

CONFIG_ADDR can be found at any address in the range 0xFEC00000–0xFEDFFFFF, while CONFIG_DAT can be found at any address in the range 0xFEE00000–0xFEEFFFFF

PCI Config addresses work as follows:

bus << 16 | device << 11 | function <<  8 | offset

#Revisions

Revisions under 4.0 could allow up to 75 MHz, whereas 4.0 and newer can allow up to 83 MHz.
