#include "pit.h"
#include "pci.h"
#include "pic.h"
#include "timer.h"
#include "gvm.h"

static struct Pit Pit;

#define mod_64(x, y) ((x) - (y) * div64_u64(x, y))

void PitSetOut(struct Counter* Counter, CHAR Data)
{
	if (Counter->outPin != Data) {
		Counter->outPin = Data;
		if (Data)
			PicRaiseIrq(0);
		else
			PicLowerIrq(0);
	}
}

void PitSetCountToBinary(struct Counter* Counter)
{
	if (Counter->bcdMode) {
		Counter->count =
			(((Counter->countBinary / 1) % 10) << 0) |
			(((Counter->countBinary / 10) % 10) << 4) |
			(((Counter->countBinary / 100) % 10) << 8) |
			(((Counter->countBinary / 1000) % 10) << 12);
	}
	else {
		Counter->count = Counter->countBinary;
	}
}

void PitDecrement(struct Counter* Counter)
{
	if (!Counter->count) {
		if (Counter->bcdMode) {
			Counter->count = 0x9999;
			Counter->countBinary = 9999;
		}
		else {
			Counter->count = 0xFFFF;
			Counter->countBinary = 0xFFFF;
		}
	}
	else {
		Counter->countBinary--;
		PitSetCountToBinary(Counter);
	}
}


void PitDecrementMultiple(struct Counter *Counter, DWORD Cycles)
{
	while (Cycles > 0) {
		if (Cycles <= Counter->countBinary) {
			Counter->countBinary -= Cycles;
			Cycles -= Cycles;
			PitSetCountToBinary(Counter);
		}
		else {
			Cycles -= (Counter->countBinary + 1);
			Counter->countBinary -= Counter->countBinary;
			PitSetCountToBinary(Counter);
			PitDecrement(Counter);
		}
	}
}

VOID PitSetCount(struct Counter* Counter, DWORD Data)
{
	Counter->count = Data & 0xffff;
	PitSetCountToBinary(Counter);
}

VOID PitTimerHandler(VOID* Param)
{
	ULONG Channel;

	Channel = (ULONG)Param;
	//Pit.counter[Channel].totalTicks -= Pit.counter[Channel].tickChunk;
	//Pit.counter[Channel].totalTicks -= TICK_PERIOD;
	if (Channel) {
		Pit.counter[Channel].count = ~Pit.counter[Channel].count;
		return Pit.counter[Channel].count;
	}
	Pit.counter[Channel].count -= TICK_PERIOD;
	
	switch (Pit.counter[Channel].mode) {
		case 0:
		case 1:
		case 4:
			if (Pit.counter[Channel].count <= 0) {
				Pit.counter[Channel].outPin = 1;
				Pit.counter[Channel].count = 0xffff;
			}
			break;
		case 2:
		case 3:
			if (Pit.counter[Channel].count <= 0) {
				Pit.counter[Channel].outPin = ~Pit.counter[Channel].outPin;
				Pit.counter[Channel].count = Pit.counter[Channel].initCount;
			}
			break;
		case 5:
			break;
	}

	if (Pit.counter[Channel].outPin)
		PicRaiseIrq(0);
	else 
		PicLowerIrq(0);

	
}

static int PitGetOut(ULONG Channel)
{
	LONG64 elapsed, interval;
	int out;

	elapsed = TimerGetElapsed(Pit.counter[Channel].timerId);
	if (elapsed)
		interval = (PIT_TICKS_PER_SECOND / elapsed);
	else
		interval = 0;
	switch (Pit.counter[Channel].mode) {
		default:
		case 0:
			out = (interval >= Pit.counter[Channel].count);
			break;
		case 1:
			out = (interval < Pit.counter[Channel].count);
			break;
		case 2:
			out = ((mod_64(interval, Pit.counter[Channel].count) == 0) && (interval != 0));
			break;
		case 3:
			out = (mod_64(interval, Pit.counter[Channel].count) < ((Pit.counter[Channel].count + 1) >> 1));
			break;
		case 4:
		case 5:
			out = (interval == Pit.counter[Channel].count);
			break;
	}

	return out;
}

