#include "pci.h"
#include "bitmap.h"
#include "SVMM.h"
#include "haxm.h"
#include "gvm.h"
#include <stdio.h>

//PORT IO
static RTL_BITMAP  PortIoBitmap;
static WritePortIoHandlerCallback WritePortHandlers[0xffff];
static ReadPortIoHandlerCallback ReadPortHandlers[0xffff];

//MMIO 
static RTL_BITMAP  MMIOBitmap;
static WriteMMIOHandlerCallback WriteMMIOHandlers[0xffff];
static ReadMMIOHandlerCallback ReadMMIOHandlers[0xffff];


PCI PciDevices[MAX_BUSES][MAX_PCI_DEVICES_PER_BUS];



NTSTATUS RegisterPortIoHandler(ULONG Address, WritePortIoHandlerCallback WriteHandler, ReadPortIoHandlerCallback ReadHandler)
{
	PULONG buffer;

	if (PortIoBitmap.SizeOfBitMap == 0) {
		buffer = calloc(0xffff / 32, sizeof(ULONG));
		RtlInitializeBitMap(&PortIoBitmap, buffer, 0xffff / 32);
	}
	if (Address >= 0xffff || RtlTestBit(&PortIoBitmap, Address)) {
		fprintf(stderr, "Error in RegisterPortIoHandler, Address 0x%lx already mapped!\n", Address);
		exit(-1);
	}
	RtlSetBit(&PortIoBitmap, Address);
	WritePortHandlers[Address] = WriteHandler;
	ReadPortHandlers[Address] = ReadHandler;

	return 0;
}

NTSTATUS RemovePortIoHandler(ULONG Address)
{
	if (Address >= 0xffff || !RtlTestBit(&PortIoBitmap, Address)) {
		fprintf(stderr, "Error in RegisterPortHandler, Address 0x%lx not mapped!\n", Address);
		exit(-1);
	}
	RtlClearBit(&PortIoBitmap, Address);
	WritePortHandlers[Address] = NULL;
	ReadPortHandlers[Address] = NULL;

	return 0;
}

ULONG ReadPortIoHandler(ULONG Address, ULONG Length)
{
	if (Address >= 0xffff || !RtlTestBit(&PortIoBitmap, Address) || !ReadPortHandlers[Address]) {
		fprintf(stderr, "Error in ReadPortIoHandler, Address 0x%lx is not mapped!\n", Address);
		exit(-1);
	}
	return ReadPortHandlers[Address](Address, Length);
}

VOID WritePortIoHandler(ULONG Address, ULONG Value, ULONG Length)
{
	if (Address >= 0xffff || !RtlTestBit(&PortIoBitmap, Address) || !WritePortHandlers[Address]) {
		fprintf(stderr, "Error in WritePortHandler, Address 0x%lx is not mapped!\n", Address);
		struct Registers regs;
		SvmmGetRegisters(&regs);
		SvmmPrintRegisters(&regs);
		exit(-1);
	}
	WritePortHandlers[Address](Address, Value, Length);
}

VOID GetPortIoHandler(ULONG Address, WritePortIoHandlerCallback* WriteHandler, ReadPortIoHandlerCallback* ReadHandler)
{
	if (Address >= 0xffff || !RtlTestBit(&PortIoBitmap, Address) || !ReadPortHandlers[Address] || !WritePortHandlers[Address]) {
		fprintf(stderr, "Error in ReadPortIoHandler, Address 0x%lx is not mapped!\n", Address);
		exit(-1);
	}
	*WriteHandler = WritePortHandlers[Address];
	*ReadHandler = ReadPortHandlers[Address];
}

//PCI 
VOID PciInitialize(VOID)
{
	LONG i, j;

	memset(PciDevices, '\0', sizeof(PciDevices));

	//config space should be 0xff
	for (i = 0; i < MAX_BUSES; i++) {
		for (j = 0; j < MAX_PCI_DEVICES_PER_BUS; j++)
			memset(PciDevices[i][j].conf, 0xff, sizeof(PciDevices[i][j].conf));
	}
}

