# Heathrow ASIC

The Heathrow ASIC is an integrated I/O controller designed for use in Power
Macintosh G3 computers.

Its predecessors are Grand Central and O'Hare ASICs used in other Power Macintosh
computers. As those names suggest, Apple engineers liked to name their I/O controllers
after airports and train stations.

Heathrow and its siblings are collectively referred to as __mac-io__ devices in the
Open Firmware device tree.

## Mac I/O family

The purpose of a Mac I/O (MIO) controller is to bring support for Apple legacy
I/O hardware to the PCI-based Power Macintosh. That legacy hardware has
existed long before Power Macintosh was introduced. It includes:
- versatile interface adapter (VIA)
- Sander-Woz integrated machine (SWIM) that is a floppy disk controller
- CUDA MCU that controls ADB, parameter RAM, real time clock and power management
- serial communication controller (SCC)
- Macintosh Enhanced SCSI Hardware (MESH)

In the 68k Macintosh era, all this hardware was implemented in separate ICs.
In a PCI-compatible Power Macintosh, the above devices are part
of the MIO chip itself. MIO's functional blocks implementing the above devices
are called __cells__, i.e. "VIA cell", "SWIM cell" etc.

MIO itself is PCI compliant while the legacy hardware it emulates isn't.
MIO occupies 512Kb of the PCI memory space divided into registers space and
DMA space. Access to emulated legacy devices is accomplished by reading from/
writing to MIO's PCI address space at predefined offsets.

MIO includes a DMA controller that offers 15 DMA channels implementing
Apple's own DMA protocol called descriptor-based DMA (DBDMA).

Official documentation (that is somewhat incomplete and erroneous) can be
found in the second chapter of the book "Macintosh Technology in the Common
Hardware Reference Platform" by Apple Computer, Inc.

## Heathrow I/O modules

Being a Mac I/O compatible device, the Heathrow ASIC contains the following modules/cells:

* VIA cell for establishing communication with the Cuda MCU (separate IC)
* ATA/IDE cell for controlling ATA-3 compatible drives
* enhanced serial communication controller (ESCC cell)
* Ethernet controller cell funnily called __bmac__
* floppy disk controller (SWIM3 cell)
* SCSI controller (MESH cell)
* integrated DBDMA controller
* interrupt controller
* logic for controlling built-in audio HW
* logic for accessing NVRAM (separate IC)
* logic for controlling a media bay on portables

## PCI configuration space registers

| Register name | Default value  |
|:-------------:|:--------------:|
| VendorID      | 0x106B (Apple) |
| DeviceID      | 0x0010         |
| RevisionID    | 0x01           |
| Class code    | 0xFF0000       |

It looks like Heathrow supports only one Base Address Register - __BAR0__.
It contains base address of the memory-mapped registers programmed by system software
during system initialization.

## Memory-mapped registers

Software communicates with the Heathrow ASIC via memory-mapped registers that
occupy 512 Kb starting from the base address located in the BAR0 register
in the PCI configuration space.

Macintosh firmware configures the Heathrow ASIC to live at address `0xF3000000`.

### General chip control registers

| Offset | Register name     | Description                                    |
|:------:|:-----------------:|:----------------------------------------------:|
| 0x10   | InterruptEvents2  | each "1" bit indicates a pending int           |
| 0x14   | InterruptMask2    | enabling (1) / disabling(0) of specific ints   |
| 0x18   | InterruptClear2   | bit value "1" clears the corresponding int     |
| 0x1C   | InterruptLevels2  | interrupt status for devices 32...63           |
| 0x20   | InterruptEvents1  | each "1" bit indicates a pending int           |
| 0x24   | InterruptMask1    | enabling (1) / disabling(0) of specific ints   |
| 0x28   | InterruptClear1   | bit value "1" clears the corresponding int     |
| 0x2C   | InterruptLevels1  | interrupt status for devices 0...31            |
| 0x30   | UnknownReg30      | Not much is known about this register          |
| 0x34   | HeathrowIDs       | bits for identifying media bay features?       |
| 0x38   | FeatureControl    | bits for controlling Heathrow operation        |
| 0x3C   | AuxControl        | auxiliary control bits                          |

### Device DMA spaces

| Offset | Size in bytes | Space name                    |
|:------:|:-------------:|:-----------------------------:|
| 0x8000 | 256           | MESH SCSI DMA                 |
| 0x8100 | 256           | Floppy controller DMA         |
| 0x8200 | 256           | Ethernet transmit DMA         |
| 0x8300 | 256           | Ethernet receive DMA          |
| 0x8400 | 256           | Serial channel A transmit DMA |
| 0x8500 | 256           | Serial channel A receive DMA  |
| 0x8600 | 256           | Serial channel B transmit DMA |
| 0x8700 | 256           | Serial channel B receive DMA  |
| 0x8800 | 256           | Audio output DMA              |
| 0x8900 | 256           | Audio input DMA               |
| 0x8A00 | 256           | unassigned DMA ?              |
| 0x8B00 | 256           | internal ATA (IDE0) DMA       |
| 0x8C00 | 256           | media bay (?) ATA (IDE1) DMA  |
| 0x8D00 | 256           | unassigned DMA ?              |
| 0x8E00 | 256           | unassigned DMA ?              |
| 0x8F00 | 256           | unassigned DMA ?              |