VOID PitLatchStatus(ULONG Channel)
{
	if (!Pit.counter[Channel].statusLatched) {
		Pit.counter[Channel].statusLatch = ((PitGetOut(Channel) << 7) |
			(Pit.counter[Channel].rwMode << 4) |
			(Pit.counter[Channel].mode << 1) |
			Pit.counter[Channel].bcdMode);
		Pit.counter[Channel].statusLatched = 1;
	}
}

VOID PitLatchCount(ULONG Channel)
{
	if (!Pit.counter[Channel].countLatched) {
		Pit.counter[Channel].outlatch = Pit.counter[Channel].count;
		Pit.counter[Channel].countLatched = Pit.counter[Channel].rwMode;
	}
}


VOID PitLoadCount(ULONG Channel, ULONG Value)
{
	ULONG timesPerSec;

	if (!Value)
		Pit.counter[Channel].initCount = Pit.counter[Channel].count = 0x10000;
	else 
		Pit.counter[Channel].initCount = Pit.counter[Channel].count = Value & 0xffff;

	Pit.counter[Channel].countWritten = 1;
	timesPerSec = (PIT_TICKS_PER_SECOND / Pit.counter[Channel].count);
	if (timesPerSec > 0x10000)
		timesPerSec = 0x10000;
	//Pit.counter[Channel].tickChunk = (SECOND_TO_TICKS / timesPerSec) & (~0xff);
	Pit.counter[Channel].totalTicks = Pit.counter[Channel].totalTicksInit = TICK_PERIOD * timesPerSec;
	
	
	switch (Pit.counter[Channel].mode) {
		case 0:
		case 1:
		case 4:
			Pit.counter[Channel].outPin = 0;
			TimerActivate(Pit.counter[Channel].timerId, TICK_PERIOD, 1);
			break;
		case 2:
		case 3:
			TimerActivate(Pit.counter[Channel].timerId, TICK_PERIOD, 1);
			break;
		case 5:
			TimerDeactivate(Pit.counter[Channel].timerId);
			break;
	}

	//if (Channel)
		//return;
}


ULONG PitReadHandler(ULONG Address, ULONG Length)
{
	BYTE Channel, RefreshClockDiv;
	TIMER Timer;
	DWORD Ret;
	
	if (Address == 0x61)
		Channel = 2;
	else
		Channel = (Address - 0x40) & 2;

	/*
	if (Pit.totalTicks == TimerGetTotalTicks())
		Pit.counter[Channel].count--;
	else {
		Pit.totalTicks = TimerGetTotalTicks();
		Pit.counter[Channel].count = (Pit.counter[Channel].count - Pit.counter[Channel].countChunk) & ~(Pit.counter[Channel].countChunk - 1);
	}
	*/
	if (Address == 0x61)
		return Pit.counter[Channel].count;
	

	if (Pit.counter[Channel].statusLatched) {
		Pit.counter[Channel].statusLatched = 0;
		return Pit.counter[Channel].statusLatch;
	}
	else if (Pit.counter[Channel].countLatched) {
		switch (Pit.counter[Channel].countLatched) {
			default:
			case RW_STATE_LSB:
				Pit.counter[Channel].countLatched = 0;
				return Pit.counter[Channel].outlatch & 0xff;
			case RW_STATE_MSB:
				Pit.counter[Channel].countLatched = 0;
				return (Pit.counter[Channel].outlatch >> 8) & 0xff;
			case RW_STATE_WORD0:
				Pit.counter[Channel].countLatched = RW_STATE_MSB;
				return Pit.counter[Channel].outlatch & 0xff;
		}

	}
	else {
		switch (Pit.counter[Channel].readState) {
			default:
			case RW_STATE_LSB:
				//PitGetCount(Channel)
				return Pit.counter[Channel].count & 0xff;
			case RW_STATE_MSB:
				return (Pit.counter[Channel].count >> 8) & 0xff;
			case RW_STATE_WORD0:
				Pit.counter[Channel].readState = RW_STATE_MSB;
				return Pit.counter[Channel].count & 0xff;
			case RW_STATE_WORD1:
				Pit.counter[Channel].readState = RW_STATE_WORD0;
				return (Pit.counter[Channel].count >> 8) & 0xff;
		}
	}
}