NTSTATUS RegisterPciHandler(ULONG Address, WritePciConfigHandlerCallback WriteHandler, ReadPciConfigHandlerCallback ReadHandler)
{
	ULONG devFunc, bus;

	bus = PCI_ADDRESS_TO_BUS(Address);
	devFunc = PCI_ADDRESS_TO_FUNCTION_DEVICE(Address);

	if (devFunc >= MAX_PCI_DEVICES_PER_BUS || bus >= MAX_BUSES) {
		fprintf(stderr, "Error in RegisterPciHandler, Address greater than available pci devices!\n");
		return -1;
	}
	
	if (PciDevices[bus][devFunc].initialized) {
		fprintf(stderr, "Error in RegisterPciHandler, pci device already initialized!\n");
		return -1;
	}
	PciDevices[bus][devFunc].initialized = 1;
	PciDevices[bus][devFunc].WritePciConfHandler = WriteHandler;
	PciDevices[bus][devFunc].ReadPciConfHandler = ReadHandler;

	return 0;
}


VOID WritePciConfHandler(ULONG Address, ULONG Value, ULONG Length)
{
	WritePortIoHandlerCallback PortIoWriteHandler;
	ReadPortIoHandlerCallback PortIoReadHandler;
	WriteMMIOHandlerCallback MMIOWriteHandler;
	ReadMMIOHandlerCallback MMIOReadHandler;
	BYTE devFunc, offset, barNum, value8, oldval, bar_change;
	ULONG i, bus;

	
	devFunc = PCI_ADDRESS_TO_FUNCTION_DEVICE(Address);
	offset = PCI_ADDRESS_TO_OFFSET(Address);
	bus = PCI_ADDRESS_TO_BUS(Address);
	bar_change = 0;
	if (devFunc >= MAX_PCI_DEVICES_PER_BUS || bus >= MAX_BUSES) {
		fprintf(stderr, "Error in WritePciConfHandler, Address greater than available pci devices!\n");
		exit(-1);
	}

	if (!PciDevices[bus][devFunc].initialized) {
		fprintf(stderr, "Error in WritePciConfHandler, pci device not initialized!\n");
		return;
		//exit(-1);
	}
	if (offset == 0xff || offset + Length < offset || offset + Length >= 0xff) {
		fprintf(stderr, "Error in WritePciConfHandler, invalid offset!\n");
		return;
		//exit(-1);
	}
	//set bars
	if ((PciDevices[bus][devFunc].conf[PCI_CONF_HDR_TYPE] & 3) == PCI_HDR_TYPE_DEVICE && offset >= PCI_CONF_BAR0 && offset <= PCI_CONF_BAR5) {
		barNum = (offset - 0x10) >> 2;
		if (PciDevices[bus][devFunc].bar[barNum].type != BX_PCI_BAR_TYPE_NONE) {
			for (i = 0; i < Length; i++) {
				value8 = (Value >> (i * 8)) & 0xff;
				oldval = PciDevices[bus][devFunc].conf[offset + i];
				if (((Address + i) & 0x03) == 0) {
					if (PciDevices[bus][devFunc].bar[barNum].type == BX_PCI_BAR_TYPE_IO) {
						value8 = (value8 & 0xfc) | 0x01;
					}
					else {
						value8 = (value8 & 0xf0) | (oldval & 0x0f);
					}
				}
				bar_change |= (value8 != oldval);
				PciDevices[bus][devFunc].conf[offset + i] = value8;
			}
			if (bar_change) {
				
				if (PciDevices[bus][devFunc].bar[barNum].type == BX_PCI_BAR_TYPE_IO) {
					if (Value == 0xffffffff) {
						DWORD mask;
						mask = ~(PciDevices[bus][devFunc].bar[barNum].size - 1);
						
						PciDevices[bus][devFunc].conf[offset] = (mask & 0xfc) | BX_PCI_BAR_TYPE_IO;
						PciDevices[bus][devFunc].conf[offset + 1] = (mask >> 8);
						PciDevices[bus][devFunc].conf[offset + 2] = (mask >> 16);
						PciDevices[bus][devFunc].conf[offset + 3] = (mask >> 24);
						
						/*
						PciDevices[bus][devFunc].conf[offset + 3] &= (mask & 0xfc);
						PciDevices[bus][devFunc].conf[offset + 2] &= (mask >> 8);
						PciDevices[bus][devFunc].conf[offset + 1] &= (mask >> 16);
						PciDevices[bus][devFunc].conf[offset] &= (mask >> 24);
						*/
						return;
					}
					for (i = 0; i < PciDevices[bus][devFunc].bar[barNum].size; i++) {
						if (PciDevices[bus][devFunc].bar[barNum].mask & (1ULL << i))
							continue;
						if (PciDevices[bus][devFunc].bar[barNum].addr)
							RemovePortIoHandler(PciDevices[bus][devFunc].bar[barNum].addr + i);
						PortIoWriteHandler = PciDevices[bus][devFunc].bar[barNum].WriteHandler;
						PortIoReadHandler = PciDevices[bus][devFunc].bar[barNum].ReadHandler;
						RegisterPortIoHandler(Value + i, PortIoWriteHandler, PortIoReadHandler);
					}
				}
				else {
					if (Value == 0xffffffff) {
						DWORD mask;
						mask = ~(PciDevices[bus][devFunc].bar[barNum].size - 1);

						PciDevices[bus][devFunc].conf[offset + 3] &= (mask & 0xfc);
						PciDevices[bus][devFunc].conf[offset + 2] &= (mask >> 8);
						PciDevices[bus][devFunc].conf[offset + 1] &= (mask >> 16);
						PciDevices[bus][devFunc].conf[offset] &= (mask >> 24);
						return;
					}

					for (i = 0; i < PciDevices[bus][devFunc].bar[barNum].size; i += 0x10000) {
						if (PciDevices[bus][devFunc].bar[barNum].addr)
							RemoveMMIOHandler(PciDevices[bus][devFunc].bar[barNum].addr + i);
						MMIOWriteHandler = PciDevices[bus][devFunc].bar[barNum].WriteHandler;
						MMIOReadHandler = PciDevices[bus][devFunc].bar[barNum].ReadHandler;
						RegisterMMIOHandler(Value + i, MMIOWriteHandler, MMIOReadHandler);
					}
				}
				PciDevices[bus][devFunc].bar[barNum].addr = Value;
				
			}
		}
	}
	//set rom address
	else if (offset == PCI_CONF_ROM_ADDRESS) {

	}
	//set irq line
	else if (offset == PCI_CONF_IRQ_LINE) {
		PciDevices[bus][devFunc].conf[PCI_CONF_IRQ_LINE] = Value & 0xff;
		PciDevices[bus][devFunc].conf[PCI_CONF_IRQ_PIN] = 1;
	}
	//call device specific handler
	else {
		PciDevices[bus][devFunc].WritePciConfHandler(&PciDevices[bus][devFunc], offset, Value, Length);
	}
}

