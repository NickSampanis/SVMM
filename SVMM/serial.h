#ifndef SERIAL_H
#define SERIAL_H
#include <Windows.h>

#define BX_SERIAL_MAXDEV   4

#define BX_PC_CLOCK_XTL   1843200.0

#define BX_SER_RXIDLE  0
#define BX_SER_RXPOLL  1
#define BX_SER_RXWAIT  2

#define BX_SER_THR  0
#define BX_SER_RBR  0
#define BX_SER_IER  1
#define BX_SER_IIR  2
#define BX_SER_FCR  2
#define BX_SER_LCR  3
#define BX_SER_MCR  4
#define BX_SER_LSR  5
#define BX_SER_MSR  6
#define BX_SER_SCR  7

#define BX_SER_MODE_NULL          0
#define BX_SER_MODE_FILE          1
#define BX_SER_MODE_TERM          2
#define BX_SER_MODE_RAW           3
#define BX_SER_MODE_MOUSE         4
#define BX_SER_MODE_SOCKET_CLIENT 5
#define BX_SER_MODE_SOCKET_SERVER 6
#define BX_SER_MODE_PIPE_CLIENT   7
#define BX_SER_MODE_PIPE_SERVER   8

typedef struct _SERIAL {
	BYTE	Data;
	BYTE	Status;
	BYTE	Control;
} SERIAL;

VOID SerialInitialize();
ULONG SerialPortIoReadHandler(ULONG64 Address, ULONG Length);
VOID SerialPortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length);

#endif