VOID PitWriteHandler(ULONG Address, ULONG Value, ULONG Length)
{
	BYTE Channel, Access;

	if (Address == 0x43) 
		Channel = (Value >> 6) & 2;

	else 
		Channel = (BYTE)Address & 2;
	
	/*
	if (Pit.counter[Channel].countWritten) {
		if (Pit.totalTicks == TimerGetTotalTicks())
			Pit.counter[Channel].count--;
		else {
			Pit.totalTicks = TimerGetTotalTicks();
			Pit.counter[Channel].count = (Pit.counter[Channel].count - Pit.counter[Channel].countChunk) & ~(Pit.counter[Channel].countChunk - 1);
		}
		if (Pit.counter[Channel].count < 0)
			Pit.counter[Channel].count = Pit.counter[Channel].initCount;
	}
	*/
	switch (Address) {
		case 0x40:
		case 0x41:
		case 0x42:
			/* Write Count */
			switch (Pit.counter[Channel].writeState) {
				default:
				case RW_STATE_LSB:
					PitLoadCount(Channel, Value);
					break;
				case RW_STATE_MSB:
					PitLoadCount(Channel, Value << 8);
					break;
				case RW_STATE_WORD0:
					Pit.counter[Channel].inlatch = (BYTE)Value;
					Pit.counter[Channel].writeState = RW_STATE_WORD1;
					break;
				case RW_STATE_WORD1:
					PitLoadCount(Channel, Pit.counter[Channel].inlatch | (Value & 0xff) << 8);
					Pit.counter[Channel].writeState = RW_STATE_WORD0;
					break;
			}
			break;
		case 0x43:
			Channel = (Value >> 6) & 3;
			Access = (Value >> 4) & 3;
			if (Channel == PIT_READ_BACK) {
				for (Channel = 0; Channel < 3; Channel++) {
					if (Value & (2 << Channel)) {
						//If we are using this counter;
						if (!(Value & 0x20)) {
							PitLatchCount(Channel);
						}
						if (!(Value & 0x10) && !Pit.counter[Channel].statusLatched)
							PitLatchStatus(Channel);
					}
				}
			}
			else {
				if (!Access) 
					PitLatchCount(Channel); //save time elapsed
				else {
					//select counter/channel
					Pit.counter[Channel].readState = Access;
					Pit.counter[Channel].writeState = Access;
					Pit.counter[Channel].mode = (Value >> 1) & 0x7;
					if (Pit.counter[Channel].mode > 5)
						Pit.counter[Channel].mode -= 4;

					Pit.counter[Channel].rwMode = Access;
					Pit.counter[Channel].bcdMode = Value & 1;
				}
			}
			break;
		case 0x61:
			PitLoadCount(2, TICK_PERIOD);
			break;
	}
}


VOID PitInitialize()
{
	DWORD i;

	memset(&Pit, '\0', sizeof(Pit));

	for (i = 0x40; i <= 0x43; i++)
		RegisterPortIoHandler(i, (WritePortIoHandlerCallback)PitWriteHandler, (ReadPortIoHandlerCallback)PitReadHandler);

	RegisterPortIoHandler(0x61, (WritePortIoHandlerCallback)PitWriteHandler, (ReadPortIoHandlerCallback)PitReadHandler);
	
	for (i = 0; i < 3; i++) {
		Pit.counter[i].readState = RW_STATE_LSB;
		Pit.counter[i].writeState = RW_STATE_LSB;
		Pit.counter[i].gate = 1;
		Pit.counter[i].outPin = 1;
		Pit.counter[i].triggerGate = 0;
		Pit.counter[i].mode = 4;
		Pit.counter[i].count = 0;
		Pit.counter[i].nullCount = 0;
		Pit.counter[i].rwMode = 1;
		Pit.counter[i].statusLatch = 0;
		Pit.counter[i].nextChangeTime = 0;
		Pit.counter[i].countChunk = 0;
		//Pit.counter[i].timerId = TimerRegister(MSECONDS_TO_NS(1), PitTimerHandler, (VOID *)i);
		Pit.counter[i].timerId = TimerRegister(TICK_PERIOD, PitTimerHandler, (VOID*)i);
		if (i != 2)
			TimerDeactivate(Pit.counter[i].timerId);
	}
	Pit.totalTicks = TimerGetTotalTicks();

}