ULONG ReadPciConfHandler(ULONG Address, ULONG Length)
{
	UCHAR bus, devFunc, offset, barNum;
	ULONG i, Value;

	bus = PCI_ADDRESS_TO_BUS(Address);
	devFunc = PCI_ADDRESS_TO_FUNCTION_DEVICE(Address);
	offset = PCI_ADDRESS_TO_OFFSET(Address);

	if (devFunc >= MAX_PCI_DEVICES_PER_BUS || bus >= MAX_BUSES) {
		fprintf(stderr, "Error in ReadPciConfHandler, Address greater than available pci devices!\n");
		exit(-1);
	}
	/*
	if (!PciDevices[bus][devFunc].initialized) {
		fprintf(stderr, "Error in ReadPciConfHandler, pci device not initialized!\n");
		exit(-1);
	}
	*/
	if (offset == 0xff || offset + Length < offset || offset + Length >= 0xff) {
		fprintf(stderr, "Error in ReadPciConfHandler, invalid offset!\n");
		exit(-1);
	}
	if ((PciDevices[bus][devFunc].conf[PCI_CONF_HDR_TYPE] & 3) == PCI_HDR_TYPE_DEVICE && offset >= PCI_CONF_BAR0 && offset <= PCI_CONF_BAR5) {
		barNum = (offset - 0x10) >> 2;
		if (PciDevices[bus][devFunc].bar[barNum].type == BX_PCI_BAR_TYPE_NONE)
			return 0;
		
	}
	if (offset == PCI_CONF_ROM_ADDRESS)
		return 0;

	Value = 0;
	for (i = 0; i < Length; i++) {
		Value |= (PciDevices[bus][devFunc].conf[offset + i] << (i * 8));
	}

	return Value;
}

