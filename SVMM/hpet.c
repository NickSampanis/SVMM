#include <Windows.h>
#include "hpet.h"
#include "pci.h"
#include "timer.h"
#include "pit.h"
#include "cmos.h"
#include "pic.h"
#include "SVMM.h"
HPET Hpet;

static inline ULONG64 HpetGetTicks(VOID)
{
	return ((TimerGetClockNs() - Hpet.ReferenceTime) / 10) + Hpet.ReferenceValue;
}
static inline ULONG64 HpetTicksToNs(ULONG64 Ticks)
{
	return Ticks * 10;
}

VOID HpetUpdateIrq(HPET_TIMER_BLOCK* TimerBlock, BYTE RaiseIrq)
{
	BYTE IrqNumber;

	if (TimerBlock->ConfigCapabilities & HPET_CONFIG_LEGACY)
		IrqNumber = TimerBlock->Id ? RTC_IRQ : PIT_IRQ;

	else
		IrqNumber = (TimerBlock->ConfigCapabilities & HPET_TIMER_CONF_INT_ROUTE_MASK) >> HPET_TIMER_CONF_INT_ROUTE_SHIFT;

	if (RaiseIrq && Hpet.GlobalConfiguration & HPET_CONFIG_ENABLED) {
		/* Status should be set even if IRQ is disabled */
		if (TimerBlock->ConfigCapabilities & HPET_TIMER_CONF_EDGE_LEVEL)
			Hpet.InterruptStatus |= (1ULL << TimerBlock->Id);
		if (!(TimerBlock->ConfigCapabilities & HPET_TIMER_CONF_IRQ_ENABLE))
			return;

		if (TimerBlock->ConfigCapabilities & HPET_TIMER_CONF_FSB_ENABLE)
			*(ULONG*)SvmmGetHostAddress(TimerBlock->FrontSideBusInterruptAddress) = TimerBlock->FrontSideBusInterruptValue;
		else if (!(TimerBlock->ConfigCapabilities & HPET_TIMER_CONF_EDGE_LEVEL)) {
			PicLowerIrq(IrqNumber);
			PicRaiseIrq(IrqNumber);
		}
		else 
			PicRaiseIrq(IrqNumber);
	}
	else
		PicLowerIrq(IrqNumber);

}

VOID HpetUpdateTimer(HPET_TIMER_BLOCK* TimerBlock)
{
	ULONG64 Diff, CurrentTick;

	CurrentTick = HpetGetTicks();
	Diff = TimerBlock->Comparator - CurrentTick;
	TimerActivate(TimerBlock->TimerId, HpetTicksToNs(Diff), TimerBlock->ConfigCapabilities & HPET_TIMER_CONF_PERIODIC_ENABLED);
}

VOID HpetDisableTimer(HPET_TIMER_BLOCK* TimerBlock)
{
	TimerDeactivate(TimerBlock->TimerId);
	HpetUpdateIrq(TimerBlock, 0);
}

