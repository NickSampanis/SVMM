#ifndef BITMAP_H
#define BITMAP_H
#include <Windows.h>

typedef struct _RTL_BITMAP {
    ULONG SizeOfBitMap;
    PULONG Buffer;
} RTL_BITMAP;

typedef RTL_BITMAP* PRTL_BITMAP;

VOID
RtlInitializeBitMap(
    IN PRTL_BITMAP BitMapHeader,
    IN PULONG BitMapBuffer,
    IN ULONG SizeOfBitMap
);

VOID
RtlClearBit(
    IN PRTL_BITMAP BitMapHeader,
    IN ULONG BitNumber
);

VOID
RtlSetBit(
    IN PRTL_BITMAP BitMapHeader,
    IN ULONG BitNumber
);

BOOLEAN
RtlTestBit(
    IN PRTL_BITMAP BitMapHeader,
    IN ULONG BitNumber
);

#endif