//DingusPPC
//Written by divingkatae and maximum
//(c)2018-20 (theweirdo)     spatium
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 or powermax#2286 on Discord)

//The main runfile - main.cpp
//This is where the magic begins

#include <thirdparty/loguru.hpp>
#include <iostream>
#include <map>
#include <cstring>
#include <cinttypes>
#include <array>
#include <stdio.h>
#include <fstream>
#include <stdexcept>
#include "ppcemu.h"
#include "ppcmmu.h"
#include "memreadwrite.h"
#include "devices/mpc106.h"
#include "debugger/debugger.h"
#include "devices/machineid.h"
#include "devices/macio.h"
#include "devices/mpc106.h"

using namespace std;

/**
    Power Macintosh ROM identification string

    is located in the ConfigInfo structure starting at 0x30D064 (PCI Macs)
    or 0x30C064 (Nubus Macs). This helps a lot to determine which
    hardware is to be used.
*/
static const map<string,string> PPCMac_ROMIdentity = { //Codename Abbreviation for...
    {"Alch", "Performa 6400"},                         //Alchemy
    {"Come", "PowerBook 2400"},                        //Comet
    {"Cord", "Power Mac 5200/6200 series"},            //Cordyceps
    {"Gaze", "Power Mac 6500"},                        //Gazelle
    {"Goss", "Power Mac G3 Beige"},                    //Gossamer
    {"GRX ", "PowerBook G3 Wallstreet"},               //(Unknown)
    {"Hoop", "PowerBook 3400"},                        //Hooper
    {"PBX ", "PowerBook Pre-G3"},                      //(Unknown)
    {"PDM ", "Nubus Power Mac or WGS"},                //Piltdown Man (6100/7100/8100)
    {"Pip ", "Pippin... uh... yeah..."},               //Pippin
    {"Powe", "Generic Power Mac"},                     //PowerMac?
    {"Spar", "20th Anniversay Mac, you lucky thing."}, //Spartacus
    {"Tanz", "Power Mac 4400"},                        //Tanzania
    {"TNT ", "Power Mac 7xxxx/8xxx series"},           //Trinitrotoluene :-)
    {"Zanz", "A complete engima."},                    //Zanzibar (mentioned in Sheepshaver's code, but no match to any known ROM)
    {"????", "A clone, perhaps?"}                      //N/A (Placeholder ID)
};

HeathrowIC  *heathrow = 0;
GossamerID  *machine_id;


