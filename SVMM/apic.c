#include "apic.h"
#include "pci.h"
#include "SVMM.h"
#include "gvm.h"
#include "haxm.h"
#include <stdio.h>

#ifdef HAXM
extern HANDLE hHaxCpu;
extern volatile struct hax_tunnel* tunnel;
#elif GVM
extern HANDLE hGvmCpu;
extern struct kvm_run* kvm_run;
#endif

extern DWORD svmm_events;
extern struct Registers Registers;
extern BYTE RequestInterruptWindow;

LAPIC Lapic;

unsigned lzcntd(DWORD val_32)
{
    DWORD mask = 0x80000000;
    unsigned count = 0;

    while ((val_32 & mask) == 0 && mask) {
        mask >>= 1;
        count++;
    }

    return count;
}

LONG HighestPriorityInterrupt(LONG InterruptRegisters[])
{
    LONG i, tmp;

    for (i = 7; i >= 0; i--) {
        tmp = InterruptRegisters[i];
        tmp &= Lapic.InterruptEnableRegister[i];

        if (tmp)
            return i * 32 + (31 - lzcntd(tmp));

    }
}

VOID ApicService(VOID)
{
    LONG highestIsr, highestIrr;

    /* if there are no high prio interrupts, return */
    highestIrr = HighestPriorityInterrupt(Lapic.InterruptRequestRegister);
    if (highestIrr < 0)
        return;
    /* */
    highestIsr = HighestPriorityInterrupt(Lapic.InterruptServiceRegister);
    /* if an higher prio interrupt is in service, return */
    if (highestIsr >= 0 && highestIrr <= highestIsr)
        return;

    if (svmm_events & EVENT_PENDING_LAPIC_INTR)
        return;
    /*
#ifdef HAXM
    tunnel->request_interrupt_window = 1;
#elif GVM
    if (kvm_run->request_interrupt_window)
        return;
   
    kvm_run->request_interrupt_window = 1;
#endif
    */
    if (RequestInterruptWindow)
        return;

    RequestInterruptWindow = 1;
    svmm_events |= EVENT_PENDING_LAPIC_INTR;

}
VOID ApicBusBroadcastEndOfInterrupt(BYTE Vector)
{
    //does nothing
    //IoApicReceiveEndOfInterrupt(Vector);
}

VOID ApicReceiveEOI(ULONG Value)
{
    LONG vec;
    
    vec = HighestPriorityInterrupt(Lapic.InterruptServiceRegister);
    if (vec < 0)
        return;

    if (vec != Lapic.SpuriousVector) {
        Lapic.InterruptServiceRegister[vec / 32] &= ~(1 << (vec % 32));
        if (Lapic.TriggerModeRegister[vec / 32] & vec) {
            ApicBusBroadcastEndOfInterrupt(vec);
            Lapic.TriggerModeRegister[vec / 32] &= ~(1 << (vec % 32));
        }
        ApicService();
    }
}

void ApicWriteSpuriousInterrupt(ULONG value)
{
    unsigned i;

    // bits 0-3 of the spurious vector hardwired to '1
    Lapic.SpuriousVector = (value & 0xf0) | 0xf;
    Lapic.SoftwareEnable = (value >> 8) & 1;
    Lapic.FocusDisable = (value >> 9) & 1;

    if (!Lapic.SoftwareEnable) {
        for (i = 0; i < APIC_LVT_ENTRIES; i++) {
            Lapic.LocalVectorTable[i] |= 0x10000;	// all LVT are masked
        }
    }
}

void ApicSetDivideConfiguration(DWORD value)
{
    // move bit 3 down to bit 0
    value = ((value & 8) >> 1) | (value & 3);
    Lapic.TimerDivideFactor = (value == 7) ? 1 : (2 << value);
}

