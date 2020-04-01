#ifndef ATI_RAGE_H
#define ATI_RAGE_H

#include <cinttypes>
#include "pcidevice.h"

using namespace std;

/* PCI related definitions. */
enum {
    ATI_PCI_VENDOR_ID   = 0x1002,
    ATI_RAGE_PRO_DEV_ID = 0x4750,
    ATI_RAGE_GT_DEV_ID  = 0x4754,
};

/** Mach registers offsets. */
enum {
    ATI_CRTC_H_TOTAL_DISP     = 0x0000,
    ATI_CRTC_H_SYNC_STRT_WID  = 0x0004,
    ATI_CRTC_V_TOTAL_DISP     = 0x0008,
    ATI_CRTC_V_SYNC_STRT_WID  = 0x000C,
    ATI_CRTC_INT_CNTL         = 0x0018,
    ATI_CRTC_GEN_CNTL         = 0x001C,
    ATI_DSP_CONFIG            = 0x0020,
    ATI_DSP_TOGGLE            = 0x0024,
    ATI_TIMER_CFG             = 0x0028,
    ATI_MEM_BUF_CNTL          = 0x002C,
    ATI_MEM_ADDR_CFG          = 0x0034,
    ATI_VGA_DSP_CFG           = 0x004C,
    ATI_VGA_DSP_TGL           = 0x0050,
    ATI_DSP2_CONFIG           = 0x0054,
    ATI_DSP2_TOGGLE           = 0x0058,
    ATI_CLOCK_CNTL            = 0x0090,
    ATI_BUS_CNTL              = 0x00A0,
    ATI_EXT_MEM_CNTL          = 0x00AC,
    ATI_MEM_CNTL              = 0x00B0,
    ATI_VGA_WP_SEL            = 0x00B4,
    ATI_VGA_RP_SEL            = 0x00B8,
    ATI_I2C_CNTL_1            = 0x00BC,
    ATI_DAC_CNTL              = 0x00C4,
    ATI_GEN_TEST_CNTL         = 0x00D0,
    ATI_CFG_STAT0             = 0x00E4,
    ATI_DST_OFF_PITCH         = 0x0100,
    ATI_SRC_OFF_PITCH         = 0x0180,
    ATI_DP_PIX_WIDTH          = 0x02D0,
    ATI_DST_X_Y               = 0x02E8,
    ATI_DST_WIDTH_HEIGHT      = 0x02EC,
    ATI_CONTEXT_MASK          = 0x0320,
};

class ATIRage : public PCIDevice
{
public:
    ATIRage(uint16_t dev_id);
    ~ATIRage() = default;

    /* MMIODevice methods */
    uint32_t read(uint32_t reg_start, uint32_t offset, int size);
    void write(uint32_t reg_start, uint32_t offset, uint32_t value, int size);

    bool supports_type(HWCompType type) { return type == HWCompType::MMIO_DEV; };

    /* PCI device methods */
    bool supports_io_space(void) { return true; };

    uint32_t pci_cfg_read(uint32_t reg_offs, uint32_t size);
    void     pci_cfg_write(uint32_t reg_offs, uint32_t value, uint32_t size);

    /* I/O space access methods */
    bool pci_io_read(uint32_t offset, uint32_t size, uint32_t *res);
    bool pci_io_write(uint32_t offset, uint32_t value, uint32_t size) ;

protected:
    uint32_t size_dep_read(uint8_t* buf, uint32_t size);
    void size_dep_write(uint8_t* buf, uint32_t val, uint32_t size);
    bool io_access_allowed(uint32_t offset, uint32_t* p_io_base);
    void write_reg(uint32_t offset, uint32_t value, uint32_t size);

private:
    //uint32_t atirage_membuf_regs[9];    /* ATI Rage Memory Buffer Registers */
    //uint32_t atirage_scratch_regs[4];   /* ATI Rage Scratch Registers */
    //uint32_t atirage_cmdfifo_regs[3];   /* ATI Rage Command FIFO Registers */
    //uint32_t atirage_datapath_regs[12]; /* ATI Rage Data Path Registers*/

    uint8_t block_io_regs[256] = { 0 };

    uint8_t pci_cfg[256] = { 0 }; /* PCI configuration space */
};
#endif /* ATI_RAGE_H */
