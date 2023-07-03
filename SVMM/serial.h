#ifndef SERIAL_H
#define SERIAL_H
#include <Windows.h>

typedef struct _SERIAL {
	BYTE	Data;
	BYTE	Status;
	BYTE	Control;
} SERIAL;

VOID SerialInitialize();
ULONG SerialPortIoReadHandler(ULONG64 Address, ULONG Length);
VOID SerialPortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length);

#endif