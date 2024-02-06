#include "serial.h"
#include "pci.h"

VOID SerialInitialize()
{
	ULONG i;
	//0x03f8, 0x02f8, 0x03e8, 0x02e8
	for (i = 0; i < 8; i++) {
		RegisterPortIoHandler(0x03f8 + i, (WritePortIoHandlerCallback)SerialPortIoWriteHandler, (ReadPortIoHandlerCallback)SerialPortIoReadHandler);
		RegisterPortIoHandler(0x02f8 + i, (WritePortIoHandlerCallback)SerialPortIoWriteHandler, (ReadPortIoHandlerCallback)SerialPortIoReadHandler);
		RegisterPortIoHandler(0x03e8 + i, (WritePortIoHandlerCallback)SerialPortIoWriteHandler, (ReadPortIoHandlerCallback)SerialPortIoReadHandler);
		RegisterPortIoHandler(0x02e8 + i, (WritePortIoHandlerCallback)SerialPortIoWriteHandler, (ReadPortIoHandlerCallback)SerialPortIoReadHandler);
	}
}

ULONG SerialPortIoReadHandler(ULONG64 Address, ULONG Length)
{
	return 2;
}

VOID SerialPortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length)
{

}
