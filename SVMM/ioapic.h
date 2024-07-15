#pragma once
#include <Windows.h>

#define IOAPIC_VERSION_82093AA	0x11
#define IOAPIC_DEFAULT_MMIO		0xfec00000
#define IOAPIC_NUM_PINS			24


#define IOAPIC_DESTINATION		(0xffULL << 24)
#define IOAPIC_MASKED			(1UL << 16)
#define IOAPIC_LEVEL_SENSITIVE	(1UL << 15)
#define IOAPIC_IRR				(1UL << 14)
#define IOAPIC_RAISED			(1UL << 13)
#define IOAPIC_DELIVERY_STATUS	(1UL << 12)
#define IOAPIC_DESTINATION_MODE	(1UL << 11)
#define IOAPIC_DELIVERY_MODE	(7UL << 8)

#define IOAPIC_SET_INDEX		0x00
#define IOAPIC_SET_DATA         0x10

#define IOAPIC_EXTERNAL_INT		(0x700)

typedef struct _IO_REDIRECT_ENTRY {
	DWORD High;
	DWORD Low;
	
} IO_REDIRECT_ENTRY, *PIO_REDIRECT_ENTRY;

typedef struct _IOAPIC {
	DWORD IoRegisterSelect;
	DWORD IoApicId;
	DWORD IoApicVersion;
	IO_REDIRECT_ENTRY IoRedirectTable[24];
	DWORD Irr;
} IOAPIC, *PIOAPIC;

VOID IoApicInitialize(VOID);
VOID IoApicRaiseIrq(BYTE IrqNumber);
VOID IoApicLowerIrq(BYTE IrqNumber);
VOID IoApicReceiveEndOfInterrupt(BYTE Vector);


