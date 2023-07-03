#ifndef PIIX3_H
#define PIIX3_H

#include <Windows.h>
#include "pci.h"

struct piix3 {
	BYTE APMControl;
	BYTE APMStatus;
	BYTE edgeLevelTrigger1;
	BYTE edgeLevelTrigger2;
	BYTE pciReset;
	BYTE irqRegister[16];
	DWORD irqStatus[4][16];
};

VOID Piix3RegisterIrq(PCI* Pci, ULONG Address, BYTE irq);
VOID Piix3UnregisterIrq(PCI* Pci, ULONG Address, BYTE irq);

#endif