VOID InitPciConfig(ULONG Address, USHORT Vendor, USHORT Device, BYTE Revision,
	ULONG Class, BYTE Type, BYTE Interrupt)
{
	ULONG devFunc, bus;

	bus = PCI_ADDRESS_TO_BUS(Address);
	devFunc = PCI_ADDRESS_TO_FUNCTION_DEVICE(Address);
	if (devFunc >= MAX_PCI_DEVICES_PER_BUS || bus >= MAX_BUSES) {
		fprintf(stderr, "Error in InitPciConfig, Address greater than available pci devices!\n");
		exit(-1);
	}

	if (!PciDevices[bus][devFunc].initialized) {
		fprintf(stderr, "Error in InitPciConfig, pci device not initialized!\n");
		exit(-1);
	}

	//memset(PciDevices[bus][devFunc].conf, 0xff, sizeof(PciDevices[bus][devFunc].conf));
	PciDevices[bus][devFunc].conf[0x00] = (BYTE)(Vendor & 0xff);
	PciDevices[bus][devFunc].conf[0x01] = (BYTE)(Vendor >> 8);
	PciDevices[bus][devFunc].conf[0x02] = (BYTE)(Device & 0xff);
	PciDevices[bus][devFunc].conf[0x03] = (BYTE)(Device >> 8);
	PciDevices[bus][devFunc].conf[0x08] = Revision;
	PciDevices[bus][devFunc].conf[0x09] = (BYTE)(Class & 0xff);
	PciDevices[bus][devFunc].conf[0x0a] = (BYTE)((Class >> 8) & 0xff);
	PciDevices[bus][devFunc].conf[0x0b] = (BYTE)((Class >> 16) & 0xff);
	PciDevices[bus][devFunc].conf[0x0e] = Type;
	PciDevices[bus][devFunc].conf[PCI_CONF_IRQ_LINE] = Interrupt;
	PciDevices[bus][devFunc].conf[PCI_CONF_IRQ_PIN] = 1;

}

void PciSetBarIo(ULONG Address, BYTE BarNumber, USHORT Size, ReadPortIoHandlerCallback ReadPortHandler,
	WritePortIoHandlerCallback WritePortHandler, ULONG64 Mask)
{
	ULONG devFunc, bus;

	bus = PCI_ADDRESS_TO_BUS(Address);
	devFunc = PCI_ADDRESS_TO_FUNCTION_DEVICE(Address);
	if (devFunc >= MAX_PCI_DEVICES_PER_BUS || bus >= MAX_BUSES) {
		fprintf(stderr, "Error in ReadPciConfHandler, Address greater than available pci devices!\n");
		exit(-1);
	}
	if (BarNumber < 6) {
		PciDevices[bus][devFunc].bar[BarNumber].type = BX_PCI_BAR_TYPE_IO;
		PciDevices[bus][devFunc].bar[BarNumber].size = Size;
		PciDevices[bus][devFunc].bar[BarNumber].ReadHandler = ReadPortHandler;
		PciDevices[bus][devFunc].bar[BarNumber].WriteHandler = WritePortHandler;
		PciDevices[bus][devFunc].bar[BarNumber].mask = Mask;
		PciDevices[bus][devFunc].bar[BarNumber].addr = 0;
		PciDevices[bus][devFunc].conf[0x10 + BarNumber * 4] = 0x01;
	}
}

