#include "pit.h"
#include "pci.h"
#include "pic.h"
#include "timer.h"

static struct Pit Pit;

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

VOID PitTimerHandler()
{
	DWORD i;

	for (i = 0; i < 3; i++) {
		if (!Pit.counter[i].nextChangeTime)
			continue;
		switch (Pit.counter[i].mode) {
			case 2:
				if (Pit.counter[i].triggerGate) {
					if (!Pit.counter[i].outPin)
						PitSetOut(&Pit.counter[i], 1);
					Pit.counter[i].triggerGate = 0;
				}
				else {
					PitSetOut(&Pit.counter[i], 0);
					Pit.counter[i].triggerGate = 1;
				}
				break;
		}
	}
}

VOID PitInitialize()
{
	DWORD i;

	for (i = 0x40; i <= 0x43; i++)
		RegisterPortIoHandler(i, (WritePortIoHandlerCallback)PitWriteHandler, (ReadPortIoHandlerCallback)PitReadHandler);

	RegisterPortIoHandler(0x61, (WritePortIoHandlerCallback)PitWriteHandler, (ReadPortIoHandlerCallback)PitReadHandler);

	memset(&Pit, '\0', sizeof(Pit));

	for (i = 0; i < 3; i++) {
		Pit.counter[i].readState = PIT_ACCESS_LSBYTE;
		Pit.counter[i].writeState = PIT_ACCESS_LSBYTE;
		Pit.counter[i].gate = 1;
		Pit.counter[i].outPin = 1;
		Pit.counter[i].triggerGate = 0;
		Pit.counter[i].mode = 4;
		//Pit.counter[i].first_pass = 0;
		//Pit.counter[i].bcd_mode = 0;
		Pit.counter[i].count = 0;
		//Pit.counter[i].count_binary = 0;
		//Pit.counter[i].state_bit_1 = 0;
		//Pit.counter[i].state_bit_2 = 0;
		Pit.counter[i].nullCount = 0;
		Pit.counter[i].access = 1;
		//Pit.counter[i].countWritten = 1;
		Pit.counter[i].statusLatch = 0;
		Pit.counter[i].nextChangeTime = 0;
	}
	Pit.timerId = TimerRegister(0x034fb5e3 * 4, PitTimerHandler);
}



ULONG PitReadHandler(ULONG64 Address, ULONG Length)
{
	BYTE Channel;

	Channel = (Address - 0x40) & 2;
	
	if (Pit.counter[Channel].statusLatched) {
		Pit.counter[Channel].statusLatched = 0;
		return Pit.counter[Channel].statusLatch;
	}
	else if (Pit.counter[Channel].countLatched) {
		switch (Pit.counter[Channel].countLatched) {
		default:
		case PIT_ACCESS_LSBYTE:
			Pit.counter[Channel].countLatched = 0;
			return Pit.counter[Channel].outlatch & 0xff;
		case PIT_ACCESS_MSBYTE:
			Pit.counter[Channel].countLatched = 0;
			return (Pit.counter[Channel].outlatch >> 8) & 0xff;
		case PIT_ACCESS_LS_MS_BYTE:
			Pit.counter[Channel].readState = PIT_ACCESS_MS_LS_BYTE;
			return Pit.counter[Channel].count & 0xff;
		case PIT_ACCESS_MS_LS_BYTE:
			Pit.counter[Channel].readState = PIT_ACCESS_LS_MS_BYTE;
			return (Pit.counter[Channel].count >> 8) & 0xff;
		}

	}
	else {
		switch (Pit.counter[Channel].readState) {
		default:
		case PIT_ACCESS_LSBYTE:
			return Pit.counter[Channel].count & 0xff;
		case PIT_ACCESS_MSBYTE:
			return (Pit.counter[Channel].count >> 8) & 0xff;
		case PIT_ACCESS_LS_MS_BYTE:
			Pit.counter[Channel].readState = PIT_ACCESS_MS_LS_BYTE;
			return Pit.counter[Channel].count & 0xff;
		case PIT_ACCESS_MS_LS_BYTE:
			Pit.counter[Channel].readState = PIT_ACCESS_LS_MS_BYTE;
			return (Pit.counter[Channel].count >> 8) & 0xff;
		}
	}
}

