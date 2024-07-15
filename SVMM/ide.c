#include "ide.h"
#include "pci.h"
#include "timer.h"
#include "harddisk.h"
#include "SVMM.h"
#include <stdio.h>

struct ide Ide;

extern PCI PciDevices[MAX_BUSES][MAX_PCI_DEVICES_PER_BUS];

DWORD IdeBdmaPresent()
{
	return PciDevices[0][BX_PCI_DEVICE(1, 1)].bar[4].addr;
}

static ULONG IdePciConfReadHandler(ULONG64 Address, ULONG Length)
{
	return 0;
}

static VOID IdePciConfWriteHandler(PCI* Pci, ULONG64 Address, ULONG Value, ULONG Length)
{
	ULONG i;
	BYTE Value8, devFunc;

	devFunc = BX_PCI_DEVICE(1, 1);

	if (((Address >= 0x10) && (Address < 0x20)) ||
		((Address > 0x23) && (Address < 0x40)))
		return;

	for (i = 0; i < Length; i++) {
		Value8 = (Value >> (i * 8)) & 0xff;
		switch (Address + i) {
			case 0x05:
			case 0x06:
				break;
			case 0x04:
				Pci[devFunc].conf[Address + i] = Value8 & 0x5;
				break;
			default:
				Pci[devFunc].conf[Address + i] = Value8;
				break;
		}
	}

}

VOID IdePortIoWriteHandler(ULONG Address, ULONG Value, ULONG Length)
{
	BYTE offset, channel;
	ULONG devFunc;

	devFunc = BX_PCI_DEVICE(1, 1);
	

	offset = Address - PciDevices[0][devFunc].bar[4].addr;
	channel = (offset >> 3);
	offset &= 0x07;
	switch (offset) {
		case 0x00:
			if ((Value & IDE_BDMA_COMMAND_START) && !(Ide.bmdma[channel].Command & IDE_BDMA_COMMAND_START)) {
				Ide.bmdma[channel].Command = Value;
				Ide.bmdma[channel].Status |= IDE_STATUS_ACTIVE;
				Ide.bmdma[channel].PrdCurrent = Ide.bmdma[channel].PrdTable;
				Ide.bmdma[channel].BufferOffset = 0;
				//bx_pc_system.activate_timer(Ide.bmdma[channel].timer_index, 1000, 0);
			}
			else if (!(Value & IDE_BDMA_COMMAND_START) && (Ide.bmdma[channel].Command & IDE_BDMA_COMMAND_START)) {
				Ide.bmdma[channel].Command = Value;
				Ide.bmdma[channel].Status &= ~IDE_STATUS_ACTIVE;
			}
			break;
		case 0x02:
			Ide.bmdma[channel].Status = (Value & 0x60) | (Ide.bmdma[channel].Status & IDE_STATUS_ACTIVE) | (Ide.bmdma[channel].Status & (~Value & 0x06));
			break;
		case 0x04:
			Ide.bmdma[channel].PrdTable = Value & 0xfffffffc;
			break;
	}
}

ULONG IdePortIoReadHandler(ULONG Address, ULONG Length)
{
	BYTE offset, channel;
	WORD devFunc;
	DWORD value = 0xffffffff;

	devFunc = BX_PCI_DEVICE(1, 1);
	offset = Address - PciDevices[0][devFunc].bar[4].addr;
	channel = (offset >> 3);
	offset &= 0x07;
	switch (offset) {
		case 0x00:
			return Ide.bmdma[channel].Command;
		case 0x02:
			return Ide.bmdma[channel].Status;
		case 0x04:
			return Ide.bmdma[channel].PrdTable;
	}
	return value;
}

VOID IdeStartTransfer(BYTE Channel)
{
	if (Channel < 2)
		Ide.bmdma[Channel].DataReady = 1;
}

