#ifndef PCI_H
#define PCI_H
#include <Windows.h>

#define BX_PCI_BAR_TYPE_NONE 0
#define BX_PCI_BAR_TYPE_MEM  1
#define BX_PCI_BAR_TYPE_IO   2

#define BX_MAX_PCI_DEVICES 20

#define BX_PCI_DEVICE(device, function)					((device) << 3 | (function))
#define PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, offset)	((devFunc << 8) | (offset))

#define PCI_DEVFUNC_TO_ADDRESS(device, function)		((device << 11) | (function << 8))
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

#define PCI_CLASS_STORAGE_IDE	0x0101
#define PCI_CLASS_DISPLAY_VGA	0x0300
#define PCI_CLASS_SYSTEM_PIC	0x0800

#define PCI_VENDOR_ID		0x00	/* 16 bits */
#define PCI_DEVICE_ID		0x02	/* 16 bits */
#define PCI_COMMAND		0x04	/* 16 bits */
#define  PCI_COMMAND_IO		0x1	/* Enable response in I/O space */
#define  PCI_COMMAND_MEMORY	0x2	/* Enable response in Memory space */
#define PCI_CLASS_DEVICE        0x0a    /* Device class */
#define PCI_HEADER_TYPE         0x0e    /* Header type */
#define PCI_INTERRUPT_LINE	0x3c	/* 8 bits */
#define PCI_INTERRUPT_PIN	0x3d	/* 8 bits */
#define PCI_MIN_GNT		0x3e	/* 8 bits */
#define PCI_MAX_LAT		0x3f	/* 8 bits */

#define PCI_BASE_ADDRESS_0	0x10	/* 32 bits */

#define PCI_ROM_ADDRESS		0x30	/* Bits 31..11 are address, 10..1 reserved */
#define  PCI_ROM_ADDRESS_ENABLE	0x01

#define PCI_VENDOR_ID_INTEL             0x8086
#define PCI_VENDOR_ID_TEST              0x1234

#define PCI_DEVICE_ID_INTEL_82437       0x0122
#define PCI_DEVICE_ID_INTEL_82441       0x1237
#define PCI_DEVICE_ID_INTEL_82443       0x7190
#define PCI_DEVICE_ID_INTEL_82443_1     0x7191
#define PCI_DEVICE_ID_INTEL_82443_NOAGP 0x7192
#define PCI_DEVICE_ID_INTEL_82371FB_0   0x122e
#define PCI_DEVICE_ID_INTEL_82371FB_1   0x1230
#define PCI_DEVICE_ID_INTEL_82371SB_0   0x7000
#define PCI_DEVICE_ID_INTEL_82371SB_1   0x7010
#define PCI_DEVICE_ID_INTEL_82371AB_0   0x7110
#define PCI_DEVICE_ID_INTEL_82371AB     0x7111
#define PCI_DEVICE_ID_INTEL_82371AB_3   0x7113

#define PCI_VENDOR_ID_IBM               0x1014
#define PCI_VENDOR_ID_APPLE             0x106b

typedef ULONG(*ReadPortIoHandlerCallback)(ULONG Address, ULONG Length);
typedef VOID(*WritePortIoHandlerCallback)(ULONG Address, ULONG Value, ULONG Length);

typedef ULONG(*ReadMMIOHandlerCallback)(ULONG Address, BYTE *Data, ULONG Length);
typedef VOID(*WriteMMIOHandlerCallback)(ULONG Address, BYTE *Data, ULONG Length);

NTSTATUS RegisterMMIOHandler(ULONG Address, WriteMMIOHandlerCallback WriteHandler, ReadMMIOHandlerCallback ReadHandler);
VOID ReadMMIOHandler(ULONG Address, BYTE* Data, ULONG Len);
VOID WriteMMIOHandler(ULONG Address, BYTE* Data, ULONG Len);
VOID GetMMIOHandler(ULONG Address, WriteMMIOHandlerCallback* WriteHandler, ReadMMIOHandlerCallback* ReadHandler);
NTSTATUS RemoveMMIOHandler(ULONG Address);


NTSTATUS RegisterPortIoHandler(ULONG Address, WritePortIoHandlerCallback WriteHandler, ReadPortIoHandlerCallback ReadHandler);
ULONG ReadPortIoHandler(ULONG Address, ULONG Len);
VOID WritePortIoHandler(ULONG Address, ULONG Value, ULONG Len);
VOID GetPortIoHandler(ULONG Address, WritePortIoHandlerCallback *WriteHandler, ReadPortIoHandlerCallback *ReadHandler);
NTSTATUS RemovePortIoHandler(ULONG Address);

typedef ULONG(*ReadPciConfigHandlerCallback)(VOID *Pci, ULONG Address, ULONG Length);
typedef VOID(*WritePciConfigHandlerCallback)(VOID *Pci, ULONG Address, ULONG Value, ULONG Length);

NTSTATUS RegisterPciHandler(ULONG Address, WritePciConfigHandlerCallback WriteHandler, ReadPciConfigHandlerCallback ReadHandler);
VOID WritePciConfHandler(ULONG Address, ULONG Value, ULONG Length);
ULONG ReadPciConfHandler(ULONG Address, ULONG Length);
VOID InitPciConfig(ULONG Address, USHORT vid, USHORT did, BYTE rev, ULONG classc, BYTE headt, BYTE intpin);
void PciSetBarIo(BYTE DeviceNumber, BYTE BarNumber, USHORT size, ReadPortIoHandlerCallback ReadPortHandler,
	WritePortIoHandlerCallback WritePortHandler, DWORD Mask);
void PciSetBarMmio(BYTE DeviceNumber, BYTE BarNumber, DWORD size, ReadMMIOHandlerCallback ReadMMIOHandler,
	WriteMMIOHandlerCallback WriteMMIOHandler);

struct pci_bar {
	BYTE type;
	DWORD size;
	DWORD addr;
	DWORD mask;
	VOID* WriteHandler;
	VOID* ReadHandler;
};

typedef struct _PCI {
	BYTE conf[256];
	struct pci_bar bar[6];
	BYTE initialized;
	WritePciConfigHandlerCallback WritePciConfHandler;
	ReadPciConfigHandlerCallback ReadPciConfHandler;
}PCI, *PPCI;

#endif