The following document is intended primarily as a documentation on how the BeBox operates. It is currently not implemented in DingusPPC.

## BeBox

The BeBox is a dual PowerPC 603(e) CPU setup. As it was designed with the intent of using standard PC parts, it has both three PCI slots and five ISA slots. As it was made with media production as its primary audience, it contains both input and output RCA jacks, a microphone input, and MIDI ports.

It also has a unique 37-pin connector dubbed the Geekport to allow hobbyists to create unique devices.

### Known components

* MPC105 PCI Bridge (Codename: Eagle) - Predecessor to the MPC106 (Grackle); Used for bridging between the motherboard and PCI slots
* Intel 82378ZB - Used for bridging between the motherboard and ISA slots
* SYM53C810A - SCSI I/O Processor
* bq3285 - Used for the Real-Time Clock

### Motherboard registers

| Register name               | Address        | Default value  |
|:---------------------------:|:--------------:|:--------------:|
| CPU 0 Interrupt Mask        | 0x7FFFF0F0     |                |
| CPU 1 Interrupt Mask        | 0x7FFFF1F0     |                |
| Interrupt sources           | 0x7FFFF2F0     |                |
| Cross-proccessor interrupts | 0x7FFFF3F0     |                |
| Proc. resets, other stuff   | 0x7FFFF4F0     |                |


### Memory Map

| Region                      | Starting Address | Ending Address   |
|:---------------------------:|:----------------:|:----------------:|
| Main Memory                 | 0x00000000       | 0x3FFFFFFF       |
| Other Memory                | 0x40000000       | 0x7FFFFFFF       |
| ISA I/O                     | 0x80000000       | 0x807FFFFF       |
| PCI I/O                     | 0x81000000       | 0xBEFFFFFF       |
| PCI/ISA Interrupt Ack       | 0xBFFFFFF0       | 0xBFFFFFFF       |
| PCI Memory                  | 0xC0000000       | 0xFEFFFFFF       |
| ROM/Flash                   | 0xFF000000       | 0xFFFFFFFF       |

### Sources

* https://www.haiku-os.org/legacy-docs/benewsletter/Issue1-27.html
* https://www.cebix.net/downloads/bebox/82378zb.pdf
