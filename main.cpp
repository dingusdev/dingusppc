//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

//The main runfile - main.cpp
//This is where the magic begins

#include <iostream>
#include <map>
#include <cstring>
#include <cinttypes>
#include <array>
#include <stdio.h>
#include <fstream>
#include <chrono>
#include <ctime>
#include <stdexcept>
#include "ppcemumain.h"
#include "macioserial.h"
#include "macswim3.h"
#include "ppcmemory.h"
#include "viacuda.h"
#include "devices/mpc106.h"
#include "openpic.h"
#include "debugger.h"
//#include <vector>

#define max_16b_int 65535
#define max_32b_int 4294967295
#define ENDIAN_REVERSE16(x) (x >> 8) | (((x) & 0x00FF) << 8)
#define ENDIAN_REVERSE32(x) (x >> 24) | ((x & 0x00FF0000) >> 8) | ((x & 0x0000FF00) << 8) | (x << 24)
#define ENDIAN_REVERSE64(x) (x >> 56) | ((x & 0x00FF000000000000) >> 48)  | ((x & 0x0000FF0000000000) >> 40) | ((x & 0x000000FF00000000) >> 32) | \
                            ((x & 0x00000000FF000000) << 32) | ((x & 0x0000000000FF0000) << 40) | ((x & 0x000000000000FF00) << 48) | ((x & 0x00000000000000FF) << 56)


using namespace std;

SetPRS ppc_state;

bool power_on = 1;
bool is_nubus = 0;

bool grab_branch;
bool grab_exception;
bool grab_return;
bool grab_breakpoint;

uint32_t ppc_cur_instruction; //Current instruction for the PPC
uint32_t ppc_effective_address;
uint32_t ppc_real_address;
uint32_t ppc_next_instruction_address; //Used for branching, setting up the NIA

uint32_t return_value;


//A pointer to a pointer, used for quick movement to one of the following
//memory areas. These are listed right below.
unsigned char * grab_array_ptr;

unsigned char * machine_sysram_mem;
unsigned char * machine_sysconfig_mem;
unsigned char * machine_upperiocontrol_mem;
unsigned char * machine_iocontrolcdma_mem;
unsigned char * machine_loweriocontrol_mem;
unsigned char * machine_interruptack_mem;
unsigned char * machine_iocontrolmem_mem;
unsigned char * machine_f8xxxx_mem;
unsigned char * machine_fexxxx_mem;
unsigned char * machine_fecxxx_mem;
unsigned char * machine_feexxx_mem;
unsigned char * machine_ff00xx_mem;
unsigned char * machine_ff80xx_mem;
unsigned char * machine_sysrom_mem;

uint32_t grab_sysram_size;
uint32_t grab_iocont_size;
uint32_t grab_sysrom_size;

uint32_t ram_size_set;

uint32_t prev_msr_state = 0;
uint32_t cur_msr_state = 0;

//MSR Flags
bool msr_es_change; //Check Endian

uint32_t rom_file_begin; //where to start storing ROM files in memory
uint32_t pci_io_end;

uint32_t rom_file_setsize;

clock_t clock_test_begin; //Used to make sure the TBR does not increment so quickly.

uint32_t write_opcode;
uint8_t write_char;

    /*
//DISK VARIABLES
unsigned char * grab_disk_buf;
bool disk_inserted;
uint64_t disk_offset = 0;
uint32_t disk_word = 0;
*/


uint16_t rev_endian16(uint16_t insert_int){
    return ENDIAN_REVERSE16(insert_int);
}

uint32_t rev_endian32(uint32_t insert_int){
    return ENDIAN_REVERSE32(insert_int);
}

uint64_t rev_endian64(uint64_t insert_int){
    return ENDIAN_REVERSE64(insert_int);
}

