//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

//Functionality for the DAVBus (Sound Bus + Screamer?)

#ifndef DAVBUS_H_
#define DAVBUS_H_

extern uint32_t davbus_address;
extern uint32_t davbus_write_word;
extern uint32_t davbus_read_word;

extern void davbus_init();
extern void davbus_read();
extern void davbus_write();

#endif