void PciSetBarMmio(ULONG Address, BYTE BarNumber, DWORD Size, ReadMMIOHandlerCallback ReadMMIOHandler,
	WriteMMIOHandlerCallback WriteMMIOHandler)
{
	ULONG devFunc, bus;

	bus = PCI_ADDRESS_TO_BUS(Address);
	devFunc = PCI_ADDRESS_TO_FUNCTION_DEVICE(Address);

	if (devFunc >= MAX_PCI_DEVICES_PER_BUS || bus >= MAX_BUSES) {
		fprintf(stderr, "Error in ReadPciConfHandler, Address greater than available pci devices!\n");
		exit(-1);
	}
	if (BarNumber < 6) {
		PciDevices[bus][devFunc].bar[BarNumber].type = BX_PCI_BAR_TYPE_MEM;
		PciDevices[bus][devFunc].bar[BarNumber].size = Size;
		PciDevices[bus][devFunc].bar[BarNumber].ReadHandler = ReadMMIOHandler;
		PciDevices[bus][devFunc].bar[BarNumber].WriteHandler = WriteMMIOHandler;
		PciDevices[bus][devFunc].bar[BarNumber].addr = 0;
		PciDevices[bus][devFunc].conf[0x10 + BarNumber * 4] = 0;
	}
}


//Register MMIO, each entry should 0x10000 in size
NTSTATUS RegisterMMIOHandler(ULONG Address, WriteMMIOHandlerCallback WriteHandler, ReadMMIOHandlerCallback ReadHandler)
{
	PULONG buffer;
	ULONG index;

	index = Address >> 16;
	
	if (MMIOBitmap.SizeOfBitMap == 0) {
		buffer = calloc(0xffff / 32, sizeof(ULONG));
		RtlInitializeBitMap(&MMIOBitmap, buffer, 0xffff / 32);
	}
	if (index >= 0xffff || RtlTestBit(&MMIOBitmap, index)) {
		fprintf(stderr, "Error in RegisterMMIOHandler, Address already mapped!\n");
		exit(-1);
	}
	RtlSetBit(&MMIOBitmap, index);
	WriteMMIOHandlers[index] = WriteHandler;
	ReadMMIOHandlers[index] = ReadHandler;
	gvm_register_mmio(Address, 0x10000);

	return 0;
}

VOID GetMMIOHandler(ULONG Address, WriteMMIOHandlerCallback* WriteHandler, ReadMMIOHandlerCallback* ReadHandler)
{
	ULONG index;

	index = Address >> 16;
	if (index >= 0xffff || !RtlTestBit(&MMIOBitmap, index) || !ReadMMIOHandlers[index] || !WriteMMIOHandlers[index]) {
		fprintf(stderr, "Error in ReadMMIOHandler, Address 0x%lx is not mapped!\n", Address);
		exit(-1);
	}
	*WriteHandler = WriteMMIOHandlers[index];
	*ReadHandler = ReadMMIOHandlers[index];
}

NTSTATUS RemoveMMIOHandler(ULONG Address)
{
	ULONG index;

	index = Address >> 16;

	if (index >= 0xffff || !RtlTestBit(&MMIOBitmap, index)) {
		fprintf(stderr, "Error in RegisterMMIOHandler, Address 0x%lx not mapped!\n", Address);
		exit(-1);
	}
	RtlClearBit(&MMIOBitmap, index);
	WriteMMIOHandlers[index] = NULL;
	ReadMMIOHandlers[index] = NULL;
	gvm_remove_mmio(Address, 0x10000);

	return 0;
}

VOID ReadMMIOHandler(ULONG Address, BYTE *Data, ULONG Length)
{
	ULONG index;

	index = Address >> 16;

	if (index >= 0xffff || !RtlTestBit(&MMIOBitmap, index) || !ReadMMIOHandlers[index]) {
		fprintf(stderr, "Error in ReadMMIOHandler, Address 0x%lx is not mapped!\n", Address);
		exit(-1);
	}
	ReadMMIOHandlers[index](Address, Data, Length);
}

VOID WriteMMIOHandler(ULONG Address, BYTE *Data, ULONG Length)
{
	ULONG index;

	index = Address >> 16;

	if (index >= 0xffff || !RtlTestBit(&MMIOBitmap, index) || !WriteMMIOHandlers[index]) {
		fprintf(stderr, "Error in WriteMMIOHandler, Address 0x%lx is not mapped!\n", Address);
		exit(-1);
	}
	WriteMMIOHandlers[index](Address, Data, Length);
}

VOID PciSetIrq(ULONG Address, BYTE line, BYTE level)
{

}