#include "timer.h"
#include "pic.h"
#include "apic.h"
#include "SVMM.h"
#include "haxm.h"
#include <xmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>

#define CPU_FREQ	4000000

static DWORD TimersIdx;
static ULONG64 TicksTotal, NsecondsTotal;
static LARGE_INTEGER TimerFreq;
TIMER Timers[32];
extern BYTE RequestInterruptWindow;
extern DWORD svmm_events;
extern struct Registers Registers;

//ULONG64 TimerGetSystemTime



ULONG64 muldiv64(ULONG64 a, ULONG b, ULONG c)
{
	__m128i a128 = _mm_set_epi64x(0, a);
	__m128i b128 = _mm_set1_epi32(b);
	__m128i mulResult = _mm_mul_epu32(a128, b128);

	uint64_t lo = _mm_cvtsi128_si64(mulResult);
	uint32_t hi = _mm_extract_epi32(mulResult, 1);
	uint64_t result = ((uint64_t)hi << 32) | lo;

	return result / c;
}

VOID TimerInitialize(VOID)
{
	TimersIdx = 0;
	TicksTotal = 0;
	NsecondsTotal = 0;
	memset(Timers, '\0', sizeof(Timers));
	QueryPerformanceFrequency(&TimerFreq);
}

TIMER TimerGet(DWORD Index)
{
	return Timers[Index];
}

ULONG64 TimerGetClockNs(void)
{
	return muldiv64((ULONG64)TicksTotal, MSECOND_TO_NANOSECONDS, CPU_FREQ);

}


ULONG64 TimerGetElapsed(DWORD Index)
{
	return Timers[Index].Nseconds - Timers[Index].TimeToFire;
}

BYTE TimerGetState(DWORD Index)
{
	return Timers[Index].Active;
}

VOID TimerActivate(DWORD Index, ULONG64 Nseconds, BYTE OneShot)
{
	Timers[Index].Active = 1;
	Timers[Index].Nseconds =  Nseconds;
	Timers[Index].OneShot = OneShot;
}

VOID TimerDeactivate(DWORD Index)
{
	Timers[Index].Active = 0;
	Timers[TimersIdx].Nseconds = 0;
	Timers[TimersIdx].TimeToFire = 0;
}
DWORD TimerCreate(VOID(*TimerHandler)(VOID*), VOID* Param)
{
	Timers[TimersIdx].Handler = TimerHandler;
	Timers[TimersIdx].Nseconds = 0;
	Timers[TimersIdx].TimeToFire = 0;
	Timers[TimersIdx].Param = Param;
	Timers[TimersIdx].Active = 0;
	Timers[TimersIdx].OneShot = 0;

	return TimersIdx++;
}

DWORD TimerRegister(DWORD Nseconds, VOID(*TimerHandler)(VOID*), VOID *Param)
{
	Timers[TimersIdx].Handler = TimerHandler;
	Timers[TimersIdx].Nseconds = Nseconds;
	Timers[TimersIdx].TimeToFire = Nseconds;
	Timers[TimersIdx].Param = Param;
	Timers[TimersIdx].Active = 1;
	Timers[TimersIdx].OneShot = 0;

	return TimersIdx++;
}
/*
VOID TimerTick2()
{
	DWORD i, vector;
	ULONG64 Ticks;

	Ticks = TimerGetClockNs();
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
}
*/

VOID TimerTick(ULONG64 Ticks)
{
	DWORD i, vector;

	TicksTotal += Ticks;
	NsecondsTotal += TimerGetClockNs();
	for (i = 0; i < TimersIdx; i++) {
		if (Timers[i].Active)
			Timers[i].TimeToFire -= NsecondsTotal;
		if (Timers[i].Active && Timers[i].TimeToFire <= 0) {
			Timers[i].TimeToFire = 0;
			Timers[i].Handler(Timers[i].Param);
			if (Timers[i].OneShot)
				Timers[i].Active = 0;
			Timers[i].TimeToFire = Timers[i].Nseconds;
		}
	}
	/*
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
	*/
}

ULONG64 TimerGetTotalUseconds(VOID)
{
	return TicksTotal / 1000ULL;
}

ULONG64 TimerGetTotalTicks(VOID)
{
	return TicksTotal;
}
