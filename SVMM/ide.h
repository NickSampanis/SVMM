#pragma once
#include <Windows.h>

#define IDE_COMMAND_START   (1 << 0)
#define IDE_COMMAND_WRITE   (1 << 3)

#define IDE_STATUS_ACTIVE       (1 << 0)
#define IDE_STATUS_ERROR        (1 << 1)
#define IDE_STATUS_INTERRUPT    (1 << 2)
#define IDE_STATUS_WRITE        (1 << 3)
#define IDE_STATUS_START        (1 << 4)
#define IDE_STATUS_DMA0_CAP     (1 << 5)
#define IDE_STATUS_DMA1_CAP     (1 << 6)
#define IDE_STATUS_SIMPLEX      (1 << 7)

#define IDE_PORT_MASK (1 << 1) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 9) | (1 << 5) | (1 << 11) | (1 << 13) | (1 << 14) | (1 << 15)

#define IDE_BUFFER_SIZE         0x20000

#pragma pack(push, 1)
typedef struct PRD_ENTRY {
    DWORD Address;
    DWORD Count;
} PRD_ENTRY;

#pragma pack(pop)

typedef struct BMDMA {
    DWORD Command;
    DWORD Status;
    DWORD PrdTable;
    DWORD PrdCurrent;
    BYTE* Buffer;
    DWORD BufferOffset;
} BMDMA;

struct ide {
    struct BMDMA bmdma[2];
};

VOID IdeInitialize();