### Device register spaces

| Offset  | Size          | Space name                               |
|:-------:|:-------------:|:----------------------------------------:|
| 0x10000 | 4 KB          | MESH SCSI controller registers           |
| 0x11000 | 4 KB          | Ethernet (bmac) controller registers     |
| 0x12000 | 4 KB          | Legacy serial (SCC) controller registers |
| 0x13000 | 4 KB          | Serial (ESCC) controller registers       |
| 0x14000 | 4 KB          | Audio codec registers                    |
| 0x15000 | 4 KB          | Floppy (SWIM3) controller  registers     |
| 0x16000 | 8 KB          | VIA registers                            |
| 0x20000 | 4 KB          | IDE0 registers                           |
| 0x21000 | 4 KB          | IDE1 registers                           |
| 0x60000 | 128 KB        | NVRAM access                             |

## Registers description

### General chip control

#### Feature control register

Bit names in the table below were pulled from Open Firmware v2.4.
The field "Description" represents my personal attempt to describe the function
of those bits based on publicly available Apple and Linux sources.

| Bit # | Active | Name          | Description                             |
|:-----:|:------:|:-------------:|:---------------------------------------:|
| 0     | high   | in_use_led    | "1" enables monitor sense on G3 desktop |
| 1     | low    | -mb_pwr       | media bay power on/off                  |
| 2     | high   | pci_mb_en     | enable media bay PCI (?)                |
| 3     | high   | ide_mb_en     | enable media bay ATA (IDE1)             |
| 4     | high   | floppy_en     | enable floppy disk controller cell      |
| 5     | high   | ide_int_en    | enable internal ATA (IDE0)              |
| 6     | low    | -ide0_reset   | reset internal ATA                      |
| 7     | low    | -mb_reset     | reset media bay                         |
| 8     | high   | iobus_enable  | *not available*                         |
| 9     | high   | scc_enable    | enable SCC controller cell              |
| 10    | high   | scsi_cell_en  | enable MESH SCSI controller cell        |
| 11    | high   | swim_cell_en  | enable SWIM3 floppy controller cell     |
| 12    | high   | snd_pwr       | *not available*                         |
| 13    | high   | snd_clk_en    | enable sound chip clock (?)             |
| 14    | high   | scc_a_enable  | enable SCC channel A                    |
| 15    | high   | scc_b_enable  | enable SCC channel B                    |
| 16    | low    | -port_via     | switch VIA cell to portable mode (?)    |
| 16    | high   | desktop_via   | switch VIA cell to desktop mode (?)     |
| 17    | low    | -pwm          | *not available*                         |
| 17    | high   | mon_id        | *not available*                         |
| 18    | low    | -hookpb       | *not available*                         |
| 18    | high   | mb_cnt        | *not available*                         |
| 19    | low    | -swimiii      | floppy controller in SWIM3 mode (?)     |
| 19    | high   | clonefloppy   | floppy controller in PC-compat mode (?) |
| 20    | high   | aud22run      | *not available*                         |
| 21    | high   | scsi_linkmode | *not available*                         |
| 22    | high   | arb_bypass    | disable internal PCI arbiter (?)        |
| 23    | low    | -ide1_reset   | reset media bay ATA                     |
| 24    | high   | slow_scc_pclk | *not available*                         |
| 25    | high   | reset_scc     | reset serial (SCC) controller cell      |
| 26    | high   | mfdc_cell_en  | enable PC-compat floppy controller (?)  |
| 27    | high   | use_mfdc      | use PC floppy ctrl instead of SWIM3 ?   |
| 29    | high   | enet_ctrl_en  | enable Ethernet controller cell (?)     |
| 30    | high   | enet_xcvr_en  | enable Ethernet Transceiver (?)         |
| 31    | high   | enet_reset    | reset Ethernet cell                     |


### Cell-specific registers

#### ATA cell registers

##### Internal ATA (IDE0)

