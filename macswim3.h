//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

#ifndef MAC_SWIM3_H
#define MAC_SWIM3_H

extern uint32_t mac_swim3_address;
extern uint8_t swim3_write_byte;
extern uint8_t swim3_read_byte;

extern void mac_swim3_init();
extern void mac_swim3_read();
extern void mac_swim3_write();

#endif // MAC_IO_SERIAL_H
