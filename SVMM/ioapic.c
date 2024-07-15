#include "ioapic.h"
#include "apic.h"
#include "pci.h"
#include "pic.h"

IOAPIC IoApic;


static VOID IoApicSetIntr(BYTE IrqNumber)
{
	LONG Status;

	if (IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_MASKED)
		return;

	//Level can be set if IoApic.Irr is set, but NOT if IoApic.IoRedirectTable[IrqNumber].Low.IRR is set
	if (IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_LEVEL_SENSITIVE &&
		IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_IRR)
		return;

	//Deliver intr to LAPIC
	Status = ApicBusDeliverInterrupt(
		IoApic.IoRedirectTable[IrqNumber].Low & 0xff,
		IoApic.IoRedirectTable[IrqNumber].High >> 24,
		IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_DESTINATION_MODE, //1 << 11 dest mode
		IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_DELIVERY_MODE,    //7 << 8 interrupt type 
		IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_RAISED,
		IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_LEVEL_SENSITIVE);
	if (!Status) {
		if (IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_LEVEL_SENSITIVE)
			IoApic.IoRedirectTable[IrqNumber].Low |= IOAPIC_IRR;
		else
			IoApic.Irr &= ~(1UL << IrqNumber);
		IoApic.IoRedirectTable[IrqNumber].Low &= ~(IOAPIC_DELIVERY_STATUS);
	}
	else 
		IoApic.IoRedirectTable[IrqNumber].Low |= (IOAPIC_DELIVERY_STATUS);

}

VOID IoApicService(VOID)
{
	BYTE pin;

	for (pin = 0; pin < IOAPIC_NUM_PINS; pin++) {
		if (IoApic.Irr & (1UL << pin)) 
			IoApicSetIntr(pin);
	}
}

VOID IoApicLowerIrq(BYTE IrqNumber)
{
	if (!IrqNumber)
		IrqNumber = 2;

	if (IrqNumber >= IOAPIC_NUM_PINS)
		return;
	if (IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_LEVEL_SENSITIVE)
		IoApic.Irr &= ~(1UL << IrqNumber);
}

//sets RR bit in register
VOID IoApicRaiseIrq(BYTE IrqNumber)
{
	if (!IrqNumber)
		IrqNumber = 2;

	if (IrqNumber >= IOAPIC_NUM_PINS)
		return;

	if (IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_LEVEL_SENSITIVE) {
		//Level (raise the interrupt, even if its already in irr)
		IoApic.Irr |= (1UL << IrqNumber);
		IoApicService();
	}
	else {
		//Edge raise the interrupt ONLY if it's not raised already
		if (IoApic.Irr & (1UL << IrqNumber))
			return;
		IoApic.Irr |= (1UL << IrqNumber);
		IoApicService();
	}
}


VOID IoApicReceiveEndOfInterrupt(BYTE Vector)
{
	BYTE pin;

	for (pin = 0; pin < IOAPIC_NUM_PINS; pin++) {
		if (IoApic.IoRedirectTable[pin].Low & IOAPIC_LEVEL_SENSITIVE &&
			(IoApic.IoRedirectTable[pin].Low & 0xff) == Vector) {
			IoApic.IoRedirectTable[pin].Low &= (~IOAPIC_IRR);
		}
	}
}


static VOID _IoApicMMIOWriteHandler(ULONG Address, ULONG Value)
{
	BYTE i;

	
	if (Address == IOAPIC_SET_INDEX) {
		IoApic.IoRegisterSelect = Value & 0xff;
		return;
	}
	else if (Address == IOAPIC_SET_DATA) {
		//IOAPIC_SET_DATA
		switch (IoApic.IoRegisterSelect) {
			case 0:
				IoApic.IoApicId = (Value >> 24) & 0xf;
				break;
			case 1:
			case 2:
				break;
			default:
				i = (BYTE)(IoApic.IoRegisterSelect - 0x10) >> 1;
				if (i >= IOAPIC_NUM_PINS)
					return;
				if (IoApic.IoRegisterSelect & 1)
					IoApic.IoRedirectTable[i].High = Value;
				else {
					IoApic.IoRedirectTable[i].Low = Value;
					if (!(IoApic.IoRedirectTable[i].Low & IOAPIC_LEVEL_SENSITIVE)) 
						IoApic.IoRedirectTable[i].Low &= ~IOAPIC_IRR;
				}
				IoApicService();
				break;
			}
	}
}

VOID IoApicMMIOWriteHandler(ULONG Address, BYTE* Data, ULONG Length)
{
	ULONG Value;

	Address &= 0xff;

	if (Address & 3)
		return;

	
	switch (Length) {
		case 1:
			Value = *(BYTE*)Data;
			break;
		case 2:
			Value = *(WORD*)Data;
			break;
		case 4:
			Value = *(DWORD*)Data;
			break;
		default:
			return;
	}

	_IoApicMMIOWriteHandler(Address, Value);
}

static ULONG _IoApicMMIOReadHandler(ULONG Address)
{
	BYTE i;

	if (!Address)
		return IoApic.IoRegisterSelect;

	switch (IoApic.IoRegisterSelect) {
		case 0:
			return (IoApic.IoApicId & 0xf) << 24;
		case 1:
			return IoApic.IoApicVersion;
		case 2:
			return 0;
		default:
			i = (BYTE)(IoApic.IoRegisterSelect - 0x10) >> 1;
			if (i >= IOAPIC_NUM_PINS)
				return 0;
			if (IoApic.IoRegisterSelect & 1)
				return IoApic.IoRedirectTable[i].High;
			else
				return IoApic.IoRedirectTable[i].Low;
	}

}

VOID IoApicMMIOReadHandler(ULONG Address, BYTE* Data, ULONG Length)
{
	ULONG Value;


	if (Address & 3)
		return;

	Value = _IoApicMMIOReadHandler(Address);

	switch (Length) {
		case 1:
			*(BYTE*)Data = Value & 0xff;
			break;
		case 2:
			*(WORD*)Data = Value & 0xffff;
			break;
		case 4:
			*(DWORD*)Data = Value;
			break;

	}
	
}

VOID IoApicInitialize(VOID)
{
	int i;

	memset(&IoApic, '\0', sizeof(IoApic));
	IoApic.IoApicId = 1;
	IoApic.IoApicVersion = ((IOAPIC_NUM_PINS - 1) << 16) | IOAPIC_VERSION_82093AA;
	RegisterMMIOHandler(IOAPIC_DEFAULT_MMIO, IoApicMMIOWriteHandler, IoApicMMIOReadHandler);

	for (i = 0; i < IOAPIC_NUM_PINS; i++) {
		IoApic.IoRedirectTable[i].Low = IOAPIC_MASKED;
		IoApic.IoRedirectTable[i].High = 0;
	}
}