//Time Base Register Update Code
//TODO - Make this a bit less hacky somehow.
void ppc_tbr_update()
{
    clock_t clock_test_current = clock();
    uint32_t test_clock = ((uint32_t) (clock_test_current - clock_test_begin)) / CLOCKS_PER_SEC;
    if (test_clock){
        if (ppc_state.ppc_tbr[0] != 0xFFFFFFFF){
            ppc_state.ppc_tbr[0]++;
        }
        else{
            ppc_state.ppc_tbr[0] = 0;
            if (ppc_state.ppc_tbr[1] !=0xFFFFFFFF){
                ppc_state.ppc_tbr[1]++;
            }
            else{
                ppc_state.ppc_tbr[1] = 0;
            }
        }
        clock_test_begin = clock();
        //Placeholder Decrementing Code
        if(ppc_state.ppc_spr[22] > 0){
            ppc_state.ppc_spr[22]--;
        }
    }
}

void ppc_exception_handler(uint32_t exception_type, uint32_t handle_args){
    ppc_next_instruction_address = 0x0; //used to construct a new address
    grab_exception = true;

    printf("MSR VALUE: %x \n Exception Type: %x", ppc_state.ppc_msr, exception_type);

    //Check ROM Exception Prefix
    if (ppc_state.ppc_msr & 0x40){
        ppc_next_instruction_address |= 0xFFF00000;
    }
    else{
        ppc_next_instruction_address &= 0x0000FFFF;
    }

    switch(exception_type){
        case 0x0100: //System Reset
            ppc_state.ppc_spr[26] = ((ppc_cur_instruction + 4) & 0xFFFFFFFC);
            ppc_state.ppc_spr[27] = ppc_state.ppc_msr;
            ppc_state.ppc_msr = (ppc_state.ppc_msr & 0xFFFD0041);
            ppc_next_instruction_address += 0x0100;
            break;
        /**
        case 0x0200: //Machine Check
            break;
        case 0x0300: //DSI
            break;
        case 0x0400: //ISI
            break;
        **/
        case 0x0500: //External Interrupt
            ppc_state.ppc_spr[26] = ((ppc_cur_instruction + 4) & 0xFFFFFFFC);
            ppc_state.ppc_spr[27] = (ppc_state.ppc_msr & 0xFFFF);
            ppc_state.ppc_msr = (ppc_state.ppc_msr & 0xFFFD0041);
            ppc_next_instruction_address += 0x0500;
            break;
        case 0x0600: //Alignment Exception
            ppc_state.ppc_spr[26] = ((ppc_cur_instruction + 4) & 0xFFFFFFFC);
            ppc_state.ppc_spr[27] = (ppc_state.ppc_msr & 0xFFFF);
            ppc_state.ppc_msr = (ppc_state.ppc_msr & 0xFFFD0041);
            ppc_state.ppc_spr[19] = ppc_cur_instruction;
            ppc_next_instruction_address += 0x0600;
        case 0x0700: //Program;
            ppc_state.ppc_spr[26] = ((ppc_cur_instruction + 4) & 0xFFFFFFFC);
            handle_args += 0x10000;
            ppc_state.ppc_spr[27] = handle_args + (ppc_state.ppc_msr & 0xFFFF);
            ppc_state.ppc_msr = (ppc_state.ppc_msr & 0xFFFD0041);
            ppc_next_instruction_address += 0x0700;
            break;
        case 0x0C00: //Sys Call
            ppc_state.ppc_spr[26] = ((ppc_cur_instruction + 4) & 0xFFFFFFFC);
            ppc_state.ppc_spr[27] = (ppc_state.ppc_msr & 0xFFFF);
            ppc_state.ppc_msr = (ppc_state.ppc_msr & 0xFFFD0041);
            ppc_next_instruction_address += 0x0C00;
            break;
        /**
        case 0x0d: //Trace
            break
        **/
        default:
            ppc_state.ppc_spr[26] = ((ppc_cur_instruction + 4) & 0xFFFFFFFC);
            ppc_state.ppc_spr[27] = (ppc_state.ppc_msr & 0xFFFF);
            ppc_state.ppc_msr = (ppc_state.ppc_msr & 0xFFFD0041);
            ppc_next_instruction_address += exception_type;

    }
}