VOID PitLatchCount(struct Counter* Counter)
{
	if (Counter->countLatched) {
		Counter->outlatch = Counter->count;
		Counter->countLatched = Counter->access;
	}
}

VOID PitLoadCount(struct Counter* Counter, ULONG Value)
{
	if (!Value)
		Value = 0x10000;
	Counter->count = Value;
	PitSetOut(Counter, Counter->outPin);
}

VOID PitWriteHandler(ULONG64 Address, ULONG Value, ULONG Length)
{
	BYTE Channel, Access;

	switch (Address) {
		case 0x40:
		case 0x41:
		case 0x42:
			Channel = (BYTE)Address - 0x40;
			switch (Pit.counter[Channel].writeState) {
				case PIT_ACCESS_LSBYTE:
					//PitLoadCount(&Pit.counter[Channel], Value);
					Pit.counter[Channel].inlatch = (BYTE)Value;
					break;
				case PIT_ACCESS_MSBYTE:
					//PitLoadCount(&Pit.counter[Channel], Value << 8);
					Pit.counter[Channel].inlatch = (USHORT)(Value << 8);
					break;
				case PIT_ACCESS_LS_MS_BYTE:
					Pit.counter[Channel].inlatch = (BYTE)Value;
					Pit.counter[Channel].writeState = PIT_ACCESS_MS_LS_BYTE;
					break;
				case PIT_ACCESS_MS_LS_BYTE:
					////PitLoadCount(&Pit.counter[Channel], Pit.counter[Channel].inlatch | Value << 8);
					Pit.counter[Channel].inlatch |= (USHORT)(Value << 8);
					Pit.counter[Channel].writeState = PIT_ACCESS_LS_MS_BYTE;
					break;
			}
			if (Pit.counter[Channel].writeState != PIT_ACCESS_MS_LS_BYTE) {
				Pit.counter[Channel].count = Pit.counter[Channel].inlatch ? Pit.counter[Channel].inlatch : 0xffff;
				//if bcd TODO
				Pit.counter[Channel].countBinary = Pit.counter[Channel].count;
			}
			switch (Pit.counter[Channel].mode) {
				case 0:
					break;
				case 2:
					Pit.counter[Channel].nextChangeTime = 1;
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
						if (Value & 0x20) {
							PitLatchCount(&Pit.counter[Channel]);
						}
						if (!(Value & 0x10) && !Pit.counter[Channel].statusLatched) {
							Pit.counter[Channel].statusLatch =
								((Pit.counter[Channel].outPin & 0x1) << 7) |
								((Pit.counter[Channel].nullCount & 0x1) << 6) |
								((Pit.counter[Channel].access & 0x3) << 4) |
								((Pit.counter[Channel].mode & 0x7) << 1) |
								(Pit.counter[Channel].bcdMode & 0x1);
							Pit.counter[Channel].statusLatched = 1;
						}
					}
				}
			}
			else {
				switch (Access) {
					case 0:
						PitLatchCount(&Pit.counter[Channel]);
						return;
					case PIT_ACCESS_LSBYTE:
						Pit.counter[Channel].readState = PIT_ACCESS_LSBYTE;
						Pit.counter[Channel].writeState = PIT_ACCESS_LSBYTE;
						break;
					case PIT_ACCESS_MSBYTE:
						Pit.counter[Channel].readState = PIT_ACCESS_MSBYTE;
						Pit.counter[Channel].writeState = PIT_ACCESS_MSBYTE;
						break;
					case PIT_ACCESS_LS_MS_BYTE:
						Pit.counter[Channel].readState = PIT_ACCESS_LS_MS_BYTE;
						Pit.counter[Channel].writeState = PIT_ACCESS_LS_MS_BYTE;
						break;
				}
				Pit.counter[Channel].mode = (Value >> 1) & 0x7;
				if (Pit.counter[Channel].mode)
					PitSetOut(&Pit.counter[Channel], 1);
				else 
					PitSetOut(&Pit.counter[Channel], 0);
				Pit.counter[Channel].access = Access;
				Pit.counter[Channel].nextChangeTime = 0;
			}
			break;
		case 0x61:
			break;
	}

}