VOID ApicSetLvtEntry(DWORD ApicRegister, DWORD Value)
{
    static DWORD lvt_mask[] = {
    0x000710ff, /* TIMER */
    0x000117ff, /* THERMAL */
    0x000117ff, /* PERFMON */
    0x0001f7ff, /* LINT0 */
    0x0001f7ff, /* LINT1 */
    0x000110ff, /* ERROR */
    0x000117ff, /* CMCI */
    };

    unsigned lvt_entry = (ApicRegister == LAPIC_LVT_CMCI) ? APIC_LVT_CMCI : (ApicRegister - LAPIC_LVT_TIMER) >> 4;
/*
#if BX_CPU_LEVEL >= 6
    if (apic_reg == BX_LAPIC_LVT_TIMER) {
        if (!cpu->is_cpu_extension_supported(BX_ISA_TSC_DEADLINE)) {
            value &= ~0x40000; // cannot enable TSC-Deadline when not supported
        }
        else {
            if ((value ^ lvt[lvt_entry]) & 0x40000) {
                // Transition between TSC-Deadline and other timer modes disarm the timer
                if (timer_active) {
                    bx_pc_system.deactivate_timer(timer_handle);
                    timer_active = 0;
                }
            }
        }
    }
#endif
*/
    Lapic.LocalVectorTable[lvt_entry] = Value & lvt_mask[lvt_entry];
    if (!Lapic.SoftwareEnable) {
        Lapic.LocalVectorTable[lvt_entry] |= 0x10000;
    }
}

LONG ApicBusDeliverLowPriorityInterrupt(BYTE Vector, DWORD Destination, DWORD Raised, BYTE LevelSensitive)
{
    int i;
    int lowest_priority_agent = -1 , lowest_priority = 0x100, priority;

    // search for if focus processor exists
    for (i = 0; i < NUM_LOCAL_APICS; i++) {
        if (!Lapic.FocusDisable && Lapic.InterruptRequestRegister) {
            ApicDeliverInterrupt(Vector, INTERRUPT_TYPE_LOW_PRI, LevelSensitive);
            return 0;
        }
    }
    /*
    // focus processor not found, looking for lowest priority agent
    for (i = 0; i < NUM_LOCAL_APICS; i++) {
        if (broadcast || BX_CPU_APIC(i)->match_logical_addr(dest)) {
            if (BX_CPU_APIC(i)->is_xapic())
                priority = BX_CPU_APIC(i)->get_tpr();
            else
                priority = BX_CPU_APIC(i)->get_apr();
            if (priority < lowest_priority) {
                lowest_priority = priority;
                lowest_priority_agent = i;
            }
        }
    }
    */
    if (lowest_priority_agent >= 0)
    {
        ApicDeliverInterrupt(Vector, INTERRUPT_TYPE_LOW_PRI, LevelSensitive);
        return 0;
    }

    return 1;
}

VOID ApicSetIrq(BYTE Vector, BYTE LevelSensitive)
{
    /* set IRR */
    Lapic.InterruptRequestRegister[Vector / 32] |= 1 << (Vector % 32);

    /* set TMR */
    if (LevelSensitive)
        Lapic.TriggerModeRegister[Vector / 32] |= 1 << (Vector % 32);
    else
        Lapic.TriggerModeRegister[Vector / 32] &= ~(1 << (Vector % 32));

    ApicService();
}



LONG ApicBusBroadcastInterrupt(BYTE Vector, DWORD InterruptType, BYTE LevelSensitive, DWORD ExcludeCpus) 
{
    if (InterruptType == INTERRUPT_TYPE_LOW_PRI) {
        return ApicBusDeliverLowPriorityInterrupt(Vector, 0, 1, LevelSensitive);
    }

    // deliver to all bus agents except 'exclude_cpu'
    for (int i = 0; i < NUM_LOCAL_APICS; i++) {
        //TODO make it & instead of ==
        if (i == ExcludeCpus)
            continue;
        ApicDeliverInterrupt(Vector, InterruptType, LevelSensitive);
    }

    return 0;
}