//Initialize the PPC's registers.
void reg_init(){
    for (uint32_t i = 0; i < 32; i++){
        ppc_state.ppc_fpr[i] = 0;
    }
    ppc_state.ppc_pc = 0;
    for (uint32_t i = 0; i < 32; i++){
        ppc_state.ppc_gpr[i] = 0;
    }
    ppc_state.ppc_cr = 0;
    ppc_state.ppc_fpscr = 0;
    ppc_state.ppc_tbr[0] = 0;
    ppc_state.ppc_tbr[1] = 0;

    for (uint32_t i = 0; i < 1024; i++){
        switch(i){
            case 287:
               //Identify as a G3
               //Processor IDS
               // 601 v1  -  00010001
               // 603 v1  -  00030001
               // 604 v1  -  00040001
               // 603e v1 -  00060101
               // 750 v1  -  00080200
                ppc_state.ppc_spr[i] = 0x00080200;
                break;
                /**
            case 528:
            case 536:
                ppc_state.ppc_spr[i] = 0x00001FFE;
                break;
            case 530:
            case 538:
                ppc_state.ppc_spr[i] = 0xC0001FFE;
                break;
            case 532:
            case 540:
                ppc_state.ppc_spr[i] = 0xE0001FFE;
                break;
            case 534:
            case 542:
                ppc_state.ppc_spr[i] = 0xF0001FFE;
                break;
            case 529:
            case 531:
            case 537:
            case 539:
                ppc_state.ppc_spr[i] = 0x00000002;
                break;
            case 533:
            case 541:
                ppc_state.ppc_spr[i] = 0xE0000002;
                break;
            case 535:
                ppc_state.ppc_spr[i] = 0xF0000002;
                break;
            case 543:
                ppc_state.ppc_spr[i] = 0x00000002;
                break;
            **/
            default:
                ppc_state.ppc_spr[i] = 0;
        }
    }


    //Only bit 25 of the MSR is initially set on bootup.
    ppc_state.ppc_msr = 0x40;
    for (uint32_t i = 0; i < 16; i++){
        ppc_state.ppc_sr[i] = 0;
    }
}

//Debugging Functions
uint32_t reg_print(){
    for (uint32_t i = 0; i < 32; i++){
        printf("FPR %d : %" PRIx64 "", i, ppc_state.ppc_fpr[i]);
    }
    ppc_state.ppc_pc = 0;
    for (uint32_t i = 0; i < 32; i++){
        printf("GPR %d : %x", i, ppc_state.ppc_gpr[i]);
    }
    printf("CR : %x", ppc_state.ppc_cr);
    printf("FPSCR : %x", ppc_state.ppc_fpscr);
    printf("TBR 0 : %x", ppc_state.ppc_tbr[0]);
    printf("TBR 1 : %x", ppc_state.ppc_tbr[1]);

    for (uint32_t i = 0; i < 1024; i++){
        printf("SPR %d  : %x", i, ppc_state.ppc_spr[i]);
    }

    printf("CR  : %x", ppc_state.ppc_cr);

    printf("MSR : %x", ppc_state.ppc_msr);

    for (uint32_t i = 0; i < 16; i++){
        printf("SR %d : %x", i, ppc_state.ppc_sr[i]);
    }

    return 0;
}

uint32_t reg_read(){
    uint32_t grab_me = 0;
    std::cout << hex << "TODO: Decide which register to read from; for now, which GPR?" << endl;
    //printf("Which register to read from? 0 - GPR, 1 = FPR, 2 = CR, 3 = FPSCR");
    std::cin >> hex >> grab_me;
    if (grab_me < 32){
        printf("GPR value: %d", ppc_state.ppc_gpr[grab_me]);
    }
    return 0;
}

uint32_t reg_write(){
    uint32_t grab_me = 0;
    std::cout << hex << "TODO: Decide which register to write to; for now, which GPR?" << endl;
    //printf("Which register to write to? 0 - GPR, 1 = FPR, 2 = CR, 3 = FPSCR");
    std::cin >> hex >> grab_me;
    if (grab_me < 32){
        printf("GPR value: %d", ppc_state.ppc_gpr[grab_me]);
    }
    return 0;
}