VOID HpetTimerHandler(VOID* Param)
{
	HPET_TIMER_BLOCK* TimerBlock;
	ULONG64 CurrentTick;

	TimerBlock = (HPET_TIMER_BLOCK*)Param;
	CurrentTick = HpetGetTicks();

	if ((TimerBlock->LastTimeChecked <= CurrentTick && TimerBlock->LastTimeChecked <= TimerBlock->Comparator && TimerBlock->Comparator <= CurrentTick) ||
		(TimerBlock->LastTimeChecked <= TimerBlock->Comparator || TimerBlock->Comparator <= CurrentTick)) {
		HpetUpdateIrq(TimerBlock, 1);
		if (TimerBlock->ConfigCapabilities & HPET_TIMER_CONF_PERIODIC_ENABLED && TimerBlock->TimerPeriod) {
			do {
				TimerBlock->Comparator += TimerBlock->TimerPeriod;
			} while ((TimerBlock->LastTimeChecked <= CurrentTick && TimerBlock->LastTimeChecked <= TimerBlock->Comparator && TimerBlock->Comparator <= CurrentTick));
		}
	}
	HpetUpdateTimer(TimerBlock);
	TimerBlock->LastTimeChecked = CurrentTick;


}
VOID
_HpetWriteMmioHandler(ULONG Address, ULONG Value)
{
	ULONG Index, TimerId, i;
	HPET_TIMER_BLOCK* TimerBlock;

	Index = Address & 0x3ff;
	if (Index < 0x100) {
		switch (Address & 0x3ff) {
			case HPET_ID:
				return;
			case HPET_PERIOD:
				return;
			case HPET_CONFIGURE:
				
				if (Value & HPET_CONFIG_ENABLED && !(Hpet.GlobalConfiguration & HPET_CONFIG_ENABLED)) {
					/* Enable Timers */
					Hpet.ReferenceTime = TimerGetClockNs();
					Hpet.ReferenceValue = Hpet.Counter;
					for (i = 0; i < 3; i++) {
						if (Hpet.Timer[i].ConfigCapabilities & HPET_TIMER_CONF_IRQ_ENABLE && (Hpet.InterruptStatus & (1ULL << i))) {
							HpetUpdateIrq(&Hpet.Timer[i], 1);
						}
						HpetUpdateTimer(&Hpet.Timer[i]);
					}

				}
				else if (Hpet.GlobalConfiguration & HPET_CONFIG_ENABLED && !(Value & HPET_CONFIG_ENABLED)) {
					/* Disable Timer */
					/* Halt main counter and disable interrupt generation. */
					Hpet.Counter = HpetGetTicks();
					for (i = 0; i < 3; i++) {
						HpetDisableTimer(&Hpet.Timer[i]);
					}

				}
				if (Value & HPET_CONFIG_LEGACY && !(Hpet.GlobalConfiguration & HPET_CONFIG_LEGACY)) {
					PitSetInterrupt(0);
					CmosSetInterrupt(0);
				}
				else if (Hpet.GlobalConfiguration & HPET_CONFIG_LEGACY && !(Value & HPET_CONFIG_LEGACY)) {
					PitSetInterrupt(1);
					CmosSetInterrupt(1);
				}
				Hpet.GlobalConfiguration = (Hpet.GlobalConfiguration & ~3ULL) | (Value & 3);
			case HPET_CONFIGURE + 4:
				return;
			case HPET_STATUS:
				for (i = 0; i < 3; i++) {
					if (Value & Hpet.InterruptStatus & (1ULL << i)) {
						HpetUpdateIrq(&Hpet.Timer[i], 0);
						Hpet.InterruptStatus &= ~(1ULL << i);
					}
				}
			case HPET_STATUS + 4:
				return;
			case HPET_COUNTER:

				if (!(Hpet.GlobalConfiguration & HPET_CONFIG_ENABLED)) {
					Hpet.Counter = (Hpet.Counter & 0xffffffff00000000ULL) | Value;
					for (i = 0; i < 3; i++) {
						Hpet.Timer[i].LastTimeChecked = Hpet.Counter;
					}
				}
			case HPET_COUNTER + 4:
				if (!(Hpet.GlobalConfiguration & HPET_CONFIG_ENABLED)) {
					Hpet.Counter = (Hpet.Counter & 0xffffffffULL) | ((ULONG64)Value << 32);
					for (i = 0; i < 3; i++) {
						Hpet.Timer[i].LastTimeChecked = Hpet.Counter;
					}
				}
				break;

		}
	}
	else {
		TimerId = (Index - 0x100) / 0x20;
		if (TimerId >= 3)
			return;
		TimerBlock = &Hpet.Timer[TimerId];
		switch (Index & 0x1f) {
			case HPET_TIMER_CONFIG:
				TimerBlock->ConfigCapabilities = (TimerBlock->ConfigCapabilities & 0xffffffff00000000ULL) | (Value & HPET_TIMER_CONF_CFG_WRITE_MASK);

				if (TimerBlock->ConfigCapabilities & HPET_TIMER_CONF_TIMER_32BIT) {
					TimerBlock->Comparator &= 0xffffffff;
					TimerBlock->TimerPeriod &= 0xffffffff;					
				}
				if (TimerBlock->ConfigCapabilities & HPET_TIMER_CONF_FSB_ENABLE || !(TimerBlock->ConfigCapabilities & HPET_TIMER_CONF_EDGE_LEVEL)) {
					Hpet.InterruptStatus &= ~(1ULL << TimerId);
				}
				if (TimerBlock->ConfigCapabilities & HPET_TIMER_CONF_IRQ_ENABLE && Hpet.GlobalConfiguration & HPET_CONFIG_ENABLED) {
					if (Hpet.InterruptStatus & (1ULL << TimerId)) {
						HpetUpdateIrq(TimerBlock, 1);
					}
					else {
						HpetUpdateIrq(TimerBlock, 0);
					}
				}
				if (Hpet.GlobalConfiguration & HPET_CONFIG_ENABLED) {
					HpetUpdateTimer(TimerBlock);
				}
				break;
			case HPET_TIMER_CONFIG_ROUTE:
				break;
			case HPET_TIMER_COMPARE:

				if (!(TimerBlock->ConfigCapabilities & HPET_TIMER_CONF_PERIODIC_ENABLED) || 
					(TimerBlock->ConfigCapabilities & HPET_TIMER_CONF_VALUE_SET)) {
					TimerBlock->Comparator = (TimerBlock->Comparator & 0xffffffff00000000ULL) | Value;
				}
				TimerBlock->TimerPeriod = (TimerBlock->TimerPeriod & 0xffffffff00000000ULL) | Value;
				TimerBlock->ConfigCapabilities &= ~HPET_TIMER_CONF_VALUE_SET;
				if (Hpet.GlobalConfiguration & HPET_CONFIG_ENABLED) 
					HpetUpdateTimer(TimerBlock);
				
				break;
			case HPET_TIMER_COMPARE + 4:
				if (TimerBlock->ConfigCapabilities & HPET_TIMER_CONF_TIMER_32BIT) 
					break;
				if (!(TimerBlock->ConfigCapabilities & HPET_TIMER_CONF_PERIODIC_ENABLED) || 
					(TimerBlock->ConfigCapabilities & HPET_TIMER_CONF_VALUE_SET)) {
					TimerBlock->Comparator = (TimerBlock->Comparator & 0xffffffffULL) | ((ULONG64)Value << 32);
				}
				TimerBlock->TimerPeriod = (TimerBlock->TimerPeriod & 0xffffffffULL) | ((ULONG64)Value << 32);
				TimerBlock->ConfigCapabilities &= ~HPET_TIMER_CONF_VALUE_SET;
				if (Hpet.GlobalConfiguration & HPET_CONFIG_ENABLED) 
					HpetUpdateTimer(TimerBlock);
				
				break;
			case HPET_TIMER_ROUTE:
				TimerBlock->FrontSideBusInterruptValue = Value;
			case HPET_TIMER_ROUTE + 4:
				TimerBlock->FrontSideBusInterruptAddress = Value;
		}
	}
}

