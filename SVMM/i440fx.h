#ifndef I440FX_H
#define I440FX_H
#include <Windows.h>

struct i440fx {
	DWORD confAddress;
	DWORD confData;
	BYTE memory_type[13][2];
	BYTE dramDetect;
	BYTE drb[8];
	BYTE a20;
};

enum memory_area_t {
	BX_MEM_AREA_C0000 = 0,
	BX_MEM_AREA_C4000,
	BX_MEM_AREA_C8000,
	BX_MEM_AREA_CC000,
	BX_MEM_AREA_D0000,
	BX_MEM_AREA_D4000,
	BX_MEM_AREA_D8000,
	BX_MEM_AREA_DC000,
	BX_MEM_AREA_E0000,
	BX_MEM_AREA_E4000,
	BX_MEM_AREA_E8000,
	BX_MEM_AREA_EC000,
	BX_MEM_AREA_F0000
};

VOID I440fxInitialize(VOID);
VOID I440fxReset(VOID);

static ULONG I440fxPortIoReadHandler(ULONG64 Address, ULONG Length);
static VOID I440fxPortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length);

static VOID I440fxPciConfWriteHandler(VOID* Pci, ULONG64 Address, ULONG Value, ULONG Length);
static ULONG I440fxPciConfReadHandler(VOID* Pci, ULONG64 Address, ULONG Length);

#endif