void execute_interpreter(){
    //Main execution loop for the interpreter.

    while (power_on){
        //printf("PowerPC Address: %x \n", ppc_state.ppc_pc);
        quickinstruction_translate(ppc_state.ppc_pc);
        ppc_main_opcode();
        if (grab_branch & !grab_exception){
            ppc_state.ppc_pc = ppc_next_instruction_address;
            grab_branch = 0;
            ppc_tbr_update();
        }
        else if (grab_return | grab_exception){
            ppc_state.ppc_pc = ppc_next_instruction_address;
            grab_exception = 0;
            grab_return = 0;
            ppc_tbr_update();
        }
        else{
            ppc_state.ppc_pc += 4;
            ppc_tbr_update();
        }
    }
}

void execute_interpreter_bp (uint32_t ppc_break_addr){
    //Main loop like the above, with the only big difference
    //being that this stops on reaching the desired address.

    //It then prints the regs at the time it stopped.

    while (ppc_state.ppc_pc != ppc_break_addr){
        quickinstruction_translate(ppc_state.ppc_pc);
        ppc_main_opcode();
        if (grab_branch & !grab_exception){
            ppc_state.ppc_pc = ppc_next_instruction_address;
            grab_branch = 0;
            ppc_tbr_update();
        }
        else if (grab_return | grab_exception){
            ppc_state.ppc_pc = ppc_next_instruction_address;
            grab_exception = 0;
            grab_return = 0;
            ppc_tbr_update();
        }
        else{
            ppc_state.ppc_pc += 4;
            ppc_tbr_update();
        }
        ppc_cur_instruction = 0;
    }

    reg_print();
}

