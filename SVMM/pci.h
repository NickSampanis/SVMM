#ifndef PCI_H
#define PCI_H
#include <Windows.h>

#define BX_PCI_BAR_TYPE_NONE 0
#define BX_PCI_BAR_TYPE_MEM  1
#define BX_PCI_BAR_TYPE_IO   2

#define BX_MAX_PCI_DEVICES 20

#define PCI_DEVFUNC_TO_ADDRESS(device, function)		((device << 11) | (function << 8))
#define PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, offset)	((devFunc << 8) | (offset))
#define PCI_ADDRESS_TO_BUS(address)						((address >> 16) & 0xff)
#define PCI_ADDRESS_TO_FUNCTION(address)				((address >> 8) & 7)
#define PCI_ADDRESS_TO_DEVICE(address)					((address >> 11) & 0xff)
#define PCI_ADDRESS_TO_FUNCTION_DEVICE(address)			((address >> 8) & 0xff)
#define PCI_ADDRESS_TO_OFFSET(address)					((address) & 0xff)

#define PCI_CONF_HDR_TYPE		0x0E
#define PCI_CONF_BAR0			0x10
#define PCI_CONF_BAR5			0x24
#define PCI_CONF_ROM_ADDRESS	0x30
#define PCI_CONF_IRQ_LINE		0x3C

#define PCI_SLOTS				2

#define PCI_HDR_TYPE_DEVICE 0x00

typedef ULONG(*ReadPortIoHandlerCallback)(ULONG Address, ULONG Length);
typedef VOID(*WritePortIoHandlerCallback)(ULONG Address, ULONG Value, ULONG Length);

typedef ULONG(*ReadMMIOHandlerCallback)(ULONG Address, BYTE *Data, ULONG Length);
typedef VOID(*WriteMMIOHandlerCallback)(ULONG Address, BYTE *Data, ULONG Length);

NTSTATUS RegisterMMIOHandler(ULONG Address, WriteMMIOHandlerCallback WriteHandler, ReadMMIOHandlerCallback ReadHandler);
VOID ReadMMIOHandler(ULONG Address, BYTE* Data, ULONG Len);
VOID WriteMMIOHandler(ULONG Address, BYTE* Data, ULONG Len);


NTSTATUS RegisterPortIoHandler(ULONG Address, WritePortIoHandlerCallback WriteHandler, ReadPortIoHandlerCallback ReadHandler);
ULONG ReadPortIoHandler(ULONG Address, ULONG Len);
VOID WritePortIoHandler(ULONG Address, ULONG Value, ULONG Len);

typedef ULONG(*ReadPciConfigHandlerCallback)(VOID *Pci, ULONG Address, ULONG Length);
typedef VOID(*WritePciConfigHandlerCallback)(VOID *Pci, ULONG Address, ULONG Value, ULONG Length);


NTSTATUS RegisterPciHandler(ULONG Address, WritePciConfigHandlerCallback WriteHandler, ReadPciConfigHandlerCallback ReadHandler);
VOID WritePciConfHandler(ULONG Address, ULONG Value, ULONG Length);
ULONG ReadPciConfHandler(ULONG Address, ULONG Length);
VOID InitPciConfig(ULONG Address, USHORT vid, USHORT did, BYTE rev, ULONG classc, BYTE headt, BYTE intpin);

struct pci_bar {
	BYTE type;
	DWORD size;
	DWORD addr;
};

typedef struct _PCI {
	BYTE conf[256];
	struct pci_bar bar[6];
	BYTE initialized;
	WritePciConfigHandlerCallback WritePciConfHandler;
	ReadPciConfigHandlerCallback ReadPciConfHandler;
}PCI, *PPCI;

#endif