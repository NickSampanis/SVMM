#ifndef PIT_H
#define PIT_H
#include <Windows.h>
#include "timer.h"

//1.193181MHz Clock
#define PIT_TICKS_PER_SECOND (1193181)

#define TICKS_TO_USEC(a) (((a)*PIT_USEC_PER_SECOND)/PIT_TICKS_PER_SECOND)
#define USEC_TO_TICKS(a) (((a)*PIT_TICKS_PER_SECOND)/PIT_USEC_PER_SECOND)

#define PIT_CHANNEL0_DATA	0x40
#define PIT_CHANNEL1_DATA	0x41
#define PIT_CHANNEL2_DATA	0x42
#define PIT_CONTROL_CMD		0x43


#define PIT_READ_BACK		3

#define PIT_ACCESS_LSBYTE       1
#define PIT_ACCESS_MSBYTE       2
#define PIT_ACCESS_LS_MS_BYTE   3
#define PIT_ACCESS_MS_LS_BYTE   4

#define RW_STATE_LSB 1
#define RW_STATE_MSB 2
#define RW_STATE_WORD0 3
#define RW_STATE_WORD1 4


struct Counter {
	BYTE writeState;
	BYTE readState;
	BYTE mode;
	BYTE rwMode;
	BYTE nullCount;
	BYTE bcdMode;
	BYTE outPin;
	BYTE countWritten;
	USHORT inlatch;
	USHORT outlatch;

	BYTE statusLatch;
	BYTE statusLatched;
	BYTE nextChangeTime;
	DWORD countBinary;
	LONG count;
	LONG initCount;
	LONG countChunk;
	LONG tickChunk;
	LONG totalTicks;
	LONG totalTicksInit;
	BYTE countLatched;
	BYTE gate;
	BYTE triggerGate;
	DWORD timerId;
	ULONG64 countLoadTime;
};

struct Pit {
	BYTE controlWord;
	struct Counter counter[3];
	ULONG64 totalTicks;
	BYTE NextEventTime;
};

VOID PitInitialize();
ULONG PitReadHandler(ULONG Address, ULONG Length);
VOID PitWriteHandler(ULONG Address, ULONG Value, ULONG Length);
#endif