int main(int argc, char **argv)
{
    ram_size_set = 0x4000000; //64 MB of RAM for the Mac

    rom_file_begin = 0xFFF00000; //where to start storing ROM files in memory
    pci_io_end = 0x83FFFFFF;
    rom_file_setsize = 0x400000;

    reg_init();

    /**
    uint16_t test_endianness = 0x1234;

    //Test for endianess before beginning
    uint8_t grab_result = (uint8_t) (test_endianness >> 8);
    endian_switch endis;

    //Special case for little-endian machines
    switch(unsigned(grab_result)){
    case 18:
        endis = little_end;
        break;
    default:
        endis = big_end;
    }
    **/

    uint32_t opcode_entered = 0; //used for testing opcodes in playground

    std::cout << "DingusPPC - Prototype 5bf4 (7/14/2019)       " << endl;
    std::cout << "Written by divingkatae, (c) 2019.            " << endl;
    std::cout << "This is not intended for general use.        " << endl;
    std::cout << "Use at your own discretion.                  " << endl;

    //Open the ROM File.
    ifstream romFile;

    romFile.open("rom.bin", ios::in|ios::binary);

    if (romFile.fail()){
        cerr << "rom.bin not present. Creating it right now. Please restart this program.\n";
        std::ofstream outfile ("rom.bin", ios::binary);

        outfile << "Testing!" << std::endl;

        outfile.close();

        return 1;
    }

    //Allocate memory for ROM, RAM, and I/O.
    romFile.seekg(0, romFile.end);
    rom_file_setsize = romFile.tellg();
    printf("Rom SIZE: %d \n", rom_file_setsize);
    romFile.seekg (0, romFile.beg);

    /**
    Allocate memory wisely.

    Corresponds (mostly) to the follow memory patterns seen in
    https://www.nxp.com/docs/en/reference-manual/MPC106UM.pdf

    machine_sysram_mem         -  0x00000000 to 0x7FFFFFFF
    machine_upperiocontrol_mem -  0x80000000 to 0x807FFFFF
    machine_iocontrolcdma_mem  -  0x80800000 to 0x80FFFFFF
    machine_loweriocontrol_mem -  0x81000000 to 0xBF7FFFFF
    machine_interruptack_mem   -  0xBFFFFFF0 to 0xBFFFFFFF
    machine_iocontrolmem_mem   -  0xC0000000 to 0xFFBFFFFF
    machine_sysrom_mem         -  0xFFC00000 to 0xFFFFFFFF
    **/

    //grab_disk_buf = (unsigned char*) calloc (32768, 1);
    machine_sysram_mem = (unsigned char*) calloc (67108864, 1);
    machine_sysconfig_mem = (unsigned char*) calloc (2048, 1);
    machine_upperiocontrol_mem = (unsigned char*) calloc (8388608, 1);
    machine_iocontrolcdma_mem = (unsigned char*) calloc (8388608, 1);
    machine_loweriocontrol_mem = (unsigned char*) calloc (33554432, 1);
    machine_interruptack_mem = (unsigned char*) calloc (16, 1);
    machine_iocontrolmem_mem = (unsigned char*) calloc (67108864, 1);
    machine_f8xxxx_mem = (unsigned char*) calloc (4096, 1);
    machine_fexxxx_mem = (unsigned char*) calloc (262144, 1);
    machine_fecxxx_mem = (unsigned char*) calloc (4096, 1);
    machine_feexxx_mem = (unsigned char*) calloc (4096, 1);
    machine_ff00xx_mem = (unsigned char*) calloc (4096, 1);
    machine_ff80xx_mem = (unsigned char*) calloc (1048576, 1);
    machine_sysrom_mem = (unsigned char*) calloc (rom_file_setsize, 1);

    memset(machine_sysram_mem, 0x0, 67108864);
    memset(machine_sysconfig_mem, 0x0, 2048);
    memset(machine_upperiocontrol_mem, 0x0, 8388607);
    memset(machine_iocontrolcdma_mem, 0x0, 8388607);
    memset(machine_interruptack_mem, 0x0, 16);
    memset(machine_iocontrolmem_mem, 0x0, 67108864);
    memset(machine_f8xxxx_mem, 0x0, 4096);
    memset(machine_fexxxx_mem, 0x0, 262144);
    memset(machine_fecxxx_mem, 0x0, 4096);
    memset(machine_feexxx_mem, 0x0, 4096);
    memset(machine_ff00xx_mem, 0x0, 4096);
    memset(machine_ff80xx_mem, 0x0, 1048576);

    grab_sysram_size = sizeof(machine_sysram_mem);
    grab_sysrom_size = rom_file_setsize;

    //Sanity checks - Prevent the input files being too small or too big.
    //Also prevent the ROM area from overflow.
    if (grab_sysrom_size < 0x100000){
        cerr << "ROM File is too small. Must be at least 1 megabyte.\n";
        return 1;
    }
    else if (grab_sysrom_size > 0x400000){
        cerr << "ROM File is too big. Must be no more than 4 megabytes.\n";
        return 1;
    }

    if (ram_size_set < 0x800000){
        cerr << "The RAM size must be at least 8 MB to function.\n";
        return 1;
    }
    else if (ram_size_set > 0x20000000){
        cerr << "RAM too big. Must be no more than 2 GB.\n";
        return 1;
    }

    rom_file_begin = 0xFFFFFFFF - grab_sysrom_size + 1;

    /**
    Test for PowerPC Mac ROMS

    Starting at 0x30D064 (0x30C064 in older ROMS), there is a boot
    identifier string in the ROM. This helps a lot to determine which
    setup is to by used.
    **/
    char * memPPCBlock = new char[5];

    map<string,string> PPCMac_ROMIdentity = {              //Codename Abbreviation for...
        {"Alch", "Performa 6400"},                         //Alchemy
        {"Come", "PowerBook 2400"},                        //Comet
        {"Cord", "Power Mac 5200/6200 series"},            //Cordyceps
        {"Gaze", "Power Mac 6500"},                        //Gazelle
        {"Goss", "Power Mac G3 Beige"},                    //Gossamer
        {"GRX ", "PowerBook G3 Wallstreet"},               //(Unknown)
        {"Hoop", "PowerBook 3400"},                        //Hooper
        {"PBX ", "PowerBook Pre-G3"},                      //(Unknown)
        {"PDM ", "Power Mac G1 or WGS"},                   //(Unknown)
        {"Pip ", "Pippin... uh... yeah..."},               //Pippin
        {"Powe", "Generic Power Mac"},                     //PowerMac?
        {"Spar", "20th Anniversay Mac, you lucky thing."}, //Spartacus
        {"Tanz", "Power Mac 4400"},                        //Tanzania
        {"TNT ", "Power Mac 7xxxx/8xxx series"},           //Trinitrotoluene :-)
        {"Zanz", "A complete engima."},                    //Zanzibar (mentioned in Sheepshaver's code, but no match to any known ROM)
        {"????", "A clone, perhaps?"}                      //N/A (Placeholder ID)
    };

    char configGrab = 0;
    uint32_t configInfoOffset = 0;

    if (grab_sysrom_size == 0x400000){

    romFile.seekg (0x300082, ios::beg); //This is where the place to get the offset is
    romFile.get(configGrab); //just one byte to determine where the identifier string is
    configInfoOffset = (uint32_t)(configGrab & 0xff);

    if (configInfoOffset == 0xC0){
        is_nubus = 1;
    }

    uint32_t configInfoAddr = 0x300000 + (configInfoOffset << 8) + 0x69; //address to check the identifier string
    romFile.seekg (configInfoAddr, ios::beg);
    romFile.read(memPPCBlock, sizeof(uint32_t)); //Only four chars needed to distinguish between codenames

    std::string string_test = std::string(memPPCBlock);

    //Just auto-iterate through the list
    for (auto iter = PPCMac_ROMIdentity.begin(); iter != PPCMac_ROMIdentity.end(); ){

        string redo_me = iter->first;

        if (string_test.compare(redo_me) == 0){
            cout << "The machine is identified as..." << iter->second << endl;
            romFile.seekg (0x0, ios::beg);

            //This is where the first real instruction is in the ROM
            //0xFFF00000 - 0xFFF02FFF is for the exception table,
            //which is stored in all PPC ROMs here, btw.

            //0xFFF00100 is where the reset vector is.
            //In other words, begin executing code here.

            ppc_state.ppc_pc = 0xFFF00100;//Please don't move this from here.
                                          //A strange bug will happen where this will prevent proper branching
            mpc106_init();
            mac_serial_init();
            mac_swim3_init();
            via_cuda_init();
            openpic_init();
            break;
        }
        else{
            iter++;
        }
    }
    }

    //Copy the contents of the IO data and the ROM to memory appropriate locations.
    romFile.read ((char *)machine_sysrom_mem,grab_sysrom_size);

    /*
    //Open the Disk File.
    uint64_t disk_file_setsize = 0;
    ifstream diskFile;

    diskFile.open("disk.img", ios::in|ios::binary);

    if (diskFile){
        diskFile.seekg(0, romFile.end);
        disk_file_setsize = romFile.tellg();
        diskFile.seekg(0, romFile.beg);

        if (disk_file_setsize % 32678 != 0){
            cout << "WARNING - Disk file has improper offsets. Make sure the disk image has even 32 KB sectors." << endl;
        }
        else{
            //Copy the contents of the IO data and the ROM to memory appropriate locations.
            diskFile.read ((char *)machine_sysram_mem,(sizeof(uint8_t)*0x8000));
            disk_inserted = 1;
            disk_offset = 0;
        }
    }
    else{
        disk_inserted = 0;
    }
    */

    clock_test_begin = clock();

    if (argc > 1){
        string checker = argv[1];
        cout << checker << endl;
        if (checker == "fuzzer"){
            //Brute force every instruction that the PPC can interpret.
            //TODO: Change this so that this goes through user-specified instructions.
            ppc_cur_instruction = 0xFFFFFFFF;
            std::cout << "Testing Opcode: " << ppc_cur_instruction  << endl;
            while (ppc_cur_instruction > 0x00000000){
                ppc_main_opcode();
                ppc_cur_instruction--;
            }
        }
        else if ((checker=="1")|(checker=="realtime")|(checker=="/realtime")|(checker=="-realtime")){
            execute_interpreter();
                /*
                if (disk_inserted){
                    if (disk_word == 32768){
                        if (disk_offset < disk_file_setsize){
                            disk_offset += 32768;
                            diskFile.seekg(disk_offset, romFile.beg);
                            char *ptr = ((char*)machine_iocontrolmem_mem + (0xF3008000 % 0x4000000));
                            diskFile.read (ptr,(sizeof(uint8_t)*0x8000));
                            disk_word = 0;
                        }
                        else{
                            disk_offset = 0;
                            diskFile.seekg(0, romFile.beg);
                            char *ptr = ((char*)machine_iocontrolmem_mem + (0xF3008000 % 0x4000000));
                            diskFile.read (ptr,(sizeof(uint8_t)*0x8000));
                            disk_word = 0;
                        }
                    }
                    else{
                        disk_word += 4;
                    }
                }
                */
        }
        else if ((checker=="e")|(checker=="loadelf")|(checker=="/loadelf")|(checker=="-loadelf")){
            ifstream elfFile;
            uint32_t elf_file_setsize = 0;

            char elf_headerchk [4];
            char elf_bformat [1];
            char elf_machine [2];
            char elf_memoffset [4];

            elfFile.seekg(0, elfFile.end);
            elf_file_setsize = elfFile.tellg();
            elfFile.seekg (0, elfFile.beg);

            if (elf_file_setsize < 45){
                cerr << "Elf File TOO SMALL. Please make sure it's a legitimate file.";

                return 1;
            }
            else if (elf_file_setsize > ram_size_set){
                cerr << "Elf File TOO BIG. Please make sure it fits within memory.";

                return 1;
            }

            //There's got to be a better way to get fields of info from an ELF file.
            elfFile.seekg(0x0, ios::beg); //ELF file begins here
            elfFile.read(elf_headerchk, 4);
            elfFile.seekg(0x4, ios::cur);
            elfFile.read(elf_bformat, 1);
            elfFile.seekg(0x8, ios::cur);
            elfFile.read(elf_machine, 2);
            elfFile.seekg(0x6, ios::cur);
            elfFile.read(elf_memoffset, 4);
            elfFile.seekg (0, elfFile.beg);

            bool elf_valid_check = (atoi(elf_headerchk) == 0x7F454C46) && (atoi(elf_bformat) == 1) &&\
                                   ((atoi(elf_machine) == 0) | (atoi(elf_machine) == 20));

            if (!elf_valid_check){
                cerr << "The ELF file inserted was not legitimate. Please try again." << endl;
                return 1;
            }

            elfFile.read ((char *)machine_sysram_mem,ram_size_set);

            if (argc > 2){
                string elfname = string(argv[1]);
                elfname = elfname + ".elf";
                elfFile.open(elfname, ios::in|ios::binary);
            }
            else{
                elfFile.open("test.elf", ios::in|ios::binary);
            }
            if (elfFile.fail()){
                cerr << "Please insert the elf file before continuing.\n";

                return 1;
            }

            ppc_state.ppc_pc = atoi(elf_memoffset);

            execute_interpreter();
        }
        else if ((checker=="until")|(checker=="/until")|(checker=="-until")){
            uint32_t grab_bp = 0x0;

            std::cout << hex << "Enter the address in hex for where to stop execution." << endl;
            cin >> hex >> grab_bp;

            execute_interpreter_bp(grab_bp);
        }
        else if (checker=="disas"){
            if (argc > 2){
                checker = argv[1];
            }
            else{
                checker = "\\compile.txt";
            }
            ifstream inFile (checker, ios::binary | ios::ate);
            if (!inFile) {
                cerr << "Unable to open file for assembling.";
                exit(1);
            }
        }
        else if ((checker=="stepi")|(checker=="/stepi")|(checker=="-stepi")){
            std::cout << hex << "Ready to execute an opcode?" << endl;

            string check_q;
            cin >> check_q;
            getline (cin, check_q);

            if (check_q != "No"){
                quickinstruction_translate(ppc_state.ppc_pc);
                ppc_main_opcode();
                if (grab_branch & !grab_exception){
                    ppc_state.ppc_pc = ppc_next_instruction_address;
                    grab_branch = 0;
                    ppc_tbr_update();
                }
                else if (grab_return | grab_exception){
                    ppc_state.ppc_pc = ppc_next_instruction_address;
                    grab_exception = 0;
                    grab_return = 0;
                    ppc_tbr_update();
                }
                else{
                    ppc_state.ppc_pc += 4;
                    ppc_tbr_update();
                }
            }
        }
        else if ((checker=="stepp")|(checker=="/stepp")|(checker=="-stepp")){
            std::cout << hex << "Ready to execute a page of opcodes?" << endl;

            string check_q;
            cin >> check_q;
            getline (cin, check_q);

            if (check_q != "No"){
                for (int instructions_to_do = 0; instructions_to_do < 256; instructions_to_do++){
                    quickinstruction_translate(ppc_state.ppc_pc);
                    ppc_main_opcode();
                    if (grab_branch & !grab_exception){
                        ppc_state.ppc_pc = ppc_next_instruction_address;
                        grab_branch = 0;
                        ppc_tbr_update();
                    }
                    else if (grab_return | grab_exception){
                        ppc_state.ppc_pc = ppc_next_instruction_address;
                        grab_exception = 0;
                        grab_return = 0;
                        ppc_tbr_update();
                    }
                    else{
                        ppc_state.ppc_pc += 4;
                        ppc_tbr_update();
                    }
                }
            }
        }
        else if ((checker=="play")|(checker=="playground")|(checker=="/playground")|(checker=="-playground")){
            std::cout << hex << "Enter any opcodes for the PPC you want here." << endl;

            while (power_on){
                cin >> hex >> opcode_entered;
                //power off the PPC
                if (opcode_entered == 0x00000000){
                    power_on = 0;
                }
                //print registers
                else if (opcode_entered == 0x00000001){
                    reg_print();
                }
                else if (opcode_entered == 0x00000002){
                    reg_read();
                }
                else if (opcode_entered == 0x00000003){
                    reg_write();
                }
                //test another opcode
                else{
                    ppc_cur_instruction = opcode_entered;
                    quickinstruction_translate(ppc_state.ppc_pc);
                    ppc_main_opcode();
                    if (grab_branch & !grab_exception){
                        ppc_state.ppc_pc = ppc_next_instruction_address;
                        grab_branch = 0;
                        ppc_tbr_update();
                    }
                    else if (grab_return | grab_exception){
                        ppc_state.ppc_pc = ppc_next_instruction_address;
                        grab_exception = 0;
                        grab_return = 0;
                        ppc_tbr_update();
                    }
                    else{
                        ppc_state.ppc_pc += 4;
                        ppc_tbr_update();
                    }
                }
            }
        } else if (checker == "debugger") {
            enter_debugger();
        }
    }
    else{
        std::cout << "                    " << endl;
        std::cout << "Please enter one of the following commands when " << endl;
        std::cout << "booting up DingusPPC...                         " << endl;
        std::cout << "                    " << endl;
        std::cout << "                    " << endl;
        std::cout << "realtime - Run the emulator in real-time.       " << endl;
        std::cout << "loadelf - Load an ELF file to run from RAM.     " << endl;
        std::cout << "debugger - Enter the interactive debugger.      " << endl;
        std::cout << "fuzzer - Test every single PPC opcode.          " << endl;
        std::cout << "stepp - Execute a page of opcodes per key press." << endl;
        std::cout << "playground - Mess around with and opcodes.      " << endl;
    }

    romFile.close();

    //Free memory after the emulation is completed.
    free(machine_sysram_mem);
    free(machine_upperiocontrol_mem);
    free(machine_iocontrolcdma_mem);
    free(machine_loweriocontrol_mem);
    free(machine_interruptack_mem);
    free(machine_iocontrolmem_mem);
    free(machine_f8xxxx_mem);
    free(machine_fexxxx_mem);
    free(machine_fecxxx_mem);
    free(machine_feexxx_mem);
    free(machine_ff00xx_mem);
    free(machine_ff80xx_mem);
    free(machine_sysrom_mem);

    return 0;
}
