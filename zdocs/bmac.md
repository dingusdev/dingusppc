The BMac is an Ethernet controller featured in G3 and early G4 Macs. As described by a Linux contributor, this controller "appears to have some parts in common with the Sun "Happy Meal" (HME) controller".

The max frame size is 0x5EE bytes.

It resides on 0xF3011000, with Writing DMA on 0xF3008200 and Reading DMA on 0xF3008300.

## Register Map

| Register Name | Offset |
|:-------------:|:------:|
| XIFC          | 0x000  |
| TXFIFOCSR     | 0x100  |
| TXTH          | 0x110  |
| RXFIFOCSR     | 0x120  |
| MEMADD        | 0x130  |
| XCVRIF        | 0x160  |
| CHIPID        | 0x170  |
| MIFCSR        | 0x180  |
| SROMCSR       | 0x190  |
| TXPNTR        | 0x1A0  |
| RXPNTR        | 0x1B0  |
| STATUS        | 0x200  |
| INTDISABLE    | 0x210  |
| TXRST         | 0x420  |
| TXCFG         | 0x430  |
| IPG1          | 0x440  |
| IPG2          | 0x450  |
| ALIMIT        | 0x460  |
| SLOT          | 0x470  |
| PALEN         | 0x480  |
| PAPAT         | 0x490  |
| TXSFD         | 0x4A0  |
| JAM           | 0x4B0  |
| TXMAX         | 0x4C0  |
| TXMIN         | 0x4D0  |
| PAREG         | 0x4E0  |
| DCNT          | 0x4F0  |
| NCCNT         | 0x500  |
| NTCNT         | 0x510  |
| EXCNT         | 0x520  |
| LTCNT         | 0x530  |
| RSEED         | 0x540  |
| TXSM          | 0x550  |
| RXRST         | 0x620  |
| RXRST         | 0x620  |
| RXCFG         | 0x630  |
| RXMAX         | 0x640  |
| RXMIN         | 0x650  |
| MADD2         | 0x660  |
| MADD1         | 0x670  |
| MADD0         | 0x680  |
| FRCNT         | 0x690  |
| LECNT         | 0x6A0  |
| AECNT         | 0x6B0  |
| FECNT         | 0x6C0  |
| RXSM          | 0x6D0  |
| RXCV          | 0x6E0  |
| HASH3         | 0x700  |
| HASH2         | 0x710  |
| HASH1         | 0x720  |
| HASH0         | 0x730  |
| AFR2          | 0x740  |
| AFR1          | 0x750  |
| AFR0          | 0x760  |
| AFCR          | 0x770  |