#include "pci.h"
#include "bitmap.h"
#include "SVMM.h"
#include "haxm.h"
#include <stdio.h>

//PORT IO
static RTL_BITMAP  PortIoBitmap;
static WritePortIoHandlerCallback WritePortHandlers[0xffff];
static ReadPortIoHandlerCallback ReadPortHandlers[0xffff];

//MMIO 
static RTL_BITMAP  MMIOBitmap;
static WriteMMIOHandlerCallback WriteMMIOHandlers[0xffff];
static ReadMMIOHandlerCallback ReadMMIOHandlers[0xffff];


static PCI PciDevices[BX_MAX_PCI_DEVICES];

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

NTSTATUS RemovePortIoHandler(ULONG Address, WritePortIoHandlerCallback WriteHandler, ReadPortIoHandlerCallback ReadHandler)
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


//PCI 
NTSTATUS RegisterPciHandler(ULONG Address, WritePciConfigHandlerCallback WriteHandler, ReadPciConfigHandlerCallback ReadHandler)
{
	UCHAR devFunc, offset;

	devFunc = PCI_ADDRESS_TO_FUNCTION_DEVICE(Address);
	offset = PCI_ADDRESS_TO_OFFSET(Address);

	if (devFunc >= BX_MAX_PCI_DEVICES) {
		fprintf(stderr, "Error in RegisterPciHandler, Address greater than available pci devices!\n");
		return -1;
	}
	
	if (PciDevices[devFunc].initialized) {
		fprintf(stderr, "Error in RegisterPciHandler, pci device already initialized!\n");
		return -1;
	}
	PciDevices[devFunc].initialized = 1;
	PciDevices[devFunc].WritePciConfHandler = WriteHandler;
	PciDevices[devFunc].ReadPciConfHandler = ReadHandler;

	return 0;
}


VOID WritePciConfHandler(ULONG Address, ULONG Value, ULONG Length)
{
	UCHAR bus, devFunc, offset;

	bus = PCI_ADDRESS_TO_BUS(Address);
	devFunc = PCI_ADDRESS_TO_FUNCTION_DEVICE(Address);
	offset = PCI_ADDRESS_TO_OFFSET(Address);
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

	}
	//set rom address
	else if (offset == PCI_CONF_ROM_ADDRESS) {

	}
	//set irq line
	else if (offset == PCI_CONF_IRQ_LINE) {

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

//MMIO

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

	return 0;
}

NTSTATUS RemoveMMIOHandler(ULONG Address, WriteMMIOHandlerCallback WriteHandler, ReadMMIOHandlerCallback ReadHandler)
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