VOID ApicSendIpi(DWORD Destination, DWORD Value)
{
    LONG Status;

    Status = 0;
    if ((Value & LAPIC_INTERRUPT_TYPE) == LAPIC_DM_INIT) {
        ApicBusDeliverLowPriorityInterrupt(Value & 0xff, Destination, Value & LAPIC_RAISED, Value & LAPIC_TRIGGER_MODE);
    }

    switch (Value & LAPIC_DSH) {
        case LAPIC_DSH_APICID:
            Status = ApicBusDeliverInterrupt(Value & 0xff, Destination, Value & LAPIC_DESTINATION_MODE, Value & LAPIC_INTERRUPT_TYPE,
                Value & LAPIC_RAISED, Value & LAPIC_TRIGGER_MODE);
            break;
        case LAPIC_DSH_SELF:
            ApicSetIrq(Value & 0xff, Value & LAPIC_RAISED);
            break;
        case LAPIC_DSH_ALL_NO_SELF:
            Status = ApicBusBroadcastInterrupt(Value & 0xff, Value & LAPIC_INTERRUPT_TYPE, Value & LAPIC_TRIGGER_MODE, Lapic.ApicId);
            break;
        case LAPIC_DSH_ALL:
            Status = ApicBusBroadcastInterrupt(Value & 0xff, Value & LAPIC_INTERRUPT_TYPE, Value & LAPIC_TRIGGER_MODE, 0xf);
            break;
    }
    if (Status)
        Lapic.ShadowErrorStatus |= APIC_ERR_TX_ACCEPT_ERR;
}

void ApicSetInitialTimerCount(DWORD Value)
{
    /*
#if BX_CPU_LEVEL >= 6
    Bit32u timervec = lvt[APIC_LVT_TIMER];

    // in TSC-deadline mode writes to initial time count are ignored
    if (timervec & 0x40000) return;
#endif

    // If active before, deactivate the current timer before changing it.
    if (timer_active) {
        bx_pc_system.deactivate_timer(timer_handle);
        timer_active = 0;
    }

    timer_initial = value;
    timer_current = 0;

    if (timer_initial != 0)  // terminate the counting if timer_initial = 0
    {
        // This should trigger the counter to start.  If already started,
        // restart from the new start value.
        timer_current = timer_initial;
        timer_active = 1;
        ticksInitial = bx_pc_system.time_ticks(); // timer value when it started to count
        bx_pc_system.activate_timer_ticks(timer_handle,
            Bit64u(timer_initial) * Bit64u(timer_divide_factor), 0);
    }
    */
    printf("APIC timer count not implemented\n");
    exit(-1);
}