void IdeTimerHandler()
{
	BYTE channel;
	DWORD size, sector_size, i;
	PRD_ENTRY prdEntry;
	ULONG64 ret;

	ret = 0;

	for (channel = 0; channel < 2; channel++) {
		if (Ide.bmdma[channel].Status & IDE_STATUS_ACTIVE && Ide.bmdma[channel].PrdCurrent)
			break;
	}
	if (channel == 2)
		return;
	if (!Ide.bmdma[channel].DataReady && (Ide.bmdma[channel].Command & IDE_BDMA_COMMAND_WRITE))
		return;

	//TODO CHECK IF Address EXCEEDS ram size
	memcpy(&prdEntry.Address, GetHostPageFromGPA(Ide.bmdma[channel].PrdCurrent), 4);
	memcpy(&prdEntry.Count, GetHostPageFromGPA(Ide.bmdma[channel].PrdCurrent + 4), 4);

	size = prdEntry.Count & 0xfffe;
	if (size == 0) 
		size = 0x10000;
	
	/* TODO MAYBE instead of zero BufferOffset we should read/write the remaining bytes until IDE_BUFFER_SIZE and then wrap back to 0 */
	if (Ide.bmdma[channel].BufferOffset + size < size || Ide.bmdma[channel].BufferOffset + size > IDE_BUFFER_SIZE)
		Ide.bmdma[channel].BufferOffset = 0;

	if (!(Ide.bmdma[channel].Command & IDE_BDMA_COMMAND_WRITE)) 
		memcpy(Ide.bmdma[channel].Buffer + Ide.bmdma[channel].BufferOffset, GetHostPageFromGPA(prdEntry.Address), size);

	for (i = 0; i < size; i += 512) {
		if (i + 512 > size) {
			/* write/read remaining bytes */
			if (Ide.bmdma[channel].Command & IDE_BDMA_COMMAND_WRITE) {
				ret = HardDiskBmdmaReadSector(channel, Ide.bmdma[channel].Buffer + Ide.bmdma[channel].BufferOffset + i, size - i);
			}
			else {
				ret = HardDiskBmdmaWriteSector(channel, Ide.bmdma[channel].Buffer + Ide.bmdma[channel].BufferOffset + i, size - i);
			}
		}
		else {
			/* write/read a sector to/from disk */
			if (Ide.bmdma[channel].Command & IDE_BDMA_COMMAND_WRITE) {
				ret = HardDiskBmdmaReadSector(channel, Ide.bmdma[channel].Buffer + Ide.bmdma[channel].BufferOffset + i, 512);
			}
			else {
				ret = HardDiskBmdmaWriteSector(channel, Ide.bmdma[channel].Buffer + Ide.bmdma[channel].BufferOffset + i, 512);
			}
		}
		if (ret) {
			/* error, should we mark channel as stopped? */
			HardDiskBdmaComplete(channel);
			return;
		}
	}
	
	if (Ide.bmdma[channel].Command & IDE_BDMA_COMMAND_WRITE)
		memcpy(GetHostPageFromGPA(prdEntry.Address), Ide.bmdma[channel].Buffer + Ide.bmdma[channel].BufferOffset, size);


	/* if EOT is set, we are done */
	if (prdEntry.Count & 0x80000000) {
		Ide.bmdma[channel].Status &= ~IDE_STATUS_ACTIVE;
		Ide.bmdma[channel].Status |= IDE_STATUS_START;
		Ide.bmdma[channel].PrdCurrent = 0;
		Ide.bmdma[channel].BufferOffset = 0;
		HardDiskBdmaComplete(channel);
	}
	else {
		/* Fetch next table */
		Ide.bmdma[channel].BufferOffset += size;
		Ide.bmdma[channel].PrdCurrent += 8;
	}
}

VOID IdeSetIrq(BYTE Channel)
{
	if (Channel < 2)
		Ide.bmdma[Channel].Status |= 0x4;
}


VOID IdeInitialize()
{
	ULONG PciAddress;
	USHORT devFunc;
	BYTE i;

	memset(&Ide, '\0', sizeof(Ide));

	devFunc = BX_PCI_DEVICE(1, 1);
	PciAddress = PCI_DEVFUNC_OFFSET_TO_ADDRESS(0, devFunc, 0);
	PciRegisterConfigHandler(PciAddress, IdePciConfWriteHandler, IdePciConfReadHandler);
	PciInitConfig(PciAddress, PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371SB_1, 0x00, PCI_CLASS_STORAGE_IDE, 0x80, 0x00, 0);
	PciSetBarIo(PciAddress, 4, 0, 16, IdePortIoReadHandler, IdePortIoWriteHandler, IDE_PORT_MASK);
	
	Ide.bmdma[0].Buffer = calloc(sizeof(BYTE), IDE_BUFFER_SIZE);
	Ide.bmdma[1].Buffer = calloc(sizeof(BYTE), IDE_BUFFER_SIZE);

	TimerRegister(TICK_PERIOD, IdeTimerHandler, NULL);
}
