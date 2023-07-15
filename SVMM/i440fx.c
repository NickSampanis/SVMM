#include "i440fx.h"
#include "pci.h"

static struct i440fx i440fx;

static ULONG I440fxPciConfReadHandler(ULONG64 Address, ULONG Length)
{
	return 0;
}

static VOID I440fxPciConfWriteHandler(PCI *Pci, ULONG64 Address, ULONG Value, ULONG Length)
{
	ULONG i;
	BYTE byte, area;

	if ((Address >= 0x10) && (Address < 0x34))
		return;

	for (i = 0; i < Length; i++) {
		byte = (Value >> i) & 0xff;
		switch (Address + i) {
			case 0x04:
				Pci->conf[Address + i] = (byte & 0x40) | 0x06;
				break;
			case 0x07:
				Pci->conf[Address + i] &= ~(byte & 0xf9);
				break;
			case 0x0d:
				Pci->conf[Address + i] = (byte & 0xf8);
				break;
			case 0x06:
			case 0x0c:
			case 0x0f:
				break;
			case 0x50:
				Pci->conf[Address + i] = (byte & 0x70);
				break;
			case 0x51:
				Pci->conf[Address + i] = (byte & 0x8f) | 0x20;
				break;
			case 0x59:
			case 0x5A:
			case 0x5B:
			case 0x5C:
			case 0x5D:
			case 0x5E:
			case 0x5F:
				if (Pci->conf[Address + i] != byte) {
					Pci->conf[Address + i] = byte;
					if (Address + i == 0x59) {
						i440fx.memory_type[BX_MEM_AREA_F0000][0] = (Value >> 4) & 1;
						i440fx.memory_type[BX_MEM_AREA_F0000][1] = (Value >> 5) & 1;
					}
					else {
						area = ((Address + i) - 0x5A) << 1;
						i440fx.memory_type[area][0] = (Value >> 0) & 1;
						i440fx.memory_type[area][1] = (Value >> 1) & 1;
						area++;
						i440fx.memory_type[area][0] = (Value >> 4) & 1;
						i440fx.memory_type[area][1] = (Value >> 5) & 1;
					}
				}
				break;
			case 0x60:
			case 0x61:
			case 0x62:
			case 0x63:
			case 0x64:
			case 0x65:
			case 0x66:
			case 0x67:
				Pci->conf[Address + i] = byte;
				if (Pci->conf[Address] != i440fx.drb[(Address + i) & 7])
					i440fx.dramDetect |= (1 << ((Address + i) & 7));
				else
					i440fx.dramDetect &= ~(1 << ((Address + i) & 7));
				break;
			case 0x72:
				//smram_control(value8); // SMRAM control register
				break;
			case 0x7a:
				Pci->conf[Address + i] &= 0x0a;
				Pci->conf[Address + i] |= (byte & 0xf5);
				break;
			default:
				Pci->conf[Address + i] = byte;
			}
	}
}

VOID I440fxInitialize(VOID* Ram, ULONG64 RamSize)
{
	ULONG i, j, mc, devFunc;
	BYTE drbval;
	BYTE type[3] = { 128, 32, 8 };

	memset(&i440fx, '\0', sizeof(i440fx));
	devFunc = BX_PCI_DEVICE(0, 0);
	RegisterPciHandler(devFunc, I440fxPciConfWriteHandler, I440fxPciConfReadHandler);
	InitPciConfig(devFunc, PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82441, 0x00, 0x060000, 0x00, 0);

	RegisterPortIoHandler(0x92, (WritePortIoHandlerCallback)I440fxPortIoWriteHandler, (ReadPortIoHandlerCallback)I440fxPortIoReadHandler);
	RegisterPortIoHandler(0xcf8, (WritePortIoHandlerCallback)I440fxPortIoWriteHandler, (ReadPortIoHandlerCallback)I440fxPortIoReadHandler);
	for (i = 0xcfc; i <= 0xcff; i++)
		RegisterPortIoHandler(i, (WritePortIoHandlerCallback)I440fxPortIoWriteHandler, (ReadPortIoHandlerCallback)I440fxPortIoReadHandler);


	memset(i440fx.drb, '\0', sizeof(i440fx.drb));
	if (RamSize & 0x07)
		RamSize = (RamSize & ~0x07) + 8;

	if (RamSize > 1024)
		RamSize = 1024;

	drbval = 0;
	for (i = 0; i < 3; i++) {
		mc = RamSize / type[i];
		for (j = 0; j < mc; j++) {
			drbval += (type[i] >> 3);
			i440fx.drb[i] = drbval;
		}
	}
	for (i = 0; i < 8; i++) {
		i440fx.drb[i] = drbval;
	}
	i440fx.dramDetect = 0;
	for (i = 0; i < 8; i++)
		WritePciConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, 0x60 + i), i440fx.drb[i], 1);
}

VOID I440fxReset(VOID)
{
	ULONG devFunc, i;

	devFunc = PCI_DEVFUNC_TO_ADDRESS(0, 0);

	WritePciConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, 0x04), 0x06, 1);
	WritePciConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, 0x05), 0x00, 1);
	WritePciConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, 0x06), 0x80, 1);
	WritePciConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, 0x07), 0x02, 1);
	WritePciConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, 0x0d), 0x00, 1);
	WritePciConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, 0x0f), 0x00, 1);
	WritePciConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, 0x50), 0x00, 1);
	WritePciConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, 0x51), 0x01, 1);
	WritePciConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, 0x52), 0x00, 1);
	WritePciConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, 0x54), 0x00, 1);
	WritePciConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, 0x55), 0x00, 1);
	WritePciConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, 0x56), 0x00, 1);
	WritePciConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, 0x57), 0x00, 1);
	WritePciConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, 0x58), 0x10, 1);
	for (i = 0x59; i < 0x60; i++)
		WritePciConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, i), 0x00, 1);

	WritePciConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, 0xb4), 0x00, 1);
	WritePciConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, 0xb9), 0x00, 1);
	WritePciConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, 0xba), 0x00, 1);
	WritePciConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, 0xbb), 0x00, 1);
	WritePciConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(devFunc, 0x72), 0x02, 1);

}

static ULONG I440fxPortIoReadHandler(ULONG64 Address, ULONG Length)
{
	switch (Address) {
		case 0x92:
			return i440fx.a20 << 1;
			break;
		case 0xcf8:
			return i440fx.confAddress;
			break;
		case 0xcfc:
		case 0xcfd:
		case 0xcfe:
		case 0xcff:
			i440fx.confData = ReadPciConfHandler((i440fx.confAddress & 0xfc) + (Address & 3), Length);
			break;
		default:
			break;
	}
	return i440fx.confData;
}

static VOID I440fxPortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length)
{
	switch (Address) {
		case 0x92:
			//reset
			if (Value & 2)
				i440fx.a20 = 1;
			break;
		case 0xcf8:
			i440fx.confAddress = Value;
			break;
		case 0xcfc:
		case 0xcfd:
		case 0xcfe:
		case 0xcff:
			i440fx.confData = Value;
			WritePciConfHandler((i440fx.confAddress & 0xfc) + (Address & 3), i440fx.confData, Length);
			break;
		default:
			break;
	}
}
