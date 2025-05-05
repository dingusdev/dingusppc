The AMIC is the I/O controller used in the Power Mac 6100. Physically, it's located at 0x50F00000.

It also:

* Controls the video timing signals

## Subdevices

| Subdevice      | Range                |
|:--------------:|:--------------------:|
| VIA 1          | 0x0 - 0x1FFF         |
| SCC            | 0x4000 - 0x5FFF      |
| MACE           | 0xA000 - 0xBFFF      |
| SCSI           | 0x10000 - 0x11FFF    |
| AWACS          | 0x14000 - 0x15FFF    |
| SWIM III       | 0x16000 - 0x17FFF    |
| VIA 2          | 0x26000 - 0x27FFF    |
| Video          | 0x28000 - 0x29FFF    |
| DMA            | 0x31000 - 0x32FFF    |
| Mem Control    | 0x40000 - 0x41FFF    |