#include "ioapic.h"
#include "apic.h"
#include "pci.h"
#include "pic.h"

IOAPIC IoApic;

static VOID IoApicService(VOID)
{
	DWORD i,Dest;
	BYTE Vector;

	for (i = 0; i < IOAPIC_NUM_PINS; i++) {
		if (IoApic.Irr & (1 << i) && !(IoApic.IoRedirectTable[i].Low & IOAPIC_MASKED)) {
			if ((IoApic.IoRedirectTable[i].Low & IOAPIC_DELIVERY_MODE) == IOAPIC_EXTERNAL_INT) {
				Vector = PicIAC();
			}
			else {
				Vector = IoApic.IoRedirectTable[i].Low & 0xff;
			}
			Dest = IoApic.IoRedirectTable[i].High >> 24;
			ApicBusDeliverInterrupt(Vector, Dest, 
				IoApic.IoRedirectTable[i].Low & IOAPIC_DESTINATION_MODE, //1 << 11 dest mode
				IoApic.IoRedirectTable[i].Low & IOAPIC_DELIVERY_MODE,    //7 << 8 interrupt type 
				IoApic.IoRedirectTable[i].Low & IOAPIC_RAISED, 
				IoApic.IoRedirectTable[i].Low & IOAPIC_LEVEL_SENSITIVE);

			if (IoApic.IoRedirectTable[i].Low & IOAPIC_LEVEL_SENSITIVE) {
				IoApic.Irr &= ~(1 << i);
			}
		}
	}
}



VOID IoApicSetIrq(BYTE IrqNumber, BYTE Raise)
{
	BYTE triggerMode;

	if (!IrqNumber)
		IrqNumber = 2;

	if (IrqNumber >= IOAPIC_NUM_PINS)
		return;

	if (!Raise) {
		IoApic.Irr &= ~(1 << IrqNumber);
		return;
	}

	if (IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_LEVEL_SENSITIVE) {
		//LEVEL raise the interrupt only if wasn't raised and wasn't accepted already, then set IRR in table
		if (IoApic.Irr & (1 << IrqNumber) || IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_IRR)
			return;
		IoApic.Irr |= (1 << IrqNumber);
		ApicBusDeliverInterrupt(IrqNumber,
			IoApic.IoRedirectTable[IrqNumber].High >> 24,
			IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_DESTINATION_MODE, //1 << 11 dest mode
			IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_DELIVERY_MODE,    //7 << 8 interrupt type 
			IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_RAISED,
			IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_LEVEL_SENSITIVE);
		IoApic.IoRedirectTable[IrqNumber].Low |= IOAPIC_IRR;
	}
	else {
		//EDGE (raise the interrupt, even if its already in irr)
		IoApic.Irr |= (1 << IrqNumber);
		ApicBusDeliverInterrupt(IrqNumber,
			IoApic.IoRedirectTable[IrqNumber].High >> 24,
			IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_DESTINATION_MODE, //1 << 11 dest mode
			IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_DELIVERY_MODE,    //7 << 8 interrupt type 
			IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_RAISED,
			IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_LEVEL_SENSITIVE);

	}

}

/*

VOID IoApicSetIrq(BYTE IrqNumber, BYTE Raise)
{
	if (!IrqNumber)
		IrqNumber = 2;

	if (IrqNumber < IOAPIC_NUM_PINS) {
		if (Raise << IrqNumber != (IrqNumber & (1 << IrqNumber))) {
			if (IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_LEVEL_SENSITIVE) {
				if (Raise) {
					//LEVEL raise the interrupt only if wasn't raised
					IoApic.IntIn |= (1 << IrqNumber);
					IoApic.Irr |= (1 << IrqNumber);
					IoApicService();
				}
				else {
					IoApic.IntIn &= ~(1 << IrqNumber);
					IoApic.Irr &= ~(1 << IrqNumber);
				}
			}
			else { 
				if (Raise) {
					//EDGE (raise the interrupt, even if its already in irr)
					IoApic.IntIn |= (1 << IrqNumber);
					if (!(IoApic.IoRedirectTable[IrqNumber].Low & IOAPIC_MASKED)) {
						IoApic.Irr |= (1 << IrqNumber);
						IoApicService();
					}
					else
						IoApic.IntIn &= ~(1 << IrqNumber);
				}
			}
		}
	}
}
*/
VOID IoApicReceiveEndOfInterrupt(BYTE Vector)
{

}

#define IOAPIC_SET_INDEX						0x00
#define IOAPIC_SET_DATA                         0x10
#define IOAPIC_SET_EOI                          0x40    /* Newer I/O APIC only. */


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
				i = (IoApic.IoRegisterSelect - 0x10) >> 1;
				if (i >= IOAPIC_NUM_PINS)
					return;
				if (IoApic.IoRegisterSelect & 1)
					IoApic.IoRedirectTable[i].High = Value;
				else
					IoApic.IoRedirectTable[i].Low = Value;
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
			i = (IoApic.IoRegisterSelect - 0x10) >> 1;
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
	IoApic.IoApicVersion = ((IOAPIC_NUM_PINS - 1) << 16) | 0x11;
	RegisterMMIOHandler(IOAPIC_DEFAULT_MMIO, IoApicMMIOWriteHandler, IoApicMMIOReadHandler);

	for (i = 0; i < IOAPIC_NUM_PINS; i++) {
		IoApic.IoRedirectTable[i].Low = 0x00010000;
		IoApic.IoRedirectTable[i].High = 0;
	}
}
