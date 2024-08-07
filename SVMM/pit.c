#include "pit.h"
#include "pci.h"
#include "pic.h"
#include "timer.h"
#include "gvm.h"



static struct Pit Pit;

static BYTE PitChannelSound;

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

LONG64 PitGetNextTransitionTime(ULONG Channel, ULONG64 CurrentTime)
{
	ULONG64 Result, HighResult, NextTime, base;
	ULONG period2;

	HighResult = 0;
	Result = muldiv64(CurrentTime - Pit.counter[Channel].countLoadTime, PIT_FREQ, NANOSECONDS_PER_SECOND);

	switch (Pit.counter[Channel].mode) {
		default:
		case 0:
		case 1:
			if (Result < Pit.counter[Channel].count)
				NextTime = Pit.counter[Channel].count;
			else 
				return -1;
			
			break;
		case 2:
			base = (Result / Pit.counter[Channel].count) * Pit.counter[Channel].count;
			NextTime = base + Pit.counter[Channel].count;
			break;
		case 3:
			base = (Result / (ULONG64)Pit.counter[Channel].count) * (ULONG64)Pit.counter[Channel].count;
			period2 = (Pit.counter[Channel].count + 1) >> 1;
			if ((Result - base) < period2) 
				NextTime = base + period2;
			
			else 
				NextTime = base + Pit.counter[Channel].count;
			
			break;
		case 4:
		case 5:
			if (Result < Pit.counter[Channel].count) 
				NextTime = Pit.counter[Channel].count;
			else 
				return -1;
			
			break;
	}
	/* convert to timer units */
	Result = muldiv64(NextTime, NANOSECONDS_PER_SECOND, PIT_FREQ);
	NextTime = Pit.counter[Channel].countLoadTime + Result;
	/* fix potential rounding problems */
	/* XXX: better solution: use a clock at PIT_FREQ Hz */
	if (NextTime <= CurrentTime) 
		NextTime = CurrentTime;
	
	return NextTime + 1;
}



VOID PitLatchStatus(ULONG Channel)
{
	if (!Pit.counter[Channel].statusLatched) {
		Pit.counter[Channel].statusLatch = ((PitGetOut(Channel, Pit.counter[Channel].countLoadTime) << 7) |
			(Pit.counter[Channel].rwMode << 4) |
			(Pit.counter[Channel].mode << 1) |
			Pit.counter[Channel].bcdMode);
		Pit.counter[Channel].statusLatched = 1;
	}
}

LONG PitGetCount(ULONG Channel)
{
	ULONG64 Result;
	int counter;

	Result = muldiv64(TimerGetClockNs() - Pit.counter[Channel].countLoadTime, PIT_FREQ, NANOSECONDS_PER_SECOND);
	switch (Pit.counter[Channel].mode) {
		case 0:
		case 1:
		case 4:
		case 5:
			counter = (Pit.counter[Channel].count - Result) & 0xffff;
			break;
		case 3:
			counter = Pit.counter[Channel].count - ((2 * Result) % Pit.counter[Channel].count);
			break;
		default:
			counter = Pit.counter[Channel].count - (Result % Pit.counter[Channel].count);
			break;
	}
	return counter;
}

VOID PitLatchCount(ULONG Channel)
{
	if (!Pit.counter[Channel].countLatched) {
		Pit.counter[Channel].outlatch = PitGetCount(Channel);
		Pit.counter[Channel].countLatched = Pit.counter[Channel].rwMode;
	}
}

/* get pit output bit */
LONG PitGetOut(ULONG Channel, ULONG64 CurrentTime)
{
	ULONG64 Result, HighResult;
	LONG out;

	HighResult = 0;
	Result = muldiv64(CurrentTime - Pit.counter[Channel].countLoadTime, PIT_FREQ, NANOSECONDS_PER_SECOND);

	switch (Pit.counter[Channel].mode) {
		default:
		case 0:
			out = (Result >= Pit.counter[Channel].count);
			break;
		case 1:
			out = (Result < Pit.counter[Channel].count);
			break;
		case 2:
			if ((Result % Pit.counter[Channel].count) == 0 && Result != 0) 
				out = 1;
			
			else 
				out = 0;
			
			break;
		case 3:
			out = (Result % Pit.counter[Channel].count) < ((Pit.counter[Channel].count + 1) >> 1);
			break;
		case 4:
		case 5:
			out = (Result == Pit.counter[Channel].count);
			break;
	}
	return out;
}

