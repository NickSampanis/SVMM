#ifndef PIT_H
#define PIT_H
#include <Windows.h>

#define PIT_CHANNEL0_DATA	0x40
#define PIT_CHANNEL1_DATA	0x41
#define PIT_CHANNEL2_DATA	0x42
#define PIT_CONTROL_CMD		0x43


#define PIT_READ_BACK		3

#define PIT_ACCESS_LSBYTE       1
#define PIT_ACCESS_MSBYTE       2
#define PIT_ACCESS_LS_MS_BYTE   3
#define PIT_ACCESS_MS_LS_BYTE   4



struct Counter {
	BYTE writeState;
	BYTE readState;
	BYTE mode;
	BYTE access;
	BYTE nullCount;
	BYTE bcdMode;
	BYTE outPin;
	USHORT inlatch;
	USHORT outlatch;

	BYTE statusLatch;
	BYTE statusLatched;
	BYTE nextChangeTime;
	DWORD countBinary;
	DWORD count;
	BYTE countLatched;
	BYTE gate;
	BYTE triggerGate;
};

struct Pit {
	BYTE controlWord;
	DWORD timerId;
	struct Counter counter[3];
};

VOID PitInitialize();
ULONG PitReadHandler(ULONG64 Address, ULONG Length);
VOID PitWriteHandler(ULONG64 Address, ULONG Value, ULONG Length);
#endif