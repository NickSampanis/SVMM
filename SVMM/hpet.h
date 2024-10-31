#pragma once
#include <Windows.h>

#define HPET_MMIO							0xFED00000
#define HPET_CAP_REVISION					1ULL
#define HPET_CAP_TIMER_COUNT				(0x3ULL << 8)
#define HPET_CAP_MAIN_COUNTER_SIZE			(0x1ULL << 13)
#define HPET_CAP_LEGACY_IRQ_ROUTE			(0x1ULL << 15)
#define HPET_CAP_VENDOR_ID					(0x8086ULL << 16)

#define HPET_CONFIG_ENABLED					(1ULL << 0)
#define HPET_CONFIG_LEGACY					(1ULL << 1)


#define HPET_TIMER_CONF_RSV					(0UL << 0)
#define HPET_TIMER_CONF_EDGE_LEVEL			(1UL << 1)
#define HPET_TIMER_CONF_IRQ_ENABLE			(1UL << 2)
#define HPET_TIMER_CONF_PERIODIC_ENABLED	(1UL << 3)
#define HPET_TIMER_CONF_PERIODIC_CAP		(1UL << 4)
#define HPET_TIMER_CONF_COMPARE_WIDTH		(1UL << 5)
#define HPET_TIMER_CONF_VALUE_SET			(1UL << 6)
#define HPET_TIMER_CONF_TIMER_32BIT			(1UL << 8)
#define HPET_TIMER_CONF_INT_ROUTE_MASK		0x3e00
#define HPET_TIMER_CONF_FSB_ENABLE			0x4000
#define HPET_TIMER_CONF_CFG_WRITE_MASK		0x7f4e
#define HPET_TIMER_CONF_INT_ROUTE_SHIFT     9


#define HPET_ID         0x000
#define HPET_PERIOD     0x004
#define HPET_CONFIGURE  0x010
#define HPET_STATUS     0x020
#define HPET_COUNTER    0x0f0

#define HPET_TIMER_CONFIG		0x00
#define HPET_TIMER_CONFIG_ROUTE	0x04
#define HPET_TIMER_COMPARE      0x08
#define HPET_TIMER_ROUTE		0x010

#define FS_PER_NS							1000000ULL

typedef struct _HPET_TIMER_BLOCK {
	ULONG ConfigCapabilities;
	ULONG InterruptRoutingCapability;
	ULONG64 Comparator;
	ULONG FrontSideBusInterruptValue;
	ULONG FrontSideBusInterruptAddress;
	ULONG TimerId;
	ULONG64 LastTimeChecked;
	ULONG64 TimerPeriod;
	ULONG Id;
} HPET_TIMER_BLOCK;

typedef struct _HPET {
	ULONG GlobalCapabilities;
	ULONG ClockPeriod;
	ULONG64 GlobalConfiguration;
	ULONG64 InterruptStatus;
	ULONG64 Counter;
	ULONG64 ReferenceTime;
	ULONG64 ReferenceValue;
	HPET_TIMER_BLOCK Timer[32];
} HPET;

VOID HpetInitialize();