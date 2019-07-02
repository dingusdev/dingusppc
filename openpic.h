//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

//Functionality for the OPENPIC

#ifndef OPENPIC_H_
#define OPENPIC_H_

extern uint32_t openpic_address;
extern uint32_t openpic_write_word;
extern uint32_t openpic_read_word;

extern void openpic_init();
extern void openpic_read();
extern void openpic_write();

#endif
