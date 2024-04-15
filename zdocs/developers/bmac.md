## Description

BMac aka BigMac is an Ethernet media access controller cell integrated in the
Heathrow and Paddington I/O controllers.
According to a former Apple employee, BMac has been licensed from Sun.
It's therefore not surprising that it has a lot in common with the Sun
"Happy Meal" (HME) controller.

### BMac based Ethernet HW

There are three BMac based HW configurations:

* BMac in a Heathrow I/O + LXT907 PHY used in Power Macintosh G3 (Old World)
* BMac+ in a Paddington I/O + LXT970 aka ST10040 PHY in Power Macintosh Blue & White (New World)
* BMac+ in a Paddington I/O + DP83843 PHY in a Bronze Keyboard PowerBook G3

## Documentation

No official documentation has surfaced so far. Fortunately, the official source
code as well as the STP2002 datasheet provide enough information on the BMac device.

* Apple source code: https://github.com/apple-oss-distributions/AppleBMacEthernet
* STP2002 datasheet: https://people.freebsd.org/~wpaul/STP2002QFP-UG.pdf

## Register Map

BMac registers are located at offset 0x11000 starting from the I/O controller base address.

| Register Name | Offset | Description                                         |
|:-------------:|:------:|:---------------------------------------------------:|
| XIFC          | 0x000  | Transceiver interface configuration register        |
| TXFIFOCSR     | 0x100  | Transceiver FIFO control & status register |
| TXTH          | 0x110  | Transceiver FIFO treshold |
| RXFIFOCSR     | 0x120  | Receiver FIFO control & status register |
| MEMADD        | 0x130  |
| XCVRIF        | 0x160  |
| CHIPID        | 0x170  | Chip aka Ethernet cell identification register |
| MIFCSR        | 0x180  | Media interface control & status register |
| SROMCSR       | 0x190  | Serial EEPROM control & status register             |
| TXPNTR        | 0x1A0  | Transceiver pointer |
| RXPNTR        | 0x1B0  | Receiver pointer |
| STATUS        | 0x200  | Global status register |
| INTDISABLE    | 0x210  | Global interrupt disable register |
| TXRST         | 0x420  | Transceiver software reset                     |
| TXCFG         | 0x430  | Transceiver configuration register             |
| IPG1          | 0x440  | Interpacket gap 1 register |
| IPG2          | 0x450  | Interpacket gap 2 register |
| ALIMIT        | 0x460  | Transceiver attempt limit register |
| SLOT          | 0x470  | Transceiver slot time register |
| PALEN         | 0x480  | Transceiver preamble length register |
| PAPAT         | 0x490  | Transceiver preamble pattern register |
| TXSFD         | 0x4A0  | Transceiver start of frame delimiter (SFD) register |
| JAM           | 0x4B0  | Transceiver jam size register |
| TXMAX         | 0x4C0  | Transceiver maximum frame size register |
| TXMIN         | 0x4D0  | Transceiver minimum frame size register |
| PAREG         | 0x4E0  | Transceiver peak attempts register |
| DCNT          | 0x4F0  | Defer timer (counter ?) |
| NCCNT         | 0x500  | Normal collision counter |
| NTCNT         | 0x510  | Network collision counter |
| EXCNT         | 0x520  | Excessive collision counter |
| LTCNT         | 0x530  | Late collision counter |
| RSEED         | 0x540  | Transceiver random number seed register |
| TXSM          | 0x550  | Transceiver state machine register |
| RXRST         | 0x620  | Receiver software reset register |
| RXCFG         | 0x630  | Receiver configuration register |
| RXMAX         | 0x640  | Receiver maximum frame size register |
| RXMIN         | 0x650  | Receiver minimum frame size register |
| MADD2         | 0x660  | Receiver MAC address 2 register |
| MADD1         | 0x670  | Receiver MAC address 1 register |
| MADD0         | 0x680  | Receiver MAC address 0 register |
| FRCNT         | 0x690  | Receiver frame counter register |
| LECNT         | 0x6A0  | Receiver length error counter register |
| AECNT         | 0x6B0  | Receiver alignment error counter register |
| FECNT         | 0x6C0  | Receiver frame check sum error counter register |
| RXSM          | 0x6D0  | Receiver state machine register |
| RXCV          | 0x6E0  | Receiver code violation register |
| HASH3         | 0x700  | Receiver hash table 3 register |
| HASH2         | 0x710  | Receiver hash table 2 register |
| HASH1         | 0x720  | Receiver hash table 1 register |
| HASH0         | 0x730  | Receiver hash table 0 register |
| AFR2          | 0x740  | Receiver address filter 2 register |
| AFR1          | 0x750  | Receiver address filter 1 register |
| AFR0          | 0x760  | Receiver address filter 0 register |
| AFCR          | 0x770  | Receiver address filter control register |