VOID ApicMMIOWriteHandler(ULONG Address, BYTE* Data, ULONG Length)
{
    ULONG Value;

	if (Length != 4 || Address & 0xf)
		return;
	
    Value = *(ULONG*)Data;

    switch (Address & 0xff0) {
        case LAPIC_TPR: // task priority
            Lapic.TaskPriority = Value & 0xff;
            break;
        case LAPIC_EOI: // EOI
            ApicReceiveEOI(Value);
            break;
        case LAPIC_LDR: // logical destination
            Lapic.DestinationLogical = (Value >> 24) & 0xf;
            break;
        case LAPIC_DESTINATION_FORMAT:
            Lapic.DestinationFormat = (Value >> 28) & 0xf;
            break;
        case LAPIC_SPURIOUS_VECTOR:
            ApicWriteSpuriousInterrupt(Value);
            break;
        case LAPIC_ESR:
            Lapic.ErrorStatus = Lapic.ShadowErrorStatus;
            Lapic.ShadowErrorStatus = 0;
            break;
        case LAPIC_ICR_LO: // interrupt command reg 0-31
            Lapic.InterruptCommandRegisterLow = Value & ~(1 << 12);  // force delivery status bit = 0(idle)
            ApicSendIpi((Lapic.InterruptCommandRegisterHigh >> 24) & 0xff, Lapic.InterruptCommandRegisterLow);
            break;
        case LAPIC_ICR_HI: // interrupt command reg 31-63
            Lapic.InterruptCommandRegisterHigh = Value & 0xff000000;
            break;
        case LAPIC_LVT_TIMER:   // LVT Timer Reg
        case LAPIC_LVT_THERMAL: // LVT Thermal Monitor
        case LAPIC_LVT_PERFMON: // LVT Performance Counter
        case LAPIC_LVT_LINT0:   // LVT LINT0 Reg
        case LAPIC_LVT_LINT1:   // LVT LINT1 Reg
        case LAPIC_LVT_ERROR:   // LVT Error Reg
        case LAPIC_LVT_CMCI:
            ApicSetLvtEntry(Address & 0xff0, Value);
            break;
        case LAPIC_TIMER_INITIAL_COUNT:
            ApicSetInitialTimerCount(Value);
            break;
        case LAPIC_TIMER_DIVIDE_CFG:
            // only bits 3, 1, and 0 are writable
            ApicSetDivideConfiguration(Value & 0xb);
            break;
            /* all read-only registers go here */
        case LAPIC_ID:      // local APIC id
        case LAPIC_VERSION: // local APIC version
        case LAPIC_ARBITRATION_PRIORITY:
        case LAPIC_RRD:
        case LAPIC_PPR:     // processor priority
        // ISRs not writable
        case LAPIC_ISR1: case LAPIC_ISR2:
        case LAPIC_ISR3: case LAPIC_ISR4:
        case LAPIC_ISR5: case LAPIC_ISR6:
        case LAPIC_ISR7: case LAPIC_ISR8:
            // TMRs not writable
        case LAPIC_TMR1: case LAPIC_TMR2:
        case LAPIC_TMR3: case LAPIC_TMR4:
        case LAPIC_TMR5: case LAPIC_TMR6:
        case LAPIC_TMR7: case LAPIC_TMR8:
            // IRRs not writable
        case LAPIC_IRR1: case LAPIC_IRR2:
        case LAPIC_IRR3: case LAPIC_IRR4:
        case LAPIC_IRR5: case LAPIC_IRR6:
        case LAPIC_IRR7: case LAPIC_IRR8:
            // current count for timer
        case LAPIC_TIMER_CURRENT_COUNT:
            // all read-only registers should fall into this line
            break;
        /*
        //xapic
        case BX_LAPIC_EXT_APIC_FEATURE:
            // all read-only registers should fall into this line
            BX_INFO(("warning: write to read-only APIC register 0x%x", apic_reg));
            break;
        case BX_LAPIC_EXT_APIC_CONTROL:
            // set extended xAPIC capabilities
            xapic_ext = value & (BX_XAPIC_EXT_SUPPORT_IER | BX_XAPIC_EXT_SUPPORT_SEOI);
            break;
        case BX_LAPIC_SPECIFIC_EOI:
            receive_SEOI(value & 0xff);
            break;
        case BX_LAPIC_IER1: case BX_LAPIC_IER2:
        case BX_LAPIC_IER3: case BX_LAPIC_IER4:
        case BX_LAPIC_IER5: case BX_LAPIC_IER6:
        case BX_LAPIC_IER7: case BX_LAPIC_IER8:
        {
            if ((xapic_ext & BX_XAPIC_EXT_SUPPORT_IER) == 0) {
                BX_ERROR(("IER writes are currently disabled reg %x", apic_reg));
                break;
            }

            int index = (apic_reg - BX_LAPIC_IER1) >> 4;
            ier[index] = value;
        }
        break;
        */

        default:
            Lapic.ShadowErrorStatus |= APIC_ERR_ILLEGAL_ADDR;
	}
}

BYTE ApicGetProcessPriority(void)
{
    LONG ppr;
    
    ppr = HighestPriorityInterrupt(Lapic.InterruptServiceRegister);

    if ((ppr < 0) || ((Lapic.TaskPriority & 0xF0) >= (ppr & 0xF0)))
        ppr = Lapic.TaskPriority;
    else
        ppr &= 0xF0;

    return ppr;
}

