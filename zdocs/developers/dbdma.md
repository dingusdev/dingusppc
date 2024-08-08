The Description-Based Direct Memory Access (DBDMA) relies on memory-based descriptions, minimizing CPU interrupts.

| Channel           | Number |
|:-----------------:|:------:|
| SCSI0             | 0x0    |
| FLOPPY            | 0x1    |
| ETHERNET TRANSMIT | 0x2    |
| ETHERNET RECIEVE  | 0x3    |
| SCC TRANSMIT A    | 0x4    |
| SCC RECIEVE A     | 0x5    |
| SCC TRANSMIT B    | 0x6    |
| SCC RECIEVE B     | 0x7    |
| AUDIO OUT         | 0x8    |
| AUDIO IN          | 0x9    |
| SCSI1             | 0xA    |

| Register          | Offset |
|:-----------------:|:------:|
| ChannelControl    | 0x00   |
| ChannelStatus     | 0x04   |
| CommandPtrLo      | 0x0C   |
| InterruptSelect   | 0x10   |
| BranchSelect      | 0x14   |
| WaitSelect        | 0x18   |

| Command           | Value  |
|:-----------------:|:------:|
| OUTPUT_MORE       | 0x0    |
| OUTPUT_LAST       | 0x1    |
| INPUT_MORE        | 0x2    |
| INPUT_LAST        | 0x3    |
| STORE_QUAD        | 0x4    |
| LOAD_QUAD         | 0x5    |
| NOP               | 0x6    |
| STOP              | 0x7    |

| Key Name          | Value  |
|:-----------------:|:------:|
| KEY_STREAM0       | 0x0    |
| KEY_STREAM1       | 0x1    |
| KEY_STREAM2       | 0x2    |
| KEY_STREAM3       | 0x3    |
| KEY_REGS          | 0x5    |
| KEY_SYSTEM        | 0x6    |
| KEY_DEVICE        | 0x7    |

# References

* https://stuff.mit.edu/afs/sipb/contrib/doc/specs/protocol/chrp/chrp_io.pdf
* https://stuff.mit.edu/afs/sipb/contrib/doc/specs/protocol/chrp/chrp_hrpa.pdf