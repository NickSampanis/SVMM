#include "timer.h"

static DWORD Count;
TIMER Timers[32];


VOID TimerInitialize(VOID)
{
	Count = 0;
	memset(Timers, '\0', sizeof(Timers));
}

DWORD TimerRegister(DWORD Useconds, VOID(*TimerHandler)(VOID))
{
	Timers[Count].Handler = TimerHandler;
	Timers[Count].Useconds = Useconds;
	Timers[Count].TimeToFire = Useconds;
	
	return Count++;
}

VOID TimerTick(LONG Useconds)
{
	DWORD i;

	for (i = 0; i < Count; i++) {
		Timers[i].TimeToFire -= Useconds;
		if (Timers[i].TimeToFire <= 0) {
			Timers[i].Handler();
			Timers[i].TimeToFire = Timers[i].Useconds;
		}
	}
}
