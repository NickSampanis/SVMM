#ifndef TIMER_H
#define TIMER_H
#include <Windows.h>

/*
#define  TICK						55555555ULL 

#define	 SECONDS_TO_NS(s)			(s * 18 * TICK)
#define	 USECONDS_TO_NS(s)			(s * 18 * 0xd903ULL)
*/
#define  TICK_PERIOD				(0x8)
#define  SECOND_TO_TICKS      		(TICK_PERIOD * 0x10000)
#define  MSECOND_TO_TICKS			(TICK_PERIOD * 1)
#define	 SECONDS_TO_NS(s)			(s * 1000000000ULL)
#define  MSECONDS_TO_NS(s)			(s * 1000000ULL)
#define	 USECONDS_TO_NS(s)			(s * 1000ULL)

typedef VOID(*TimerHandler)(VOID);

typedef struct _TIMER {
	TimerHandler Handler;
	ULONG Nseconds;
	LONG TimeToFire;
	VOID* Param;
	BYTE Active;
	BYTE Period;
}TIMER, *PTIMER;

VOID TimerInitialize(VOID);

DWORD TimerRegister(DWORD Nseconds, VOID(*TimerHandler)(VOID), VOID* Param);
VOID TimerTick(LONG Nseconds);
TIMER TimerGet(DWORD Index);
VOID TimerActivate(DWORD Index, DWORD Nseconds, BYTE Period);
VOID TimerDeactivate(DWORD Index);
ULONG TimerGetElapsed(DWORD Index);
ULONG64 TimerGetTotalTicks(VOID);
ULONG64 TimerGetTotalUseconds(VOID);

#endif