BYTE ApicGetArbitrationPriority(void)
{
    DWORD tpr, isrv, irrv, first_isr, first_irr;
    BYTE apr;

    tpr = (Lapic.TaskPriority >> 4) & 0xf;
    first_isr = HighestPriorityInterrupt(Lapic.InterruptServiceRegister);
    if (first_isr < 0) 
        first_isr = 0;
    first_irr = HighestPriorityInterrupt(Lapic.InterruptRequestRegister);
    if (first_irr < 0) 
        first_irr = 0;
    isrv = (first_isr >> 4) & 0xf;
    irrv = (first_irr >> 4) & 0xf;

    if ((tpr >= irrv) && (tpr > isrv)) {
        apr = Lapic.TaskPriority & 0xff;
    }
    else {
        apr = ((tpr & isrv) > irrv) ? (tpr & isrv) : irrv;
        apr <<= 4;
    }
    return apr;
}

ULONG _ApicMMIOReadHandler(ULONG Address)
{
    ULONG Value;

	switch (Address & 0xff0) {
        case LAPIC_ID:
            return Lapic.ApicId << 24;
        case LAPIC_VERSION:
            return Lapic.ApicVersion;
        case LAPIC_TPR: // task priority
            return Lapic.TaskPriority & 0xff;
        case LAPIC_ARBITRATION_PRIORITY:
            return ApicGetArbitrationPriority();
        case LAPIC_PPR:     // processor priority
            return ApicGetProcessPriority();
        case LAPIC_EOI: // EOI
            return 0;
        case LAPIC_LDR: // logical destination
            return (Lapic.DestinationLogical & 0xf) << 24;
        case LAPIC_DESTINATION_FORMAT:
            return (Lapic.DestinationFormat & 0xf) << 28;
        case LAPIC_SPURIOUS_VECTOR:
            if (Lapic.SoftwareEnable)
                return  Lapic.SpuriousVector | 0x100;
            else if (Lapic.FocusDisable)
                return  Lapic.SpuriousVector | 0x200;
            else
                return Lapic.SpuriousVector;
        case LAPIC_ESR:
            return Lapic.ErrorStatus;
        case LAPIC_ICR_LO: // interrupt command reg 0-31
            return Lapic.InterruptCommandRegisterLow;
        case LAPIC_ICR_HI: // interrupt command reg 31-63
            return Lapic.InterruptCommandRegisterHigh;
        case LAPIC_LVT_TIMER:   // LVT Timer Reg
        case LAPIC_LVT_THERMAL: // LVT Thermal Monitor
        case LAPIC_LVT_PERFMON: // LVT Performance Counter
        case LAPIC_LVT_LINT0:   // LVT LINT0 Reg
        case LAPIC_LVT_LINT1:   // LVT LINT1 Reg
        case LAPIC_LVT_ERROR:   // LVT Error Reg
            return Lapic.LocalVectorTable[((Address & 0xff0) - LAPIC_LVT_TIMER) >> 4];
        case LAPIC_LVT_CMCI:
            return Lapic.LocalVectorTable[APIC_LVT_CMCI];
        case LAPIC_TIMER_INITIAL_COUNT:
            return Lapic.InitialCount;
        case LAPIC_TIMER_DIVIDE_CFG:
            return Lapic.TimerDivideFactor;
        case LAPIC_ISR1: case LAPIC_ISR2:
        case LAPIC_ISR3: case LAPIC_ISR4:
        case LAPIC_ISR5: case LAPIC_ISR6:
        case LAPIC_ISR7: case LAPIC_ISR8:     
            return Lapic.InterruptServiceRegister[((Address & 0xff0) - LAPIC_ISR1) >> 4];
        case LAPIC_TMR1: case LAPIC_TMR2:
        case LAPIC_TMR3: case LAPIC_TMR4:
        case LAPIC_TMR5: case LAPIC_TMR6:
        case LAPIC_TMR7: case LAPIC_TMR8:
            return Lapic.TriggerModeRegister[((Address & 0xff0) - LAPIC_TMR1) >> 4];
        case LAPIC_IRR1: case LAPIC_IRR2:
        case LAPIC_IRR3: case LAPIC_IRR4:
        case LAPIC_IRR5: case LAPIC_IRR6:
        case LAPIC_IRR7: case LAPIC_IRR8:
            return Lapic.InterruptRequestRegister[((Address & 0xff0) - LAPIC_IRR1) >> 4];
        case LAPIC_TIMER_CURRENT_COUNT:
            return 0;
        /*
        //xapic
        case BX_LAPIC_EXT_APIC_FEATURE:
            // all read-only registers should fall into this line
            BX_INFO(("warning: write to read-only APIC register 0x%x", apic_reg));
            break;
        case BX_LAPIC_EXT_APIC_CONTROL:
            // set extended xAPIC capabilities
            xapic_ext = value & (BX_XAPIC_EXT_SUPPORT_IER | BX_XAPIC_EXT_SUPPORT_SEOI);
            break;
        case BX_LAPIC_SPECIFIC_EOI:
            receive_SEOI(value & 0xff);
            break;
        case BX_LAPIC_IER1: case BX_LAPIC_IER2:
        case BX_LAPIC_IER3: case BX_LAPIC_IER4:
        case BX_LAPIC_IER5: case BX_LAPIC_IER6:
        case BX_LAPIC_IER7: case BX_LAPIC_IER8:
        {
            if ((xapic_ext & BX_XAPIC_EXT_SUPPORT_IER) == 0) {
                BX_ERROR(("IER writes are currently disabled reg %x", apic_reg));
                break;
            }

            int index = (apic_reg - BX_LAPIC_IER1) >> 4;
            ier[index] = value;
        }
        break;
        */

        default:
            Lapic.ShadowErrorStatus |= APIC_ERR_ILLEGAL_ADDR;
	}
}

