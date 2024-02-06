#pragma once
#include <Windows.h>

#define IOAPIC_DEFAULT_MMIO	0xfec00000
#define IOAPIC_NUM_PINS		24

#define IOAPIC_LEVEL_SENSITIVE	(1 << 15)

#define IOAPIC_DESTINATION		(0xff << 24)
#define IOAPIC_MASKED			(1 << 16)
#define IOAPIC_IRR				(1 << 14)
#define IOAPIC_RAISED			(1 << 13)
#define IOAPIC_DESTINATION_MODE	(1 << 11)
#define IOAPIC_DELIVERY_MODE	(7 << 8)

#define IOAPIC_EXTERNAL_INT		(0x700)

typedef struct _IO_REDIRECT_ENTRY {
	DWORD High;
	DWORD Low;
	
} IO_REDIRECT_ENTRY, *PIO_REDIRECT_ENTRY;

typedef struct _IOAPIC {
	DWORD IoRegisterSelect;
	DWORD Data;
	DWORD IoApicId;
	DWORD IoApicVersion;
	IO_REDIRECT_ENTRY IoRedirectTable[24];
	DWORD Irr;
	DWORD IntIn;
} IOAPIC, *PIOAPIC;

VOID IoApicInitialize(VOID);
VOID IoApicSetIrq(BYTE IrqNumber, BYTE Raise);

