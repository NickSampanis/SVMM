#pragma once
#include <Windows.h>

#define MSR_IA32_APICBASE		   0x1b
#define APIC_DEFAULT_MMIO		   0xfee00000

#define APIC_MSR_SW_EN			   (1UL << 11)
#define APIC_MSR_BSP			   (1UL << 8)
#define LAPIC_VERSION_VALUE		   0x10

/* Register Offsets */
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

/* Delivery Mode 3bits */
#define DELIVERY_MODE_FIXED		   0
#define DELIVERY_MODE_LOW_PRI	   (1UL << 8)
#define DELIVERY_MODE_SMI		   (1UL << 9)
#define DELIVERY_MODE_NMI		   (1UL << 10)
#define DELIVERY_MODE_INIT		   (5UL << 8)
#define DELIVERY_MODE_SIPI		   (6UL << 8)
#define DELIVERY_MODE_EXT		   (7UL << 8)
#define DELIVERY_MODE_MASK		   (7UL << 8)

/* Destination Mode 1bit */
#define DESTINATION_MODE_PHYSICAL	0
#define DESTINATION_MODE_LOGICAL	(1UL << 11)
#define LAPIC_DESTINATION_MODE_MASK	(1UL << 11)

#define LAPIC_DESTINATION			(0xffUL << 24)
#define LAPIC_LVT_MASKED			(1UL << 16)
#define LAPIC_RAISED				(1UL << 14)

#define LAPIC_TIMER_PERIODIC        (1UL << 17)

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

#define LAPIC_TRIGGER_MODE_MASK		(1 << 15)

#define LAPIC_DSH_APICID			(0 << 18)
#define LAPIC_DSH_SELF   			(1 << 18)
#define LAPIC_DSH_ALL			  	(1 << 19)
#define LAPIC_DSH_ALL_NO_SELF    	(3 << 18)
#define LAPIC_DSH_MASK				(3 << 18)


/** ESR - Send checksum error for Pentium 6. */
# define LAPIC_ESR_SEND_CHKSUM_ERROR_P6      (1UL << 0)
/** ESR - Send accept error for Pentium 6. */
# define LAPIC_ESR_RECV_CHKSUM_ERROR_P6      (1UL << 1)
/** ESR - Send accept error for Pentium 6. */
# define LAPIC_ESR_SEND_ACCEPT_ERROR_P6      (1UL << 2)
/** ESR - Receive accept error for Pentium 6. */
# define LAPIC_ESR_RECV_ACCEPT_ERROR_P6      (1UL << 3)

/** ESR - Redirectable IPI. */
#define LAPIC_ESR_REDIRECTABLE_IPI           (1UL << 4)
/** ESR - Send accept error. */
#define LAPIC_ESR_SEND_ILLEGAL_VECTOR        (1UL << 5)
/** ESR - Send accept error. */
#define LAPIC_ESR_RECV_ILLEGAL_VECTOR        (1UL << 6)
/** ESR - Send accept error. */
#define LAPIC_ESR_ILLEGAL_REG_ADDRESS        (1UL << 7)
/** ESR - Valid write-only bits. */
#define LAPIC_ESR_WO_VALID                   UINT32_C(0x0)

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
	USHORT InterruptServiceRegister[16];
	USHORT TriggerModeRegister[16];
	USHORT InterruptRequestRegister[16];
	DWORD DestinationLogical;
	DWORD DestinationFormat;
	DWORD InterruptCommandRegisterLow;
	DWORD InterruptCommandRegisterHigh;
	DWORD LocalVectorTable[APIC_LVT_ENTRIES];
	DWORD TimerDivideConfigurationRegister;
	DWORD TimerInitialCountRegister;
	ULONG64 TimerInitialLoadTime;
	ULONG64 TimerNextExpire;
	DWORD ErrorStatus;
	DWORD ShadowErrorStatus;
	DWORD TimerId;
	BYTE SpuriousVector;
	BYTE SoftwareEnable;
	BYTE FocusDisable;
	BYTE MsrEnable;
} LAPIC, *PLAPIC;

VOID ApicInitialize(VOID);
BYTE ApicAcknowledgeInterrupt(VOID);

/* APIC BUS API */
LONG ApicBusDeliverInterrupt(BYTE Vector, DWORD Destination, DWORD DestinationMode, DWORD DeliveryMode, DWORD Raised, DWORD TriggerMode);

VOID ApicMMIOWriteHandler(ULONG Address, BYTE* Data, ULONG Length);
VOID ApicMMIOReadHandler(ULONG Address, BYTE* Data, ULONG Length);
VOID ApicSetMsr(ULONG64 Value);

