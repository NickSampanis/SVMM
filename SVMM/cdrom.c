#include "cdrom.h"
#include <stdio.h>

CDROM CdRom;

VOID CdRomInitialize(CHAR* Pathname)
{
	HANDLE hFile;
	LARGE_INTEGER Size;

	memset(&CdRom, '\0', sizeof(CdRom));

	hFile = CreateFileA(Pathname, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Error in CreateFile 0x%x\n", GetLastError());
		exit(-1);
	}
	CdRom.hFile = hFile;

	if (!GetFileSizeEx(hFile, &Size)) {
		fprintf(stderr, "Error in GetFileSizeEx 0x%x\n", GetLastError());
		exit(-1);
	}

	CdRom.Capacity = Size.QuadPart / 2048;
}

ULONG64 CdRomGetCapacity(VOID)
{
	return CdRom.Capacity;
}

DWORD CdRomReadBlock(DWORD LbAddress, BYTE* Buffer, DWORD BufferSize)
{
	DWORD ret;

	ret = 0;
	if (BufferSize == 2352) {
		memset(Buffer, '\0', 2352);
		memset(Buffer + 1, 0xff, 10);
		Buffer[12] = ((LbAddress + 150) / 75) / 60;
		Buffer[13] = ((LbAddress + 150) / 75) % 60;
		Buffer[14] = ((LbAddress + 150) % 75);
		Buffer[15] = 0x01;
		Buffer += 0x10;
	}

	SetFilePointer(CdRom.hFile, LbAddress * CDROM_FRAMESIZE, NULL, SEEK_SET);
	ReadFile(CdRom.hFile, Buffer, CDROM_FRAMESIZE, &ret, NULL);

	return ret;
}

DWORD CdRomReadToc(BYTE* Buffer, ULONG Length, BYTE Msf, LONG StartTrack, BYTE Format)
{
    unsigned i;
    DWORD blocks;
    int len = 4;

    switch (Format) {
        case 0:
            // From atapi specs : start track can be 0-63, AA
            if (StartTrack > 1 && StartTrack != 0xaa)
                return 0;

            Buffer[2] = 1;
            Buffer[3] = 1;

            if (StartTrack <= 1) {
                Buffer[len++] = 0; // Reserved
                Buffer[len++] = 0x14; // ADR, control
                Buffer[len++] = 1; // Track number
                Buffer[len++] = 0; // Reserved

                // Start address
                if (Msf) {
                    Buffer[len++] = 0; // reserved
                    Buffer[len++] = 0; // minute
                    Buffer[len++] = 2; // second
                    Buffer[len++] = 0; // frame
                }
                else {
                    Buffer[len++] = 0;
                    Buffer[len++] = 0;
                    Buffer[len++] = 0;
                    Buffer[len++] = 0; // logical sector 0
                }
            }

            // Lead out track
            Buffer[len++] = 0; // Reserved
            Buffer[len++] = 0x16; // ADR, control
            Buffer[len++] = 0xaa; // Track number
            Buffer[len++] = 0; // Reserved

            blocks = CdRomGetCapacity();

            // Start address
            if (Msf) {
                Buffer[len++] = 0; // reserved
                Buffer[len++] = (((blocks + 150) / 75) / 60); // minute
                Buffer[len++] = (((blocks + 150) / 75) % 60); // second
                Buffer[len++] = ((blocks + 150) % 75); // frame;
            }
            else {
                Buffer[len++] = (blocks >> 24) & 0xff;
                Buffer[len++] = (blocks >> 16) & 0xff;
                Buffer[len++] = (blocks >> 8) & 0xff;
                Buffer[len++] = (blocks >> 0) & 0xff;
            }
            Buffer[0] = ((len - 2) >> 8) & 0xff;
            Buffer[1] = (len - 2) & 0xff;
            break;

        case 1:
            // multi session stuff - emulate a single session only
            Buffer[0] = 0;
            Buffer[1] = 0x0a;
            Buffer[2] = 1;
            Buffer[3] = 1;
            for (i = 0; i < 8; i++)
                Buffer[4 + i] = 0;
            len = 12;
            break;

        case 2:
            // raw toc - emulate a single session only (ported from qemu)
            Buffer[2] = 1;
            Buffer[3] = 1;

            for (i = 0; i < 4; i++) {
                Buffer[len++] = 1;
                Buffer[len++] = 0x14;
                Buffer[len++] = 0;
                if (i < 3) {
                    Buffer[len++] = 0xa0 + i;
                }
                else {
                    Buffer[len++] = 1;
                }
                Buffer[len++] = 0;
                Buffer[len++] = 0;
                Buffer[len++] = 0;
                if (i < 2) {
                    Buffer[len++] = 0;
                    Buffer[len++] = 1;
                    Buffer[len++] = 0;
                    Buffer[len++] = 0;
                }
                else if (i == 2) {
                    blocks = CdRomGetCapacity();
                    if (Msf) {
                        Buffer[len++] = 0; // reserved
                        Buffer[len++] = (((blocks + 150) / 75) / 60); // minute
                        Buffer[len++] = (((blocks + 150) / 75) % 60); // second
                        Buffer[len++] = ((blocks + 150) % 75); // frame;
                    }
                    else {
                        Buffer[len++] = (blocks >> 24) & 0xff;
                        Buffer[len++] = (blocks >> 16) & 0xff;
                        Buffer[len++] = (blocks >> 8) & 0xff;
                        Buffer[len++] = (blocks >> 0) & 0xff;
                    }
                }
                else {
                    Buffer[len++] = 0;
                    Buffer[len++] = 0;
                    Buffer[len++] = 0;
                    Buffer[len++] = 0;
                }
            }
            Buffer[0] = ((len - 2) >> 8) & 0xff;
            Buffer[1] = (len - 2) & 0xff;
            break;

        default:
            return 0;
    }

    return Length;
}
DWORD CdRomSeek(DWORD LbAddress)
{
    return SetFilePointer(CdRom.hFile, LbAddress * CDROM_FRAMESIZE, NULL, SEEK_SET);
}
