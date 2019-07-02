//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

//Functionality for the Serial Ports
// Based on the Zilog Z8530 SCC chip

#ifndef MAC_IO_SERIAL_H
#define MAC_IO_SERIAL_H

extern uint32_t mac_serial_address;
extern uint8_t serial_write_byte;
extern uint8_t serial_read_byte;

extern void mac_serial_init();
extern void mac_serial_read();
extern void mac_serial_write();

#endif
