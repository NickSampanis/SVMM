#ifndef TIMER_H
#define TIMER_H
#include <Windows.h>

typedef VOID(*TimerHandler)(VOID);

typedef struct _TIMER {
	TimerHandler Handler;
	ULONG Useconds;
	LONG TimeToFire;
}TIMER, *PTIMER;

VOID TimerInitialize(VOID);

DWORD TimerRegister(DWORD Useconds, VOID (*TimerHandler)(VOID));

VOID TimerTick(LONG Useconds);

#endif