int main(int argc, char **argv)
{

    std::cout << "DingusPPC - Prototype 5bf4 (7/14/2019)       " << endl;
    std::cout << "Written by divingkatae, (c) 2019.            " << endl;
    std::cout << "This is not intended for general use.        " << endl;
    std::cout << "Use at your own discretion.                  " << endl;

    if (argc > 1) {
        string checker = argv[1];
        cout << checker << endl;

        if ((checker == "1") || (checker == "realtime") || \
            (checker == "-realtime") || (checker == "/realtime")) {
            loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
            loguru::g_preamble_date = false;
            loguru::g_preamble_time = false;
            loguru::g_preamble_thread = false;
            loguru::init(argc, argv);
            loguru::add_file("dingusppc.log", loguru::Append, 0);
            //Replace the above line with this for maximum debugging detail:
            //loguru::add_file("dingusppc.log", loguru::Append, loguru::Verbosity_MAX);
        }
        else if ((checker == "debugger") || (checker == "/debugger") ||
            (checker == "-debugger")) {
            loguru::g_preamble_date = false;
            loguru::g_preamble_time = false;
            loguru::g_preamble_thread = false;
            loguru::init(argc, argv);
            loguru::g_stderr_verbosity = 0;
        }

        uint32_t rom_filesize;

        /* Init virtual CPU and request MPC750 CPU aka G3 */
        ppc_cpu_init(PPC_VER::MPC750);


        LOG_F(INFO, "Checking for ROM file");

        //Open the ROM File.
        ifstream romFile;

        romFile.open("rom.bin", ios::in|ios::binary);

        if (romFile.fail()){
            cerr << "rom.bin not present. Please provide an appropriate ROM file"
                << " and restart this program.\n";

            romFile.close();
            return 1;
        }

        //Calculate and validate ROM file size.
        romFile.seekg(0, romFile.end);
        rom_filesize = (uint32_t) romFile.tellg();
        LOG_F(INFO, "Rom SIZE: %d \n", rom_filesize);
        romFile.seekg (0, romFile.beg);

        if (rom_filesize != 0x400000){
            cerr << "Unsupported ROM File size. Expected size is 4 megabytes.\n";
            romFile.close();
            return 1;
        }

        char configGrab = 0;
        uint32_t configInfoOffset = 0;

        romFile.seekg (0x300082, ios::beg); //This is where the place to get the offset is
        romFile.get(configGrab); //just one byte to determine ConfigInfo location
        configInfoOffset = (uint32_t)(configGrab & 0xff);

        uint32_t configInfoAddr = 0x300000 + (configInfoOffset << 8) + 0x69; //address to check the identifier string
        char memPPCBlock[5]; //First four chars are enough to distinguish between codenames
        romFile.seekg (configInfoAddr, ios::beg);
        romFile.read(memPPCBlock, 4);
        memPPCBlock[4] = 0;
        uint32_t rom_id = READ_DWORD_BE_A(memPPCBlock);

        std::string string_test = std::string(memPPCBlock);

        //Just auto-iterate through the list
        for (auto iter = PPCMac_ROMIdentity.begin(); iter != PPCMac_ROMIdentity.end(); ){

            string redo_me = iter->first;

            if (string_test.compare(redo_me) == 0){
                const char* check_me = iter->second.c_str();
                LOG_F(INFO, "The machine is identified as... %s \n", check_me);
                romFile.seekg (0x0, ios::beg);
                break;
            }
            else{
                iter++;
            }
        }

    switch(rom_id) {
        case 0x476F7373: {
                LOG_F(INFO, "Initialize Gossamer hardware... \n");
                MPC106 *mpc106 = new MPC106();
                mem_ctrl_instance = mpc106;
                if (!mem_ctrl_instance->add_rom_region(0xFFC00000, 0x400000)) {
                    LOG_F(ERROR, "Failed to Gossamer hardware... \n");
                    delete(mem_ctrl_instance);
                    romFile.close();
                    return 1;
                }
                machine_id = new GossamerID(0x3d8c);
                mpc106->add_mmio_region(0xFF000004, 4096, machine_id);

                heathrow = new HeathrowIC();
                mpc106->pci_register_device(16, heathrow);
                cout << "done" << endl;
            }
            break;
        default:
            LOG_F(INFO, "This machine not supported yet. \n");
            return 1;
    }

        /* Read ROM file content and transfer it to the dedicated ROM region */
        unsigned char *sysrom_mem = new unsigned char[rom_filesize];
        romFile.read ((char *)sysrom_mem, rom_filesize);
        mem_ctrl_instance->set_data(0xFFC00000, sysrom_mem, rom_filesize);
        romFile.close();
        delete[] sysrom_mem;


        if ((checker == "1") || (checker == "realtime") || \
            (checker == "-realtime") || (checker == "/realtime")) {
            ppc_exec();
        } else if ((checker == "debugger") || (checker == "/debugger") ||
            (checker == "-debugger")) {
            enter_debugger();
        }
    }
    else {
        std::cout << "                    " << endl;
        std::cout << "Please enter one of the following commands when " << endl;
        std::cout << "booting up DingusPPC...                         " << endl;
        std::cout << "                    " << endl;
        std::cout << "                    " << endl;
        std::cout << "realtime - Run the emulator in real-time.       " << endl;
        std::cout << "debugger - Enter the interactive debugger.      " << endl;
    }

    /* Free memory after the emulation is completed. */
    delete(heathrow);
    delete(machine_id);
    delete(mem_ctrl_instance);

    return 0;
}
