#ifndef TIMER_H
#define TIMER_H
#include <Windows.h>

/*
#define  TICK						55555555ULL 

#define	 SECONDS_TO_NS(s)			(s * 18 * TICK)
#define	 USECONDS_TO_NS(s)			(s * 18 * 0xd903ULL)
*/
#define NANOSECONDS_PER_SECOND		1000000000ULL
#define NANOSECONDS_PER_MSECOND		1000000ULL
#define MSECOND_TO_NANOSECONDS		1000000ULL
#define MSECONDS_PER_SECOND			1000000ULL


#define  TICK_PERIOD				(4000000)
#define  SECOND_TO_TICKS      		(TICK_PERIOD * 0x10000)
#define  MSECOND_TO_TICKS			(TICK_PERIOD * 1)
#define	 SECONDS_TO_NS(s)			(s * 1000000000ULL)
#define  MSECONDS_TO_NS(s)			(s * 1000000ULL)
#define	 USECONDS_TO_NS(s)			(s * 1000ULL)

typedef VOID(*TimerHandler)(VOID);

typedef struct _TIMER {
	TimerHandler Handler;
	ULONG64 Nseconds;
	LONG64 TimeToFire;
	VOID* Param;
	BYTE Active;
	BYTE OneShot;
}TIMER, *PTIMER;

VOID TimerInitialize(VOID);
DWORD TimerCreate(VOID(*TimerHandler)(VOID), VOID* Param);
DWORD TimerRegister(DWORD Nseconds, VOID(*TimerHandler)(VOID), VOID* Param);
VOID TimerTick(ULONG64 Nseconds);
TIMER TimerGet(DWORD Index);
VOID TimerActivate(DWORD Index, ULONG64 Nseconds, BYTE OneShot);
VOID TimerDeactivate(DWORD Index);
ULONG64 TimerGetElapsed(DWORD Index);
ULONG64 TimerGetTotalTicks(VOID);
ULONG64 TimerGetTotalUseconds(VOID);
ULONG64 TimerGetClockNs(void);
ULONG64 muldiv64(ULONG64 a, ULONG b, ULONG c);


#endif