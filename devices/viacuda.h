//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

#ifndef VIACUDA_H
#define VIACUDA_H

/** VIA register offsets. */
enum {
    VIA_B    = 0x00, /* input/output register B */
    VIA_A    = 0x01, /* input/output register A */
    VIA_DIRB = 0x02, /* direction B */
    VIA_DIRA = 0x03, /* direction A */
    VIA_T1CL = 0x04, /* low-order  timer 1 counter */
    VIA_T1CH = 0x05, /* high-order timer 1 counter */
    VIA_T1LL = 0x06, /* low-order  timer 1 latches */
    VIA_T1LH = 0x07, /* high-order timer 1 latches */
    VIA_T2CL = 0x08, /* low-order  timer 2 latches */
    VIA_T2CH = 0x09, /* high-order timer 2 counter */
    VIA_SR   = 0x0A, /* shift register */
    VIA_ACR  = 0x0B, /* auxiliary control register */
    VIA_PCR  = 0x0C, /* periheral control register */
    VIA_IFR  = 0x0D, /* interrupt flag register */
    VIA_IER  = 0x0E, /* interrupt enable register */
    VIA_ANH  = 0x0F, /* input/output register A, no handshake */
};

class ViaCuda
{
public:
    ViaCuda();
    ~ViaCuda() = default;

    uint8_t read(int reg);
    void write(int reg, uint8_t value);

private:
    uint8_t via_regs[16]; /* VIA virtual registers */

    void print_enabled_ints(); /* print enabled VIA interrupts and their sources */
};

#endif /* VIACUDA_H */
