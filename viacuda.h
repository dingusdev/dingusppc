//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

#ifndef VIACUDA_H_
#define VIACUDA_H_

#define VIACUDA_B		0x3016000		/* B-side data */
#define VIACUDA_A		0x3016200		/* A-side data */
#define VIACUDA_DIRB	0x3016400		/* B-side direction (1=output) */
#define VIACUDA_DIRA	0x3016600		/* A-side direction (1=output) */
#define VIACUDA_T1CL	0x3016800		/* Timer 1 ctr/latch (low 8 bits) */
#define VIACUDA_T1CH	0x3016A00		/* Timer 1 counter (high 8 bits) */
#define VIACUDA_T1LL	0x3016C00		/* Timer 1 latch (low 8 bits) */
#define VIACUDA_T1LH	0x3016E00		/* Timer 1 latch (high 8 bits) */
#define VIACUDA_T2CL	0x3017000	    /* Timer 2 ctr/latch (low 8 bits) */
#define VIACUDA_T2CH	0x3017200		/* Timer 2 counter (high 8 bits) */
#define VIACUDA_SR		0x3017400		/* Shift register */
#define VIACUDA_ACR		0x3017600		/* Auxiliary control register */
#define VIACUDA_PCR		0x3017800		/* Peripheral control register */
#define VIACUDA_IFR		0x3017A00		/* Interrupt flag register */
#define VIACUDA_IER		0x3017C00		/* Interrupt enable register */
#define VIACUDA_ANH		0x3017E00		/* A-side data, no handshake */

extern uint32_t via_cuda_address;
extern uint8_t via_opcode_store_bit;

extern uint8_t via_write_byte;
extern uint8_t via_read_byte;

extern bool via_cuda_confirm;
extern bool via_cuda_signal_read;
extern bool via_cuda_signal_write;

extern unsigned char porta_ca1, porta_ca2;
extern unsigned char porta_cb1, porta_cb2;
extern uint32_t via_set_mode;

extern void via_ifr_update();
extern void via_t1_update();
extern void via_cuda_init();
extern void via_cuda_read();
extern void via_cuda_write();

#endif // VIACUDA_H
