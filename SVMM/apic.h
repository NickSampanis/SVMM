#pragma once
#include <Windows.h>

#define APIC_DEFAULT_MMIO			  0xfee00000

#define LAPIC_ID                   0x020
#define LAPIC_VERSION              0x030
#define LAPIC_TPR                  0x080
#define LAPIC_ARBITRATION_PRIORITY 0x090
#define LAPIC_PPR                  0x0A0
#define LAPIC_EOI                  0x0B0
#define LAPIC_RRD                  0x0C0
#define LAPIC_LDR                  0x0D0
#define LAPIC_DESTINATION_FORMAT   0x0E0
#define LAPIC_SPURIOUS_VECTOR      0x0F0
#define LAPIC_ISR1                 0x100
#define LAPIC_ISR2                 0x110
#define LAPIC_ISR3                 0x120
#define LAPIC_ISR4                 0x130
#define LAPIC_ISR5                 0x140
#define LAPIC_ISR6                 0x150
#define LAPIC_ISR7                 0x160
#define LAPIC_ISR8                 0x170
#define LAPIC_TMR1                 0x180
#define LAPIC_TMR2                 0x190
#define LAPIC_TMR3                 0x1A0
#define LAPIC_TMR4                 0x1B0
#define LAPIC_TMR5                 0x1C0
#define LAPIC_TMR6                 0x1D0
#define LAPIC_TMR7                 0x1E0
#define LAPIC_TMR8                 0x1F0
#define LAPIC_IRR1                 0x200
#define LAPIC_IRR2                 0x210
#define LAPIC_IRR3                 0x220
#define LAPIC_IRR4                 0x230
#define LAPIC_IRR5                 0x240
#define LAPIC_IRR6                 0x250
#define LAPIC_IRR7                 0x260
#define LAPIC_IRR8                 0x270
#define LAPIC_ESR                  0x280
#define LAPIC_LVT_CMCI             0x2F0
#define LAPIC_ICR_LO               0x300
#define LAPIC_ICR_HI               0x310
#define LAPIC_LVT_TIMER            0x320
#define LAPIC_LVT_THERMAL          0x330
#define LAPIC_LVT_PERFMON          0x340
#define LAPIC_LVT_LINT0            0x350
#define LAPIC_LVT_LINT1            0x360
#define LAPIC_LVT_ERROR            0x370
#define LAPIC_TIMER_INITIAL_COUNT  0x380
#define LAPIC_TIMER_CURRENT_COUNT  0x390
#define LAPIC_TIMER_DIVIDE_CFG     0x3E0
#define LAPIC_SELF_IPI             0x3F0

#define LAPIC_INTERRUPT_TYPE	   (7 << 8)

#define INTERRUPT_TYPE_FIXED		0
#define INTERRUPT_TYPE_LOW_PRI	   (1 << 8)
#define INTERRUPT_TYPE_SMI		   (1 << 9)
#define INTERRUPT_TYPE_NMI		   (1 << 10)
#define INTERRUPT_TYPE_INIT		   (5 << 8)
#define INTERRUPT_TYPE_SIPI		   (6 << 8)
#define INTERRUPT_TYPE_EXT		   (7 << 8)

#define DESTINATION_TYPE_GROUP	   (1 << 9)
#define DESTINATION_TYPE_LAPIC	   (0)

#define LAPIC_DESTINATION		(0xff << 24)
#define LAPIC_MASKED			(1 << 16)
#define LAPIC_RAISED			(1 << 14)
#define LAPIC_DESTINATION_MODE	(1 << 11)

#define APIC_ERR_ILLEGAL_ADDR    0x80
#define APIC_ERR_RX_ILLEGAL_VEC  0x40
#define APIC_ERR_TX_ILLEGAL_VEC  0x20
#define X2APIC_ERR_REDIRECTIBLE_IPI 0x08
#define APIC_ERR_RX_ACCEPT_ERR   0x08
#define APIC_ERR_TX_ACCEPT_ERR   0x04
#define APIC_ERR_RX_CHECKSUM     0x02
#define APIC_ERR_TX_CHECKSUM     0x01

#define LAPIC_ID_BROADCAST		0xff

#define NUM_LOCAL_APICS			1



#define LAPIC_DM_INIT		(5 << 8)
#define LAPIC_DM_SIPI		(6 << 8)
#define LAPIC_DM_EXTINT		(7 << 8)

#define LAPIC_TRIGGER_MODE			(1 << 15)

#define LAPIC_DSH					(3 << 18)
#define LAPIC_DSH_APICID			(0 << 18)
#define LAPIC_DSH_SELF   			(1 << 18)
#define LAPIC_DSH_ALL			  	(1 << 19)
#define LAPIC_DSH_ALL_NO_SELF    	(3 << 18)

enum {
	APIC_LVT_TIMER = 0,
	APIC_LVT_THERMAL = 1,
	APIC_LVT_PERFMON = 2,
	APIC_LVT_LINT0 = 3,
	APIC_LVT_LINT1 = 4,
	APIC_LVT_ERROR = 5,
	APIC_LVT_CMCI = 6,
	APIC_LVT_ENTRIES
};


typedef struct _LAPIC {
	DWORD ApicId;
	DWORD ApicVersion;
	DWORD TaskPriority;
	DWORD EndOfInterrupt;
	DWORD InterruptServiceRegister[8];
	DWORD TriggerModeRegister[8];
	DWORD InterruptRequestRegister[8];
	DWORD InterruptEnableRegister[8];
	DWORD DestinationLogical;
	DWORD DestinationFormat;
	DWORD InterruptCommandRegisterLow;
	DWORD InterruptCommandRegisterHigh;
	DWORD LocalVectorTable[APIC_LVT_ENTRIES];
	DWORD TimerDivideFactor;
	DWORD ErrorStatus;
	DWORD ShadowErrorStatus;
	BYTE SpuriousVector;
	BYTE SoftwareEnable;
	BYTE FocusDisable;
	DWORD InitialCount;
} LAPIC, *PLAPIC;

VOID ApicInitialize(VOID);
BYTE ApicAcknowledgeInterrupt(VOID);
LONG ApicDeliverInterrupt(BYTE Vector, BYTE InterruptType, BYTE LevelSensitive);

/* APIC BUS API */
LONG ApicBusDeliverInterrupt(BYTE Vector, DWORD Destination, DWORD DestinationType, DWORD InterruptType, DWORD Raised, DWORD LevelSensitive);
LONG ApicBusBroadcastInterrupt(BYTE Vector, BYTE InterruptType, BYTE LevelSensitive, DWORD ExcludeCpus);

VOID ApicMMIOWriteHandler(ULONG Address, BYTE* Data, ULONG Length);
VOID ApicMMIOReadHandler(ULONG Address, BYTE* Data, ULONG Length);
