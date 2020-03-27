#ifndef ATIRAGE_H
#define ATIRAGE_H
#include <cinttypes>

using namespace std;

/** Mach registers offsets. */
enum {
    ATI_DSP_CONFIG = 0x0020, /*memory buffer registers*/
    ATI_DSP_TOGGLE = 0x0024,
    ATI_TIMER_CFG = 0x0028,
    ATI_MEM_BUF_CNTL = 0x002C,
    ATI_MEM_ADDR_CFG = 0x0034,
    ATI_VGA_DSP_CFG = 0x004C,
    ATI_VGA_DSP_TGL = 0x0050,
    ATI_DSP2_CONFIG = 0x0054,
    ATI_DSP2_TOGGLE = 0x0058,
    ATI_EXT_MEM_CNTL = 0x00AC,
    ATI_MEM_CNTL = 0x00B0,
    ATI_VGA_WP_SEL = 0x00B4,
    ATI_VGA_RP_SEL = 0x00B8,
    ATI_I2C_CNTL_1 = 0x00BC,
    ATI_DST_OFF_PITCH = 0x0100,
    ATI_SRC_OFF_PITCH = 0x0180,
    ATI_DP_PIX_WIDTH = 0x02D0,
    ATI_DST_X_Y = 0x02E8,
    ATI_DST_WIDTH_HEIGHT = 0x02EC,
    ATI_CONTEXT_MASK = 0x0320,
};

class ATIRage
{
public:
    ATIRage();
    ~ATIRage() = default;

    uint32_t read(int reg, int size);
    void write(int reg, uint32_t value, int size);

private:
    uint32_t atirage_membuf_regs[9];    /* ATI Rage Memory Buffer Registers */
    uint32_t atirage_scratch_regs[4];   /* ATI Rage Scratch Registers */
    uint32_t atirage_cmdfifo_regs[3];   /* ATI Rage Command FIFO Registers */
    uint32_t atirage_datapath_regs[12]; /* ATI Rage Data Path Registers*/

    uint8_t atirage_vid_mem[8242880];

    uint8_t atirage_cfg[256] = {
        0x02, 0x10, //Vendor: ATI Technologies
        0x50, 0x47, //Device: 3D Rage Pro PCI

    };

    void atirage_init();
};
#endif /* ATIRAGE_H */