| Offset from I/O base | Width in bits |  Function on read | Function on write |
|:--------------------:|:-------------:|:-----------------:|:-----------------:|
| 0x20000              | 16            | Data              | Data              |
| 0x20010              | 8             | Error             | Features          |
| 0x20020              | 8             | Sector Count      | Sector Count      |
| 0x20030              | 8             | Sector Number     | Sector Number     |
| 0x20040              | 8             | Cylinder Low      | Cylinder Low      |
| 0x20050              | 8             | Cylinder High     | Cylinder High     |
| 0x20060              | 8             | Drive/Head        | Drive/Head        |
| 0x20070              | 8             | Status            | Command           |
| 0x20160              | 8             | Alternate Status  | Device Control    |
| 0x20200              | 32            | Timing Config     | Timing Config     |

##### Media Bay ATA (IDE1)

| Offset from I/O base | Width in bits |  Function on read | Function on write |
|:--------------------:|:-------------:|:-----------------:|:-----------------:|
| 0x21000              | 16            | Data              | Data              |
| 0x21010              | 8             | Error             | Features          |
| 0x21020              | 8             | Sector Count      | Sector Count      |
| 0x21030              | 8             | Sector Number     | Sector Number     |
| 0x21040              | 8             | Cylinder Low      | Cylinder Low      |
| 0x21050              | 8             | Cylinder High     | Cylinder High     |
| 0x21060              | 8             | Drive/Head        | Drive/Head        |
| 0x21070              | 8             | Status            | Command           |
| 0x21160              | 8             | Alternate Status  | Device Control    |
| 0x21200              | 32            | Timing Config     | Timing Config     |

##### Timing Config Register

The timing config register is specific to Apple's ATA implementation. According
with
[this source](https://android.googlesource.com/kernel/msm/+/android-wear-5.0.2_r0.1/drivers/ata/pata_macio.c),
this register is used to control timings for Multiword DMA (MDMA) and Programmed I/O (PIO) modes.

This register has the following format:

| 31 ... 22  | 21 | 20 ... 16     | 15 ... 11   | 10 | 9 ... 5      | 4 ... 0    |
|------------|----|---------------|-------------|----|--------------|------------|
| Unused (?) | HT | MDMA Recovery | MDMA Access | E  | PIO Recovery | PIO Access |

I don't know what Recovery and Access values mean. It seems that all values are
in 30 ns units. It corresponds to one PCI cycle when running at a 33 MHz clock speed.

Both PIO and MDMA recovery values are specified in `x / 30` units after subtracting
a constant offset. This offset is 120 ns for PIO transfers and 30 ns for MDMA transfers.

- The "HT" stands for _HalfTick_. When set, 15 ns will be added to both MDMA access
and recovery times. Otherwise, 30 ns are added to the MDMA recovery time.
- The "E" bit, when set, adds 30 ns to the final PIO timing.

The [official Apple source](https://opensource.apple.com/source/HeathrowATA/HeathrowATA-108.3.1/HeathrowATA.cpp.auto.html)
specifies the following default values for each PIO mode:

| Mode # | Cycle time | Transfer rate | Recovery | Access |
|:------:|:----------:|:-------------:|:--------:|:------:|
| 0      | 600 ns     | 3,33 MB/s     | 420 ns   | 180 ns |
| 1      | 390 ns     | 5,13 MB/s     | 240 ns   | 150 ns |
| 2      | 360 ns     | 5,56 MB/s     | 180 ns   | 180 ns |
| 3      | 330 ns     | 6,1  MB/s     | 180 ns   | 150 ns |
| 4      | 300 ns     | 6,67 MB/s     | 150 ns   | 150 ns |

Timing values for the PIO modes 2...4 deviate from the corresponding values given
in the ATA-3 specification.

PIO cycle time can be calculated using the following equation:
```
PIO_Cycle_ns = PIO_Recovery * 30 + 120 + PIO_Access * 30 + E * 30
```

Multiword DMA modes are specified as follows:

| Mode # | Cycle time | Transfer rate | Recovery | Access |
|:------:|:----------:|:-------------:|:--------:|:------:|
| 0      | 480 ns     | 4,17 MB/s     | 240 ns   | 240 ns |
| 1      | 360 ns     | 5,56 MB/s     | 180 ns   | 180 ns |
| 2      | 270 ns     | 7,4  MB/s     | 135 ns   | 135 ns |
| 3      | 240 ns     | 8,33 MB/s     | 120 ns   | 120 ns |
| 4      | 210 ns     | 9,52 MB/s     | 105 ns   | 105 ns |
| 5      | 180 ns     | 11,11 MB/s    | 90 ns    | 90 ns  |
| 6      | 150 ns     | 13,33 MB/s    | 75 ns    | 75 ns  |
| 7      | 120 ns     | 16,67 MB/s    | 45 ns    | 75 ns  |

Modes 0, 6 and 7 from the table above correspond to the MDMA modes 0...2 specified
in the ATA-2 standard.

MDMA cycle time can be calculated using the following equation:
```
MDMA_Cycle_ns = MDMA_Recovery * 30 + MDMA_Access * 30 + 30
```