VOID ApicMMIOReadHandler(ULONG Address, BYTE* Data, ULONG Length)
{
    ULONG Value;

    if (Length != 4 || Address & 0xf)
        return;

    Value = _ApicMMIOReadHandler(Address);

    switch (Length) {
    case 1:
        *(BYTE*)Data = Value & 0xff;
        break;
    case 2:
        *(WORD*)Data = Value & 0xffff;
        break;
    case 4:
        *(DWORD*)Data = Value;
        break;
    }
    Value = *(ULONG*)Data;
}




BYTE ApicAcknowledgeInterrupt(VOID)
{
    LONG Vector;
    
    if (!(svmm_events & EVENT_PENDING_LAPIC_INTR))
        exit(-1);
    

    Vector = HighestPriorityInterrupt(Lapic.InterruptRequestRegister);

    if (Vector < 0 || (Vector & 0xf) < ApicGetProcessPriority()) {
        svmm_events &= ~EVENT_PENDING_LAPIC_INTR;
        return Lapic.SpuriousVector;
    }
    Lapic.InterruptRequestRegister[Vector / 32] &= ~(1 << (Vector % 32));
    Lapic.InterruptServiceRegister[Vector / 32] |= (1 << (Vector % 32));
    svmm_events &= ~EVENT_PENDING_LAPIC_INTR;
    ApicService();

    return Vector;
}

LONG ApicMatchLogicalAddress(DWORD Destination)
{


    if (Lapic.DestinationFormat == 0xf) {
        return (Destination & Lapic.DestinationLogical);
    }
    else if (Lapic.DestinationFormat == 0) {
        // cluster model
        if (Destination == 0xff) // broadcast all
            return 1;

        if ((Destination & 0xf0) == (Lapic.DestinationLogical & 0xf0))
            return (Destination & Lapic.DestinationLogical & 0x0f);
    }

    return 0;
}

