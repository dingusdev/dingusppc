The ATI Rage is a video card that comes bundled with early Power Mac G3s and New World Macs (like the first revisions of the iMac G3).

# Memory Map

The ATI Rage can usually be located at IOBase (ex.: 0xF3000000 for Power Mac G3 Beige) + 0x9000. However, the video memory appears to be at 0x81000000 and is capped at 8 MB.

# Register Map

| Register Name       | Offset |
|:-------------------:|:------:|
| BUS_CNTL            | 0xA0   |
| EXT_MEM_CNTL        | 0xAC   |
| MEM_CNTL            | 0xB0   |
| MEM_VGA_WP_SEL      | 0xB4   |
| MEM_VGA_RP_SEL      | 0xB8   |
| GEN_TEST_CNTL       | 0xD0   |
| CONFIG_CNTL         | 0xDC   |
| CONFIG_CHIP_ID      | 0xE0   |
| CONFIG_STAT0        | 0xE4   |
| DST_CNTL            | 0x130  |
| SRC_OFF_PITCH       | 0x180  |
| SRC_X               | 0x184  |
| SRC_Y               | 0x188  |
| SRC_Y_X             | 0x18C  |
| SRC_WIDTH1          | 0x190  |
| SRC_HEIGHT1         | 0x194  |
| SRC_HEIGHT1_WIDTH1  | 0x198  |
| SRC_CNTL            | 0x1B4  |
| SCALE_3D_CNTL       | 0x1FC  |
| HOST_DATA0          | 0x200  |
| HOST_CNTL           | 0x240  |
| DP_PIX_WIDTH        | 0x2D0  |
| DP_SRC              | 0x2D8  |