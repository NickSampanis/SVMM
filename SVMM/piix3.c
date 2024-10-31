#include "piix3.h"
#include "pci.h"
#include "pic.h"
#include "acpi.h"

static struct piix3 piix3;

static VOID Piix3PciConfWriteHandler(PCI* Pci, ULONG64 Address, ULONG Value, ULONG Length)
{
	ULONG i;
	BYTE byte;

	if ((Address >= 0x10) && (Address < 0x34) || Address > 0xff)
		return;

	for (i = 0; i < Length; i++) {
		byte = (Value >> i) & 0xff;
		switch (Address + i) {
			case 0x04:
				Pci->conf[Address + i] = (byte & 0x08) | 0x07;
				break;
			case 0x05:
				Pci->conf[Address + i] = (byte & 0x01);
				break;
			case 0x07:
				Value &= 0x78;
				Pci->conf[Address + i] = (Pci->conf[Address + i] & ~byte) | 0x02;
				break;
			case 0x4e:
				if (byte & 4) {

				}
				else if (byte & 0xc0) {
					//enable low-extended bios
				}
				Pci->conf[Address + i] = byte;
				break;
			case 0x4f:
				Pci->conf[Address + i] = byte & 1;
				/*
				if (DEV_ioapic_present()) {
					DEV_ioapic_set_enabled(value8 & 0x01, (BX_P2I_THIS pci_conf[0x80] & 0x3f) << 10);
				}
				if ((value8 & 0x02) != (oldval & 0x02)) {
					BX_ERROR(("1-meg extended BIOS enable switch not supported (value=%d)",
						(value8 >> 1) & 1));
					DEV_mem_set_bios_rom_access(BIOS_ROM_1MEG, (value8 >> 1) & 1);
				}
				*/
				break;
			case 0x60:
			case 0x61:
			case 0x62:
			case 0x63:
				byte &= 0x8f;
				if (byte != Pci->conf[Address + i]) {
					if (byte & 0x80) {
						Piix3UnregisterIrq(Pci, Address + i, byte);
					}
					else {
						Piix3RegisterIrq(Pci, Address + i, byte);
					}
				}
				break;
			case 0x6a:
				Pci->conf[Address + i] = byte & 0xd7;
				break;
			case 0x80:
				break;
			default:
				Pci->conf[Address + i] = byte;
				break;
		}
	}
}

static ULONG Piix3fxPciConfReadHandler(ULONG64 Address, ULONG Length)
{
	return 0;
}

static VOID Piix3PortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length)
{
	switch (Address) {
		case 0xb2:
			//DEV_acpi_generate_smi((Bit8u)value);
			piix3.APMControl = Value & 0xff;
			AcpiGenerateAcpi(piix3.APMControl);
			break;
		case 0xb3:
			piix3.APMStatus = Value & 0xff;
			break;
		case 0x4d0:
			Value &= 0xf8;
			if (Value != piix3.edgeLevelTrigger1) {
				piix3.edgeLevelTrigger1 = Value;
				PicSetMode(1, Value);
			}
			break;
		case 0x4d1:
			Value &= 0xde;
			if (Value != piix3.edgeLevelTrigger2) {
				piix3.edgeLevelTrigger2 = Value;
				PicSetMode(0, Value);

			}
			break;
		case 0xcf9:
			piix3.pciReset = 1;
			break;

	}
}

static ULONG Piix3PortIoReadHandler(ULONG64 Address, ULONG Length)
{
	switch (Address) {
		case 0xb2:
			return piix3.APMControl;
		case 0xb3:
			return piix3.APMStatus;
		case 0x4d0:
			return piix3.edgeLevelTrigger1;
		case 0x4d1:
			return piix3.edgeLevelTrigger2;
		case 0xcf9:
			return piix3.pciReset;

	}
	return 0;
}

VOID Piix3RegisterIrq(PCI* Pci, ULONG Address, BYTE irq)
{
	if (irq >= 16)
		return;
	Pci->conf[Address] = irq;
	piix3.irqRegister[irq] |= 1 << (Address & 3);
}

