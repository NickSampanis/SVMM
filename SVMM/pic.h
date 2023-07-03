#ifndef PIC_H
#define PIC_H
#include <windows.h>

#define ICW1_REQ_ICW4			(1 << 0)
#define ICW1_INIT				(1 << 4)

#define ICW2_OFFSET_MASK		((1 << 8) - 1) & ~((1 << 3) - 1)

#define ICW3_CASCADE_ENABLE		(1 << 2)
#define ICW3_SLAVE_ID			(1 << 1)


#define ICW4_INTEL_DEFAULT				(1 << 0)
#define ICW4_AUTO_EOI					(1 << 1)
#define ICW4_BUFFERED_MODE_SLAVE		(1 << 2)
#define ICW4_BUFFERED_MODE_MASTER		(1 << 3)
#define ICW4_SPECIAL_FULLY_NESTED		(1 << 4)
#define ICW4_MASK						((1 << 4) - 1)

#define OCW1_IMR_MASK			((1 << 8) - 1)

#define OCW2_SELECT_DEFAULT		(1 << 3)

#define OCW3_MASK				((1 << 7) - 1)
#define OCW3_SPECIAL_MASK_MODE	(1 << 6)
#define OCW3_ESPECIAL_MASK_MODE (1 << 5)
#define OCW3_SELECT_DEFAULT		(1 << 3)
#define OCW3_POLL_MODE			(1 << 2)
#define OCW3_READ_IRR			2
#define OCW3_READ_ISR			3

#define STATE_INITIALIZED_ICW1	1
#define STATE_OFFSET_SET_ICW2	2
#define STATE_SLAVE_SET_ICW3	3
#define STATE_ARCH_SET_ICW4		4

#define MODE_FULLY_NESTED				1
#define MODE_SPECIAL_FULLY_NESTED		(1 << 1)
#define MODE_AUTO_ROTATE_PRIORITY		(1 << 2)
#define MODE_SPECIAL_ROTATE_PRIORITY	(1 << 3)
#define MODE_SPECIAL_MASK				(1 << 4)
#define MODE_POLL						(1 << 5)
#define MODE_AUTO_EOI					(1 << 6)
#define MODE_NORMAL_EOI					(1 << 7)



typedef struct _PIC {
	BYTE InitCmd[4];
	BYTE OperationCmd[4];
	BYTE EdgeLevel;

	BYTE InterruptMask;
	BYTE InterruptInService;
	BYTE InterruptRequest;
	BYTE InterruptPin;
	BYTE InterruptNumber;
	BYTE DataBuffer[8];

	BYTE CurrentPriority;
	BYTE CurrentOffset;
	BYTE Raised;
	BYTE State;
	BYTE Mode;
} PIC, *PPIC;

ULONG PicPortIoReadHandler(ULONG64 Address, ULONG Length);
VOID PicPortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length);
VOID PicInitialize(VOID);
VOID PicRaiseIrq(BYTE IrqNumber);
VOID PicLowerIrq(BYTE IrqNumber);
VOID PicSetMode(BYTE Master, BYTE EdgeLevel);
VOID PicService(PIC* Pic);
BYTE PicIAC();
VOID PicInterrupt(DWORD Vector);

#endif