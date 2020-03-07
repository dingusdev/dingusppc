The Heathrow is an I/O subsystem and a DMA controller.

It contains a feature control register, an auxiliary control register, and some registers to save states for the DBDMA and the VIA Cuda.

It also contains the emulations for the VIA Cuda, SWIM 3 floppy drive, ESCC, and MESH components.

# Register Map
 
| Register Name       | Offset |
|:-------------------:|:------:|
| Interrupt Events 2  | 0x10   |
| Interrupt Mask 2    | 0x14   |
| Interrupt Clear 2   | 0x18   |
| Interrupt Levels 2  | 0x1C   |
| Interrupt Events 1  | 0x20   |
| Interrupt Mask 1    | 0x24   |
| Interrupt Clear 1   | 0x28   |
| Interrupt Levels 1  | 0x2C   |
| Chassis Light Color | 0x32   |
| Media Bay Control   | 0x34   |
| Feature Control     | 0x38   |
| Auxiliary Control   | 0x3C   |