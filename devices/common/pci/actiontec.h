/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-22 divingkatae and maximum
                      (theweirdo)     spatium

(Contact divingkatae#1017 or powermax#2286 on Discord for more info)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/*

00:0e.0 PCI bridge [0604]: Actiontec Electronics Inc Mini-PCI bridge [1668:0100] (rev 11) (prog-if 00 [Normal decode])
	Control: I/O+ Mem+ BusMaster+ SpecCycle- MemWINV- VGASnoop- ParErr- Stepping- SERR- FastB2B- DisINTx-
	Status: Cap+ 66MHz- UDF- FastB2B+ ParErr- DEVSEL=medium >TAbort- <TAbort- <MAbort- >SERR- <PERR- INTx-
	Latency: 32, Cache Line Size: 32 bytes
	Interrupt: pin ? routed to IRQ 255
	Bus: primary=00, secondary=01, subordinate=01, sec-latency=0
	I/O behind bridge: 00001000-00001fff [size=4K]
	Memory behind bridge: 80800000-808fffff [size=1M]
	Prefetchable memory behind bridge: 80800000-807fffff [disabled]
	Secondary status: 66MHz- FastB2B+ ParErr- DEVSEL=medium >TAbort- <TAbort- <MAbort+ <SERR- <PERR-
	BridgeCtl: Parity- SERR- NoISA+ VGA- VGA16- MAbort- >Reset- FastB2B-
		PriDiscTmr- SecDiscTmr- DiscTmrStat- DiscTmrSERREn-
	Capabilities: [80] Power Management version 2
		Flags: PMEClk- DSI- D1+ D2+ AuxCurrent=0mA PME(D0-,D1+,D2+,D3hot+,D3cold+)
		Status: D0 NoSoftRst- PME-Enable- DSel=0 DScale=0 PME-
		Bridge: PM- B3+
	Capabilities: [90] CompactPCI hot-swap <?>

*/

#ifndef ACTIONTEC_PCI_H
#define ACTIONTEC_PCI_H

#include <devices/common/pci/pcibridge.h>

class ActiontecBridge : public PCIBridge {
public:
    ActiontecBridge(std::string name);
    ~ActiontecBridge() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<ActiontecBridge>(new ActiontecBridge("ActiontecBridge"));
    }

    uint32_t pci_cfg_read(uint32_t reg_offs, AccessDetails &details);
    void pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details);
};

#endif // ACTIONTEC_PCI_H
