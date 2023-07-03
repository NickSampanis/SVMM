#include "kb8042.h"
#include "pci.h"
#include "pic.h"
#include "timer.h"
#include <stdio.h>

struct kbController kb8042;

static DWORD Kb8042Periodic()
{

}

static VOID Kb8042InsertQueue(BYTE Data, BYTE Source)
{
	if (kb8042.StatusRegister & KB_STATUS_OUT_BUFFER && kb8042.QueueSize < KB_CONTROLLER_QSIZE - 1) {
		kb8042.Queue[kb8042.QueueSize] = Data;
		kb8042.QueueSize++;
	}
	else if (!Source) {
		kb8042.KbOutputBuffer = Data;
		kb8042.StatusRegister |= KB_STATUS_OUT_BUFFER;
		kb8042.Irq1Requested = 1;
	}
	else {
		kb8042.MsOutputBuffer = Data;
		kb8042.StatusRegister |= (KB_STATUS_OUT_BUFFER | KB_STATUS_MOUSE_DATA);
		kb8042.Irq12Requested = 1;

	}
}

static VOID Kb8042TimerHandler()
{
	
	if (kb8042.Irq1Requested)
		PicRaiseIrq(2);
	else if (kb8042.Irq12Requested)
		PicRaiseIrq(12);
}

VOID Kb8042PortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length)
{
	switch (Address) {
		case 0x60:

			break;
		case 0x64:
			kb8042.CmdRegister = (BYTE)Value;
			kb8042.StatusRegister |= KB_STATUS_LAST_CMD;
			switch (Value) {
				case 0xaa:
					kb8042.CmdRegister |= KB_STATUS_SYSTEM_FLAG;
					Kb8042InsertQueue(0x55, 0);
					break;
				case 0xab:
					if (kb8042.StatusRegister & KB_STATUS_OUT_BUFFER) {
						fprintf(stderr, "Error in interface test\n");
						exit(-1);
					}
					Kb8042InsertQueue(0x00, 0);
					break;
				case 0xad:
					break;
				case 0xae:
					break;
			}
			break;
	}
}

ULONG Kb8042PortIoReadHandler(ULONG64 Address, ULONG Length)
{
	ULONG value;
	BYTE i;

	value = 0;
	switch (Address) {
		case 0x60:
			if (kb8042.StatusRegister & KB_STATUS_OUT_BUFFER) {
				value = kb8042.KbOutputBuffer;
				kb8042.Irq1Requested = 0;
				kb8042.StatusRegister &= ~(KB_STATUS_OUT_BUFFER | KB_STATUS_MOUSE_DATA);

				if (kb8042.QueueSize) {
					kb8042.MsOutputBuffer = kb8042.Queue[0];
					kb8042.StatusRegister |= (KB_STATUS_OUT_BUFFER | KB_STATUS_MOUSE_DATA);
					kb8042.Irq1Requested = 1;
					for (i = 0; i < kb8042.QueueSize - 1; i++) {
						kb8042.Queue[i] = kb8042.Queue[i + 1];
					}
					kb8042.QueueSize--;
				}
				PicLowerIrq(1);
			}
			else {
				return kb8042.KbOutputBuffer;
			}
			break;
		case 0x64:
			value = kb8042.StatusRegister;
			kb8042.StatusRegister &= ~KB_STATUS_TIMEOUT;
			break;
	}
	return value;
}

Kb8042Initialize(VOID)
{
	memset(&kb8042, '\0', sizeof(kb8042));
	RegisterPortIoHandler(0x60, (WritePortIoHandlerCallback)Kb8042PortIoWriteHandler, (ReadPortIoHandlerCallback)Kb8042PortIoReadHandler);
	RegisterPortIoHandler(0x64, (WritePortIoHandlerCallback)Kb8042PortIoWriteHandler, (ReadPortIoHandlerCallback)Kb8042PortIoReadHandler);

	TimerRegister(0x034fb5e3 * 2, Kb8042TimerHandler);
}