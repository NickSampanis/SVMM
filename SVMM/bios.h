#ifndef BIOS_H
#define BIOS_H
#include <Windows.h>

#define BX_BIOS_MESSAGE_SIZE 80

BYTE bios_message[BX_BIOS_MESSAGE_SIZE];
unsigned int bios_message_i;
BOOL bios_panic_flag;

BYTE vgabios_message[BX_BIOS_MESSAGE_SIZE];
unsigned int vgabios_message_i;
BOOL vgabios_panic_flag;

VOID BiosInitialize();
ULONG BiosReadHandler(ULONG64 Address, ULONG Length);
VOID BiosWriteHandler(ULONG64 Address, ULONG Value, ULONG Length);


#endif