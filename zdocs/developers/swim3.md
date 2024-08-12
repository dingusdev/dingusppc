The SWIM 3 (Sanders-Wozniak integrated machine 3) is the floppy drive disk controller. As can be inferred by the name, the SWIM III chip is the improvement of a combination of floppy disk driver designs by Steve Wozniak (who worked on his own floppy drive controller for early Apple computers) and Wendell B. Sander (who worked on an MFM-compatible IBM floppy drive controller).

The original SWIM chip supported Group Coded Recording (GCR)-formatted floppy disks in 400 kb and 800 kb formats, while the newer SWIM2 and SWIM3 chips include support for 1.44 MB floppies in MFM format.

The SWIM chip is resided on the logic board physically and is located at IOBase + 0x15000 in the device tree. It sits between the I/O controller and the floppy disk connector. Its function is to translate the I/O commands to specialized signals to drive the floppy disk drive, i.e. disk spinning speed, head position, phase sync, etc.

Unlike its predecessors, it allowed some DMA capability to transfer read or write data.

The floppy drives themselves were provided by Sony.

Some New World Macs such as Rev. A iMac G3s do have a SWIM 3 driver present, but this normally goes unused due to no floppy drive being connected.

# Registers

| Offset | Functionality                   |
|:------:|:-------------------------------:|
| 0x0    | Data                            |
| 0x1    | Timer                           |
| 0x2    | Error                           |
| 0x3    | Parameter Data                  |
| 0x4    | Phase                           |
| 0x5    | Setup                           |
| 0x6    | Read Mode (Read), Mode (Write)  |
| 0x7    | Handshake (Read), Mode (Write)  |
| 0x8    | Interrupt                       |
| 0x9    | Step                            |
| 0xA    | Current Track                   |
| 0xB    | Current Sector                  |
| 0xC    | Gap/Format                      |
| 0xD    | First Sector                    |
| 0xE    | Sectors to Transfer             |
| 0xF    | Interrupt Mask                  |