#include "parallel.h"
#include "pci.h"

PARALLEL Paral;

VOID ParallelInitialize()
{
	ULONG i;

	for (i = 0; i < 3; i++) {
		RegisterPortIoHandler(0x278 + i, (WritePortIoHandlerCallback)ParallelPortIoWriteHandler, (ReadPortIoHandlerCallback)ParallelPortIoReadHandler);
		RegisterPortIoHandler(0x378 + i, (WritePortIoHandlerCallback)ParallelPortIoWriteHandler, (ReadPortIoHandlerCallback)ParallelPortIoReadHandler);
	}
}

ULONG ParallelPortIoReadHandler(ULONG64 Address, ULONG Length)
{
	BYTE Offset;

	Offset = Address & 7;

	switch (Offset) {
		case BX_PAR_DATA:
			return 0xff;
			break;
		case BX_PAR_STAT:
			return 0xff;
			break;
		case BX_PAR_CTRL:
			return 0xff;
			break;
	}
}

VOID ParallelPortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length)
{
	BYTE Offset;

	Offset = Address & 7;

	switch (Offset) {
		case BX_PAR_DATA:
			break;
		case BX_PAR_STAT:
			break;
		case BX_PAR_CTRL:
			break;
	}
}
