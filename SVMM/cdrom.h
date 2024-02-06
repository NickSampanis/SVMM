#pragma once
#include <Windows.h>

#define CDROM_FRAMESIZE 2048

typedef struct _CDROM {
	HANDLE hFile;
	ULONG64 Capacity;

} CDROM, *PCDROM;

VOID CdRomInitialize(CHAR* Pathname);
ULONG64 CdRomGetCapacity(VOID);
DWORD CdRomReadBlock(DWORD LbAddress, BYTE* Buffer, DWORD BufferSize);
DWORD CdRomReadToc(BYTE* Buffer, ULONG Length, BYTE Msf, LONG StartTrack, BYTE Format);
DWORD CdRomSeek(DWORD LbAddress);
