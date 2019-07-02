//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

//Functionality for the SCSI

#ifndef MACSCSI_H_
#define MACSCSI_H_

extern uint32_t macscsi_address;
extern uint8_t macscsi_write_byte;
extern uint8_t macscsi_read_byte;

extern void macscsi_init();
extern void macscsi_read();
extern void macscsi_write();

#endif