/*
LONG ApicBusDeliverInterrupt(BYTE Vector, DWORD Destination, DWORD DestinationType, DWORD InterruptType, DWORD Raised, DWORD LevelSensitive)
{
    unsigned int i;

	if (InterruptType == INTERRUPT_TYPE_LOW_PRI && DestinationType == DESTINATION_TYPE_GROUP) {
		return ApicBusDeliverLowPriorityInterrupt(Vector, Destination, Raised, LevelSensitive);
	}
	else if (DestinationType == DESTINATION_TYPE_LAPIC) {
		// send it to a group of cpus in normal prio 
        if ((Destination & 0xf) == 0xf) {
            return ApicBusBroadcastInterrupt(Vector, InterruptType, LevelSensitive, Destination);
        }
        else {
            for (i = 0; i < NUM_LOCAL_APICS; i++) {
                if (Lapic.ApicId == Destination) {
                    return ApicDeliverInterrupt(Vector, InterruptType, LevelSensitive);
                }
            }
        }
        return -1;
	}
	else if (DestinationType == DESTINATION_TYPE_GROUP) {
		// Send it to a specific LAPIC or broadcast in normal prio 
        if (!Destination)
            return -1;
        for (i = 0; i < NUM_LOCAL_APICS; i++) {
            if (ApicMatchLogicalAddress(Destination))
                ApicDeliverInterrupt(Vector, InterruptType, LevelSensitive);
        }

	}
	return -1;
}
*/

LONG ApicBusDeliverInterrupt(BYTE Vector, DWORD Destination, DWORD DestinationType, DWORD InterruptType, DWORD Raised, DWORD LevelSensitive)
{
    switch (DestinationType) {
        case INTERRUPT_TYPE_FIXED:

            break;
        case INTERRUPT_TYPE_LOW_PRI:
            break;
        case INTERRUPT_TYPE_SMI:
            break;
        case INTERRUPT_TYPE_NMI:
            break;
        case INTERRUPT_TYPE_INIT:
            break;
        case INTERRUPT_TYPE_SIPI:
            break;
        case INTERRUPT_TYPE_EXT:
            break;
        default:
            exit(-1);
    }
}
VOID ApicReset(VOID)
{
    int i;

    for (i = 0; i < 8; i++)
        Lapic.InterruptEnableRegister[i] = -1;

    /* 4 lvt entries */
    Lapic.ApicVersion = 0x00030010;
    Lapic.SpuriousVector = 0xff;
}

LONG ApicDeliverInterrupt(BYTE Vector, DWORD InterruptType, BYTE LevelSensitive)
{
    BOOL Ret;
    DWORD Bytes;

	switch (InterruptType) {
        case INTERRUPT_TYPE_FIXED:
		case INTERRUPT_TYPE_LOW_PRI:
			ApicSetIrq(Vector, LevelSensitive);
			break;
		case INTERRUPT_TYPE_SMI:
            Ret = DeviceIoControl(hGvmCpu, GVM_SMI, NULL, 0, NULL, 0, &Bytes, NULL);
            if (!Ret) {
                fprintf(stderr, "GVM_SMI 0x%x", GetLastError());
                return;
    }
			break;
		case INTERRUPT_TYPE_NMI:
#ifdef HAXM

#elif GVM
            Ret = DeviceIoControl(hGvmCpu, GVM_NMI, NULL, 0, NULL, 0, &Bytes, NULL);
            if (!Ret) {
                fprintf(stderr, "GVM_NMI 0x%x", GetLastError());
                return;
            }
#endif

			break;
		case INTERRUPT_TYPE_INIT:
            ApicReset();
            SvmmInitializeRegisterState(&Registers);
            SvmmSetRegisters(&Registers);
            break;
        case INTERRUPT_TYPE_SIPI:
            //load_seg_reg(&BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS], vector * 0x100);
            Registers.context._rip = ((ULONG64)Vector * 100);
            break;
		case INTERRUPT_TYPE_EXT:
			ApicSetIrq(Vector, LevelSensitive);
			break;
	}

}



VOID ApicInitialize(VOID)
{
    BOOL ret;
    ULONG bytes;
    struct kvm_msrs* msrs;

	memset(&Lapic, '\0', sizeof(Lapic));
	RegisterMMIOHandler(APIC_DEFAULT_MMIO, ApicMMIOWriteHandler, ApicMMIOReadHandler);
	ApicReset();
    //Registers.apicbase = APIC_DEFAULT_MMIO;
    //SvmmSetRegisters(&Registers);
    
}