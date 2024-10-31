#pragma once
#include <Windows.h>

struct Flash {
	BYTE Cmd;
	BYTE Status;
	BYTE Wcycle;
	DWORD Features;
	BYTE CfiTable[0x52];
	BYTE* Storage;

};


VOID FlashInitialize(BYTE* BiosRom);