VOID Piix3UnregisterIrq(PCI *Pci, ULONG Address, BYTE irq)
{
	if (irq >= 16)
		return;
	Pci->conf[Address] = irq;
	piix3.irqRegister[irq] &= ~(1 << (Address & 3));

}

VOID Piix3SetIrq(PCI* Pci, BYTE devFunc, BYTE line, BYTE level)
{
	BYTE offset, irq;

	//line is irq according to the device(for example 
	offset = ((devFunc >> 3) + line - PCI_SLOTS) & 0x03;
	irq = Pci->conf[0x60 + offset];
	
	if (irq > 16 || !((1 << irq) & 0xdef8))
		return;
		
	if (level) {
		if (!piix3.irqStatus[0][irq] && !piix3.irqStatus[1][irq] &&
			!piix3.irqStatus[2][irq] && !piix3.irqStatus[3][irq]) {
			PicRaiseIrq(irq);
		}
		piix3.irqStatus[offset][irq] |= (1 << (devFunc >> 3));
	}
	else {
		piix3.irqStatus[offset][irq] &= ~(1 << (devFunc >> 3));
		if (!piix3.irqStatus[0][irq] && !piix3.irqStatus[1][irq] &&
			!piix3.irqStatus[2][irq] && !piix3.irqStatus[3][irq]) {
			PicLowerIrq(irq);
		}
	}

}
VOID Piix3Reset()
{
	USHORT devFunc;
	devFunc = BX_PCI_DEVICE(1, 0);

	PciWriteConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(0, devFunc, 0x07), 0x02, 1);
	PciWriteConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(0, devFunc, 0x4c), 0x4d, 1);
	PciWriteConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(0, devFunc, 0x4e), 0x03, 1);
	PciWriteConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(0, devFunc, 0x69), 0x02, 1);
	PciWriteConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(0, devFunc, 0x70), 0x80, 1);
	PciWriteConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(0, devFunc, 0x76), 0x0c, 1);
	PciWriteConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(0, devFunc, 0x77), 0x0c, 1);
	PciWriteConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(0, devFunc, 0x78), 0x02, 1);
	PciWriteConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(0, devFunc, 0xa0), 0x08, 1);
	PciWriteConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(0, devFunc, 0xa8), 0x0f, 1);
}

VOID Piix3Initialize()
{
	USHORT devFunc;
	ULONG Address, i;

	memset(&piix3, '\0', sizeof(piix3));
	devFunc = BX_PCI_DEVICE(1, 0);
	Address = PCI_DEVFUNC_OFFSET_TO_ADDRESS(0, devFunc, 0);
	PciRegisterConfigHandler(Address, Piix3PciConfWriteHandler, Piix3fxPciConfReadHandler);
	PciInitConfig(Address, PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371SB_0, 0x00, 0x0601, 0x00, 0x80, 0);
	Piix3Reset();
	RegisterPortIoHandler(0xb2, (WritePortIoHandlerCallback)Piix3PortIoWriteHandler, (ReadPortIoHandlerCallback)Piix3PortIoReadHandler);
	RegisterPortIoHandler(0xb3, (WritePortIoHandlerCallback)Piix3PortIoWriteHandler, (ReadPortIoHandlerCallback)Piix3PortIoReadHandler);
	RegisterPortIoHandler(0x4d0, (WritePortIoHandlerCallback)Piix3PortIoWriteHandler, (ReadPortIoHandlerCallback)Piix3PortIoReadHandler);
	RegisterPortIoHandler(0x4d1, (WritePortIoHandlerCallback)Piix3PortIoWriteHandler, (ReadPortIoHandlerCallback)Piix3PortIoReadHandler);
	RegisterPortIoHandler(0xcf9, (WritePortIoHandlerCallback)Piix3PortIoWriteHandler, (ReadPortIoHandlerCallback)Piix3PortIoReadHandler);

	PciWriteConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(0, devFunc, 0x4), 0x07, 1);
	for (i = 0; i < 4; i++)
		PciWriteConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(0, devFunc, 0x60 + i), 0x80, 1);
}