static void PitIrqTimerUpdate(ULONG Channel, ULONG64 CurrentTime, BYTE Timer)
{
	LONG64 expire_time;
	int irq_level;


	expire_time = PitGetNextTransitionTime(Channel, CurrentTime);
	irq_level = PitGetOut(Channel, CurrentTime);

	switch (Pit.counter[Channel].mode) {
		case 2:
		case 4:
		case 5:
			if (Timer && Pit.InterruptEnabled) {
				/* flip flop*/
				PicLowerIrq(0);
				PicRaiseIrq(0);
			}
			break;
		default:
			if (Pit.InterruptEnabled) {
				if (irq_level)
					PicRaiseIrq(0);
				else
					PicLowerIrq(0);
			}
	}
	
	Pit.counter[Channel].nextTransitionTime = expire_time;
	if (expire_time != -1)
		TimerActivate(Pit.counter[Channel].timerId, expire_time, 0);
	else
		TimerDeactivate(Pit.counter[Channel].timerId);
}


VOID PitTimerHandler(VOID* Param)
{
	ULONG Channel;

	Channel = (ULONG)Param;
	
	PitIrqTimerUpdate(Channel, Pit.counter[Channel].nextTransitionTime, 1);

}

VOID PitLoadCount(ULONG Channel, ULONG Value)
{
	if (!Value)
		Pit.counter[Channel].count = 0x10000;
	else 
		Pit.counter[Channel].count = Value & 0xffff;

	Pit.counter[Channel].countWritten = 1;
	Pit.counter[Channel].countLoadTime = TimerGetClockNs();

	PitIrqTimerUpdate(Channel, Pit.counter[Channel].countLoadTime, 0);

}

VOID PitSetGate(ULONG64 Channel, ULONG Value)
{
	switch (Pit.counter[Channel].mode)
	{
		default:
		case 0:
		case 4:
			break;
		case 1:
		case 5:
			if (Pit.counter[Channel].gate < Value)
			{
				Pit.counter[Channel].countLoadTime = TimerGetClockNs();
				PitIrqTimerUpdate(Channel, Pit.counter[Channel].countLoadTime, 0);

			}
			break;
		case 2:
		case 3:
			if (Pit.counter[Channel].gate < Value)
			{
				Pit.counter[Channel].countLoadTime = TimerGetClockNs();
				PitIrqTimerUpdate(Channel, Pit.counter[Channel].countLoadTime, 0);

			}
			break;
	}
	Pit.counter[2].gate = Value;
}

ULONG PitReadHandler(ULONG Address, ULONG Length)
{
	BYTE Channel;
	ULONG Refresh, Out;
	static ULONG64 ClockNs = 0;

	Channel = (Address - 0x40) & 2;

	if (Address == 0x61) {
		Refresh = ((TimerGetClockNs() + ClockNs) / 15085) & 1;
		Out = PitGetOut(2, TimerGetClockNs() + ClockNs);
		ClockNs += 4000000;
		return (Pit.counter[2].gate) | (Refresh << 4) | (Out << 5);
	}
		
	

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
				return PitGetCount(Channel) & 0xff;
			case RW_STATE_MSB:
				return (PitGetCount(Channel) >> 8) & 0xff;
			case RW_STATE_WORD0:
				Pit.counter[Channel].readState = RW_STATE_MSB;
				return PitGetCount(Channel) & 0xff;
			case RW_STATE_WORD1:
				Pit.counter[Channel].readState = RW_STATE_WORD0;
				return (PitGetCount(Channel) >> 8) & 0xff;
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
			PitChannelSound = (Value >> 1) & 1;
			PitSetGate(2, Value & 1);
			break;
	}
}

VOID PitSetInterrupt(BYTE Enable)
{
	Pit.InterruptEnabled = Enable;
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
		Pit.counter[i].triggerGate = 0;
		Pit.counter[i].mode = 4;
		Pit.counter[i].count = 0;
		Pit.counter[i].rwMode = 1;
		Pit.counter[i].statusLatch = 0;
		Pit.counter[i].nextTransitionTime = 0;
	}
	Pit.counter[0].timerId = TimerCreate(PitTimerHandler, (VOID *)NULL);
	//Pit.counter[0].countLoadTime = TimerGetClockNs();
	//Pit.counter[0].nextTransitionTime = PitGetNextTransitionTime(0, Pit.counter[0].countLoadTime);
	Pit.totalTicks = TimerGetTotalTicks();
	Pit.InterruptEnabled = 1;

}