VOID HpetWriteMmioHandler(ULONG Address, BYTE* Data, ULONG Length)
{
	ULONG64 Value;

	switch (Length) {
		case 4:
			Value = *(ULONG*)Data;
			_HpetWriteMmioHandler(Address, Value);
			break;
		case 8:
			Value = *(ULONG64*)Data;
			_HpetWriteMmioHandler(Address, Value);
			_HpetWriteMmioHandler(Address + 4, Value >> 32);
			break;
		default:
			return;
	}

}

ULONG _HpetReadMmioHandler(ULONG Address)
{
	ULONG Index, TimerId;
	HPET_TIMER_BLOCK* TimerBlock;

	Index = Address & 0x3ff;

	if (Index < 0x100) {
		switch (Address & 0x3ff) {
			case HPET_ID:
				return Hpet.GlobalCapabilities;
			case HPET_PERIOD:
				return Hpet.ClockPeriod;
			case HPET_CONFIGURE:
				return Hpet.GlobalConfiguration & 0xffffffff;
			case HPET_CONFIGURE + 4:
				return Hpet.GlobalConfiguration >> 32;
			case HPET_STATUS:
				return Hpet.InterruptStatus & 0xffffffff;
			case HPET_STATUS + 4:
				return Hpet.InterruptStatus >> 32;
			case HPET_COUNTER:
				if (Hpet.GlobalConfiguration & 1)
					return HpetGetTicks() & 0xffffffff;
				return Hpet.Counter & 0xffffffff;
			case HPET_COUNTER + 4:
				if (Hpet.GlobalConfiguration & 1)
					return HpetGetTicks() >> 32;
				return Hpet.Counter >> 32;

		}
	}
	else {
		TimerId = (Index - 0x100) / 0x20;
		if (TimerId >= 3)
			return;
		TimerBlock = &Hpet.Timer[TimerId];
		switch (Index & 0x1f) {
			case HPET_TIMER_CONFIG:
				return TimerBlock->ConfigCapabilities;
			case HPET_TIMER_CONFIG_ROUTE:
				return TimerBlock->InterruptRoutingCapability;
			case HPET_TIMER_COMPARE:
				return TimerBlock->Comparator & 0xffffffff;
			case HPET_TIMER_COMPARE + 4:
				return TimerBlock->Comparator >> 32;
			case HPET_TIMER_ROUTE:
				return TimerBlock->FrontSideBusInterruptValue;
			case HPET_TIMER_ROUTE + 4:
				return TimerBlock->FrontSideBusInterruptAddress;
		}
	}
	return 0;
}

VOID HpetReadMmioHandler(ULONG Address, BYTE* Data, ULONG Length)
{
	ULONG Value;

	Value = _HpetReadMmioHandler(Address);
	switch (Length) {
		case 1:
			*(BYTE*)Data = Value & 0xff;
			break;
		case 2:
			*(USHORT*)Data = Value & 0xffff;
			break;
		case 4:
			*(ULONG*)Data = Value;
			break;
	}
	
}

VOID HpetInitialize()
{
	int i;

	//3 timers
	Hpet.GlobalCapabilities = HPET_CAP_REVISION | (2ULL << 8) | HPET_CAP_MAIN_COUNTER_SIZE | HPET_CAP_LEGACY_IRQ_ROUTE | HPET_CAP_VENDOR_ID;
	Hpet.ClockPeriod = FS_PER_NS * 10;

	MmioRegisterHandler(HPET_MMIO, 0x1000, HpetWriteMmioHandler, HpetReadMmioHandler);

	for (i = 0; i < 3; i++) {
		Hpet.Timer[i].TimerId = TimerCreate(HpetTimerHandler, (VOID*)&Hpet.Timer[i]);
		Hpet.Timer[i].Id = i;
	}
}