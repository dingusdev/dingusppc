The MESH is a SCSI controller used in Power Mac machines.

# Registers

| Register Name    | Number |
|:----------------:|:------:|
| R_COUNT0         | 0x0    |
| R_COUNT1         | 0x1    |
| R_FIFO           | 0x2    |
| R_CMD            | 0x3    |
| R_BUS0STATUS     | 0x4    |
| R_BUS1STATUS     | 0x5    |
| FIFO_CNT         | 0x6    |
| EXCPT            | 0x7    |
| ERROR            | 0x8    |
| INTMASK          | 0x9    |
| INTERRUPT        | 0xA    |
| SOURCEID         | 0xB    |
| DESTID           | 0xC    |
| SYNC             | 0xD    |
| MESHID           | 0xE    |
| SEL_TIMEOUT      | 0xF    |

# Commands

| Command Name     | Number |
|:----------------:|:------:|
| NOP              | 0x0    |
| ARBITRATE        | 0x1    |
| SELECT           | 0x2    |
| COMMAND          | 0x3    |
| STATUS           | 0x4    |
| DATAOUT          | 0x5    |
| DATAIN           | 0x6    |
| MSGOUT           | 0x7    |
| MSGIN            | 0x8    |
| BUSFREE          | 0x9    |
| ENABLE_PARITY    | 0xA    |
| DISABLE_PARITY   | 0xB    |
| ENABLE_RESELECT  | 0xC    |
| DISABLE_RESELECT | 0xD    |
| RESET_MESH       | 0xE    |
| FLUSH_FIFO       | 0xF    |
| SEQ_DMA          | 0x20   |
| SEQ_TARGET       | 0x40   |
| SEQ_ATN          | 0x80   |