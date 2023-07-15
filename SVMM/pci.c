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


PCI PciDevices[BX_MAX_PCI_DEVICES];

NTSTATUS RegisterPortIoHandler(ULONG Address, WritePortIoHandlerCallback WriteHandler, ReadPortIoHandlerCallback ReadHandler)
{
	PULONG buffer;

	if (PortIoBitmap.SizeOfBitMap == 0) {
		buffer = calloc(0xffff / 32, sizeof(ULONG));
		RtlInitializeBitMap(&PortIoBitmap, buffer, 0xffff / 32);
	}
	if (Address >= 0xffff || RtlTestBit(&PortIoBitmap, Address)) {
		fprintf(stderr, "Error in RegisterPortHandler, Address already mapped!\n");
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
NTSTATUS RegisterPciHandler(ULONG DeviceFunction, WritePciConfigHandlerCallback WriteHandler, ReadPciConfigHandlerCallback ReadHandler)
{

	if (DeviceFunction >= BX_MAX_PCI_DEVICES) {
		fprintf(stderr, "Error in RegisterPciHandler, Address greater than available pci devices!\n");
		return -1;
	}
	
	if (PciDevices[DeviceFunction].initialized) {
		fprintf(stderr, "Error in RegisterPciHandler, pci device already initialized!\n");
		return -1;
	}
	PciDevices[DeviceFunction].initialized = 1;
	PciDevices[DeviceFunction].WritePciConfHandler = WriteHandler;
	PciDevices[DeviceFunction].ReadPciConfHandler = ReadHandler;

	return 0;
}


VOID WritePciConfHandler(ULONG Address, ULONG Value, ULONG Length)
{
	WritePortIoHandlerCallback PortIoWriteHandler;
	ReadPortIoHandlerCallback PortIoReadHandler;
	WriteMMIOHandlerCallback MMIOWriteHandler;
	ReadMMIOHandlerCallback MMIOReadHandler;
	BYTE devFunc, offset, barNum, value8, oldval, bar_change;
	ULONG i;

	devFunc = PCI_ADDRESS_TO_FUNCTION_DEVICE(Address);
	offset = PCI_ADDRESS_TO_OFFSET(Address);
	bar_change = 0;
	if (devFunc >= BX_MAX_PCI_DEVICES) {
		fprintf(stderr, "Error in WritePciConfHandler, Address greater than available pci devices!\n");
		exit(-1);
	}

	if (!PciDevices[devFunc].initialized) {
		fprintf(stderr, "Error in WritePciConfHandler, pci device not initialized!\n");
		exit(-1);
	}
	if (offset + Length < offset || offset + Length >= 0xff) {
		fprintf(stderr, "Error in WritePciConfHandler, invalid offset!\n");
		exit(-1);
	}
	//set bars
	if ((PciDevices[devFunc].conf[PCI_CONF_HDR_TYPE] & 3) == PCI_HDR_TYPE_DEVICE && offset >= PCI_CONF_BAR0 && offset <= PCI_CONF_BAR5) {
		barNum = (offset - 0x10) >> 2;
		if (PciDevices[devFunc].bar[barNum].type != BX_PCI_BAR_TYPE_NONE) {
			for (i = 0; i < Length; i++) {
				value8 = (Value >> (i * 8)) & 0xff;
				oldval = PciDevices[devFunc].conf[offset + i];
				if (((Address + i) & 0x03) == 0) {
					if (PciDevices[devFunc].bar[barNum].type == BX_PCI_BAR_TYPE_IO) {
						value8 = (value8 & 0xfc) | 0x01;
					}
					else {
						value8 = (value8 & 0xf0) | (oldval & 0x0f);
					}
				}
				bar_change |= (value8 != oldval);
				PciDevices[devFunc].conf[offset + i] = value8;
			}
			if (bar_change) {
				if (PciDevices[devFunc].bar[barNum].type == BX_PCI_BAR_TYPE_IO) {
					//GetPortIoHandler(PciDevices[devFunc].bar[barNum].addr, &PortIoWriteHandler, &PortIoReadHandler);
					for (i = 0; i < PciDevices[devFunc].bar[barNum].size; i++) {
						if (PciDevices[devFunc].bar[barNum].mask & (1 << i))
							continue;
						RemovePortIoHandler(PciDevices[devFunc].bar[barNum].addr + i);
						PortIoWriteHandler = PciDevices[devFunc].bar[barNum].WriteHandler;
						PortIoReadHandler = PciDevices[devFunc].bar[barNum].ReadHandler;
						RegisterPortIoHandler(Value + i, PortIoWriteHandler, PortIoReadHandler);
					}
				}
				else {
					//GetMMIOHandler(PciDevices[devFunc].bar[barNum].addr, &MMIOWriteHandler, &MMIOReadHandler);
					for (i = 0; i < PciDevices[devFunc].bar[barNum].size; i += 0x10000) {
						RemoveMMIOHandler(PciDevices[devFunc].bar[barNum].addr + i);
						MMIOWriteHandler = PciDevices[devFunc].bar[barNum].WriteHandler;
						MMIOReadHandler = PciDevices[devFunc].bar[barNum].ReadHandler;
						RegisterMMIOHandler(Value + i, MMIOWriteHandler, MMIOReadHandler);
					}
				}
				PciDevices[devFunc].bar[barNum].addr = Value;
				
			}
		}
	}
	//set rom address
	else if (offset == PCI_CONF_ROM_ADDRESS) {

	}
	//set irq line
	else if (offset == PCI_CONF_IRQ_LINE) {
		PciDevices[devFunc].conf[PCI_CONF_IRQ_LINE] = Value & 0xff;
	}
	//call device specific handler
	else {
		PciDevices[devFunc].WritePciConfHandler(&PciDevices[devFunc], offset, Value, Length);
	}
}

ULONG ReadPciConfHandler(ULONG Address, ULONG Length)
{
	UCHAR bus, devFunc, offset;
	ULONG i, Value;

	bus = PCI_ADDRESS_TO_BUS(Address);
	devFunc = PCI_ADDRESS_TO_FUNCTION_DEVICE(Address);
	offset = PCI_ADDRESS_TO_OFFSET(Address);

	if (devFunc >= BX_MAX_PCI_DEVICES) {
		fprintf(stderr, "Error in ReadPciConfHandler, Address greater than available pci devices!\n");
		exit(-1);
	}

	if (!PciDevices[devFunc].initialized) {
		fprintf(stderr, "Error in ReadPciConfHandler, pci device not initialized!\n");
		exit(-1);
	}
	if (offset + Length < offset || offset + Length >= 0xff) {
		fprintf(stderr, "Error in ReadPciConfHandler, invalid offset!\n");
		exit(-1);
	}
	Value = 0;
	for (i = 0; i < Length; i++) {
		Value |= (PciDevices[devFunc].conf[offset + i] << (i * 8));
	}

	return Value;
}

VOID InitPciConfig(ULONG Address, USHORT Vendor, USHORT Device, BYTE Revision,
	ULONG Class, BYTE Type, BYTE Interrupt)
{
	ULONG devFunc;

	devFunc = PCI_ADDRESS_TO_FUNCTION_DEVICE(Address);
	if (devFunc >= BX_MAX_PCI_DEVICES) {
		fprintf(stderr, "Error in InitPciConfig, Address greater than available pci devices!\n");
		exit(-1);
	}

	if (!PciDevices[devFunc].initialized) {
		fprintf(stderr, "Error in InitPciConfig, pci device not initialized!\n");
		exit(-1);
	}

	memset(PciDevices[devFunc].conf, '\0', sizeof(PciDevices[devFunc].conf));
	PciDevices[devFunc].conf[0x00] = (BYTE)(Vendor & 0xff);
	PciDevices[devFunc].conf[0x01] = (BYTE)(Vendor >> 8);
	PciDevices[devFunc].conf[0x02] = (BYTE)(Device & 0xff);
	PciDevices[devFunc].conf[0x03] = (BYTE)(Device >> 8);
	PciDevices[devFunc].conf[0x08] = Revision;
	PciDevices[devFunc].conf[0x09] = (BYTE)(Class & 0xff);
	PciDevices[devFunc].conf[0x0a] = (BYTE)((Class >> 8) & 0xff);
	PciDevices[devFunc].conf[0x0b] = (BYTE)((Class >> 16) & 0xff);
	PciDevices[devFunc].conf[0x0e] = Type;
	PciDevices[devFunc].conf[0x3d] = Interrupt;
}

void PciSetBarIo(BYTE DeviceNumber, BYTE BarNumber, USHORT Size, ReadPortIoHandlerCallback ReadPortHandler,
	WritePortIoHandlerCallback WritePortHandler, DWORD Mask)
{

	if (BarNumber < 6) {
		PciDevices[DeviceNumber].bar[BarNumber].type = BX_PCI_BAR_TYPE_IO;
		PciDevices[DeviceNumber].bar[BarNumber].size = Size;
		PciDevices[DeviceNumber].bar[BarNumber].ReadHandler = ReadPortHandler;
		PciDevices[DeviceNumber].bar[BarNumber].WriteHandler = WritePortHandler;
		PciDevices[DeviceNumber].bar[BarNumber].mask = Mask;
		PciDevices[DeviceNumber].conf[0x10 + BarNumber * 4] = 0x01;


	}
}

void PciSetBarMmio(BYTE DeviceNumber, BYTE BarNumber, DWORD Size, ReadMMIOHandlerCallback ReadMMIOHandler,
	WriteMMIOHandlerCallback WriteMMIOHandler)
{
	
	if (BarNumber < 6) {
		PciDevices[DeviceNumber].bar[BarNumber].type = BX_PCI_BAR_TYPE_MEM;
		PciDevices[DeviceNumber].bar[BarNumber].size = Size;
		PciDevices[DeviceNumber].bar[BarNumber].ReadHandler = ReadMMIOHandler;
		PciDevices[DeviceNumber].bar[BarNumber].WriteHandler = WriteMMIOHandler;
		PciDevices[DeviceNumber].conf[0x10 + BarNumber * 4] = 0x00;
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

