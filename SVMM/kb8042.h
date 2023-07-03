#ifndef KB8042_H
#define KB8042_H
#include <windows.h>

#define KB_STATUS_OUT_BUFFER	(1 << 0)
#define KB_STATUS_IN_BUFFER		(1 << 1)
#define KB_STATUS_SYSTEM_FLAG	(1 << 2)
#define KB_STATUS_LAST_CMD		(1 << 3)
#define KB_STATUS_MOUSE_DATA	(1 << 5)
#define KB_STATUS_TIMEOUT		(1 << 6)

#define KB_STATUS_KB_DATA		(1 << 6)

#define KB_CONTROLLER_QSIZE 5

struct kbController {
	BYTE StatusRegister;
	BYTE CmdRegister;
	BYTE Irq1Requested;
	BYTE Irq12Requested;
	BYTE KbOutputBuffer;
	BYTE MsOutputBuffer;
	BYTE Queue[KB_CONTROLLER_QSIZE];
	BYTE QueueSize;
};

#endif