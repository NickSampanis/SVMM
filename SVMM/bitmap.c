#include "bitmap.h"

VOID
RtlInitializeBitMap(
    IN PRTL_BITMAP BitMapHeader,
    IN PULONG BitMapBuffer,
    IN ULONG SizeOfBitMap
)
{

    //  Initialize the BitMap header.
    BitMapHeader->SizeOfBitMap = SizeOfBitMap;
    BitMapHeader->Buffer = BitMapBuffer;

}

VOID
RtlClearBit(
    IN PRTL_BITMAP BitMapHeader,
    IN ULONG BitNumber
)
{
    BitTestAndReset((PLONG)BitMapHeader->Buffer, BitNumber);
}

VOID
RtlSetBit(
    IN PRTL_BITMAP BitMapHeader,
    IN ULONG BitNumber
)

{
    BitTestAndSet((PLONG)BitMapHeader->Buffer, BitNumber);
}

BOOLEAN
RtlTestBit(
    IN PRTL_BITMAP BitMapHeader,
    IN ULONG BitNumber
)

{
    return BitTest((PLONG)BitMapHeader->Buffer, BitNumber);
}