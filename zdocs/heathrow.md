The Heathrow is an I/O subsystem and a DMA controller.

It contains a feature control register, an auxiliary control register, and some registers to save states for the DBDMA and the VIA Cuda.

It also contains the emulations for the VIA Cuda, SWIM 3 floppy drive, ESCC, and MESH components.

# Register Map
 
| Register Name       | Offset |
|:-------------------:|:------:|
| Base Address 0      | 0x10   |
| Media Bay Control   | 0x34   |
| Feature Control     | 0x38   |
| Auxiliary Control   | 0x3C   |