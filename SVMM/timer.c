#include "timer.h"
#include "pic.h"
#include "apic.h"
#include "SVMM.h"
#include "haxm.h"

static DWORD TimersIdx;
static ULONG64 TicksTotal;
TIMER Timers[32];
extern BYTE RequestInterruptWindow;
extern DWORD svmm_events;
extern struct Registers Registers;

//ULONG64 TimerGetSystemTime

VOID TimerInitialize(VOID)
{
	TimersIdx = 0;
	TicksTotal = 0;
	memset(Timers, '\0', sizeof(Timers));
}

TIMER TimerGet(DWORD Index)
{
	return Timers[Index];
}

ULONG TimerGetElapsed(DWORD Index)
{
	return Timers[Index].Nseconds - Timers[Index].TimeToFire;
}

VOID TimerActivate(DWORD Index, DWORD Nseconds, BYTE Period)
{
	Timers[Index].Active = 1;
	Timers[Index].TimeToFire = Nseconds;
	Timers[Index].Nseconds = Nseconds;
	Timers[Index].Period = Period;
}

VOID TimerDeactivate(DWORD Index)
{
	Timers[Index].Active = 0;
	Timers[TimersIdx].Nseconds = 0;
	Timers[TimersIdx].TimeToFire = 0;
}

DWORD TimerRegister(DWORD Nseconds, VOID(*TimerHandler)(VOID), VOID *Param)
{
	Timers[TimersIdx].Handler = TimerHandler;
	Timers[TimersIdx].Nseconds = Nseconds;
	Timers[TimersIdx].TimeToFire = Nseconds;
	Timers[TimersIdx].Param = Param;
	Timers[TimersIdx].Active = 1;
	Timers[TimersIdx].Period = 1;

	return TimersIdx++;
}

VOID TimerTick(LONG Ticks)
{
	DWORD i, vector;

	TicksTotal += Ticks;
	for (i = 0; i < TimersIdx; i++) {
		if (Timers[i].Active)
			Timers[i].TimeToFire -= Ticks;
		if (Timers[i].Active && Timers[i].TimeToFire <= 0) {
			Timers[i].TimeToFire = 0;
			Timers[i].Handler(Timers[i].Param);
			if (!Timers[i].Period)
				Timers[i].Active = 0;
			Timers[i].TimeToFire = Timers[i].Nseconds;
		}
	}
	if (RequestInterruptWindow && Registers.context._rflags & EFLAGS_IF) {
		RequestInterruptWindow = 0;
		if (svmm_events & EVENT_PENDING_INTR)
			vector = PicIAC();
		else if (svmm_events & EVENT_PENDING_LAPIC_INTR)
			vector = ApicAcknowledgeInterrupt();
		else
			return;
		SvmmInterrupt(vector);
	}
	
}

ULONG64 TimerGetTotalUseconds(VOID)
{
	return TicksTotal / 1000ULL;
}

ULONG64 TimerGetTotalTicks(VOID)
{
	return TicksTotal;
}
