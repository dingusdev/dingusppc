AWACS is an audio controller present on several Old World Macs and can usually be located at IOBase (ex.: 0xF3000000 for Power Mac G3 Beige) + 0x14000.

New World Macs have a Screamer chip, which is backwards compatible with the AWACS chip, but with some additional capabilities.

# Register Maps

## NuBus Macs
| Register          | Offset | Length
|:-----------------:|:------:|:------:|
| Codec Control     | 0x0    | 3      |
| Codec Status      | 0x4    | 3      |
| Buffer Size       | 0x8    | 2      |
| Phaser            | 0xA    | 4      |
| Sound Control     | 0xE    | 2      |
| DMA In            | 0x12   | 1      |
| DMA Out           | 0x16   | 1      |

## PCI Macs

All registers are 32-bit here.

| Register          | Offset |
|:-----------------:|:------:|
| Sound Control     | 0x0    |
| Codec Control     | 0x10   |
| Codec Status      | 0x20   |
| Clipping Count    | 0x30   |
| Byte Swapping     | 0x40   |
| Frame Count       | 0x50   |

##Sound Control Register bits

| Register                  | Bit Mask |
|:-------------------------:|:--------:|
| Input Subframe Select     | 0x000F   |
| Output Subframe Select    | 0x00F0   |
| Sound Rate                | 0x0700   |
| Error                     | 0x0800   |


Separate volume controls exist for the CD drive and the microphone.

The DMA buffer size is set to be 0x40000 bytes, while the DMA hardware buffer size is set to be 0x2000 bytes.
