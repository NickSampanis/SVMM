#include "apic.h"
#include "pci.h"
#include "SVMM.h"
#include "gvm.h"
#include "haxm.h"
#include "ioapic.h"
#include "timer.h"
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

LAPIC Lapic[1];

static BYTE ApicHighestVectorBit(USHORT Register)
{
    BYTE i;

    for (i = 15; i >= 1; i--)
        if (Register & (1UL << i))
            return i;
    return i;
}


static BYTE ApicHighestPriority(USHORT InterruptRegister[])
{
    BYTE i;

    for (i = 15; i >= 1; i--) {
        if (InterruptRegister[i] & 0xffff)
            return i;
    }

    return i;

}

/* Compares taskPriority(CR8) with ISR priority */
static BYTE ApicGetProcessPriority(DWORD ApicId)
{
    LONG ppr;

    ppr = ApicHighestPriority(Lapic[ApicId].InterruptServiceRegister);

    if (ppr < Lapic[ApicId].TaskPriority)
        return Lapic[ApicId].TaskPriority;

    return ppr;
}

static BYTE ApicGetArbitrationPriority(DWORD ApicId)
{
    BYTE ppr, irrPrio;


    ppr = ApicGetProcessPriority(ApicId);
    irrPrio = ApicHighestPriority(Lapic[ApicId].InterruptRequestRegister);
    if (irrPrio > ppr)
        return irrPrio;

    return ppr;
}

static LONG ApicMatchLogicalAddress(DWORD ApicId, DWORD Destination)
{
    if (Lapic[ApicId].DestinationFormat == 0xf) {
        return (Destination & Lapic[ApicId].DestinationLogical);
    }
    else if (Lapic[ApicId].DestinationFormat == 0) {
        // cluster model
        if (Destination == 0xff) // broadcast all
            return 1;

        if ((Destination & 0xf0) == (Lapic[ApicId].DestinationLogical & 0xf0))
            return (Destination & Lapic[ApicId].DestinationLogical & 0x0f);
    }

    return 0;
}

/* If the requested interrupt IRR > PPR (called APR), Request interrupt */
static VOID ApicService(DWORD ApicId)
{
    BYTE ppr, irrPrio;

    ppr = ApicGetProcessPriority(ApicId);
    irrPrio = ApicHighestPriority(Lapic[ApicId].InterruptRequestRegister);
    if (irrPrio > ppr) {
        if (svmm_events & EVENT_PENDING_LAPIC_INTR)
            return;
#ifdef HAXM
        tunnel->request_interrupt_window = 1;
#elif GVM
        if (kvm_run->request_interrupt_window)
            return;
        kvm_run->request_interrupt_window = 1;
#endif
        svmm_events |= EVENT_PENDING_LAPIC_INTR;
    }

}


//Clear ISR, TMR  request next INTR
VOID ApicReceiveEOI(ULONG Value)
{
    BYTE Vector, isrPrio;
    
    isrPrio = ApicHighestPriority(Lapic[0].InterruptServiceRegister);
    Vector = ApicHighestVectorBit(Lapic[0].InterruptServiceRegister[isrPrio]) + isrPrio * 16;

    if (Vector != Lapic[0].SpuriousVector) {
        Lapic[0].InterruptServiceRegister[isrPrio] &= ~(1UL << (Vector & 0xf));
        if (Lapic[0].TriggerModeRegister[isrPrio] & (1UL << (Vector & 0xf))) {
            IoApicReceiveEndOfInterrupt(Vector);
            Lapic[0].TriggerModeRegister[isrPrio] &= ~(1UL << (Vector & 0xf));
        }
        ApicService(0);
    }
}

void ApicWriteSpuriousInterrupt(ULONG value)
{
    unsigned i;

    // bits 0-3 of the spurious vector hardwired to '1
    Lapic[0].SpuriousVector = (value & 0xf0) | 0xf;
    Lapic[0].SoftwareEnable = (value >> 8) & 1;
    Lapic[0].FocusDisable = (value >> 9) & 1;

    if (!Lapic[0].SoftwareEnable) {
        for (i = 0; i < APIC_LVT_ENTRIES; i++) {
            Lapic[0].LocalVectorTable[i] |= LAPIC_LVT_MASKED;	// all LVT are masked
        }
    }
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

    Lapic[0].LocalVectorTable[lvt_entry] = Value & lvt_mask[lvt_entry];
    if (!Lapic[0].SoftwareEnable) 
        Lapic[0].LocalVectorTable[lvt_entry] |= LAPIC_LVT_MASKED;
    
}

//Sets IRR, Trigger regsters and if IRR > PPR request interrupt
VOID ApicSetIrq(DWORD ApicId, BYTE Vector, DWORD LevelSensitive)
{
    if (Vector <= 15) {
        //fprintf(stderr, "Error ApicSetIrq first 15 vectors are reserved!\n");
        Lapic[ApicId].ShadowErrorStatus |= LAPIC_ESR_RECV_ILLEGAL_VECTOR;
        return;
    }
    if (Lapic[ApicId].InterruptRequestRegister[Vector >> 4] & (1UL << (Vector & 0xf))) {
        //fprintf(stderr, "Error ApicSetIrq IRR already set !\n");
        return;
    }

    /* set IRR */
    Lapic[ApicId].InterruptRequestRegister[Vector >> 4] |= 1UL << (Vector & 0xf);

    /* set TMR */
    if (LevelSensitive)
        Lapic[ApicId].TriggerModeRegister[Vector >> 4] |= 1UL << (Vector & 0xf);
    else
        Lapic[ApicId].TriggerModeRegister[Vector >> 4] &= ~(1UL << (Vector & 0xf));

    ApicService(ApicId);
   
}

LONG ApicBusDeliverLowPriorityInterrupt(BYTE Vector, DWORD Destination, DWORD Raised, DWORD TriggerMode)
{
    DWORD lowestPrioTpr;
    INT lowestPrioApicId, i;

    // search for if focus processor exists
    for (i = 0; i < NUM_LOCAL_APICS; i++) {
        if (!Lapic[i].FocusDisable && Lapic[i].MsrEnable) {
            ApicSetIrq(i, Vector, TriggerMode);
            return 0;
        }
    }
    lowestPrioTpr = 0x10;
    lowestPrioApicId = -1;
    // focus processor not found, looking for lowest priority apic
    for (i = 0; i < NUM_LOCAL_APICS; i++) {
        if (Lapic[i].MsrEnable && ApicMatchLogicalAddress(i, Destination)) {
            if (Lapic[i].TaskPriority < lowestPrioTpr) {
                lowestPrioTpr = lowestPrioTpr;
                lowestPrioApicId = i;
            }
        }
    }
    
    if (lowestPrioApicId >= 0)
    {
        ApicSetIrq(lowestPrioApicId, Vector, TriggerMode);
        return 0;
    }

    return 1;
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






//remove pending interrupt from IRR, set it to ISR and request next INTR
BYTE ApicAcknowledgeInterrupt(VOID)
{
    BYTE irrPrio, Vector;

    if (!(svmm_events & EVENT_PENDING_LAPIC_INTR))
        exit(-1);
    

    irrPrio = ApicHighestPriority(Lapic[0].InterruptRequestRegister);
    //if irr prio less than the current, it was spurious
    if (irrPrio < ApicGetProcessPriority(0)) {
        //remove from irr??
        svmm_events &= ~EVENT_PENDING_LAPIC_INTR;
        return Lapic[0].SpuriousVector;
    }
    Vector = ApicHighestVectorBit(Lapic[0].InterruptRequestRegister[irrPrio]) + irrPrio * 16;
    Lapic[0].InterruptRequestRegister[Vector >> 4] &= ~(1UL << (Vector & 0xf));
    Lapic[0].InterruptServiceRegister[Vector >> 4] |= (1UL << (Vector & 0xf));
    svmm_events &= ~EVENT_PENDING_LAPIC_INTR;

    //request the next INTR
    ApicService(0);
    
    return Vector;
}




LONG ApicBusDeliverInterrupt(BYTE Vector, DWORD Destination, DWORD DestinationMode, DWORD DeliveryMode, DWORD Raised, DWORD TriggerMode)
{
    DWORD cpuId, Bytes;
    BOOL Ret;
    
    // Loweset priority for one APIC is just FIXED
    if (DestinationMode == DESTINATION_MODE_PHYSICAL
        && DeliveryMode == DELIVERY_MODE_LOW_PRI)
    {
        DeliveryMode = DELIVERY_MODE_FIXED;
    }
    
    
    switch (DeliveryMode) {
        case DELIVERY_MODE_FIXED:
            //one recepient
            if (DestinationMode == DESTINATION_MODE_PHYSICAL) {
                if (Destination < NUM_LOCAL_APICS && Lapic[Destination].MsrEnable)
                    ApicSetIrq(Destination, Vector, TriggerMode);
                return 0;
                
            }
            //DESTINATION_MODE_LOGICAL
            for (cpuId = 0; cpuId < NUM_LOCAL_APICS; cpuId++) {
                if (Lapic[cpuId].MsrEnable && ApicMatchLogicalAddress(cpuId, Destination)) 
                    ApicSetIrq(cpuId, Vector, TriggerMode);
                
            }
            return 0;
            break;
        case DELIVERY_MODE_LOW_PRI:
            return ApicBusDeliverLowPriorityInterrupt(Vector, Destination, Raised, TriggerMode);
            break;
        case DELIVERY_MODE_SMI:
#ifdef HAXM

#elif GVM
            Ret = DeviceIoControl(hGvmCpu, GVM_SMI, NULL, 0, NULL, 0, &Bytes, NULL);
            if (!Ret) {
                fprintf(stderr, "GVM_SMI 0x%x", GetLastError());
                return 1;
            }
#endif
            break;
        case DELIVERY_MODE_NMI:
#ifdef HAXM

#elif GVM
            Ret = DeviceIoControl(hGvmCpu, GVM_NMI, NULL, 0, NULL, 0, &Bytes, NULL);
            if (!Ret) {
                fprintf(stderr, "GVM_NMI 0x%x", GetLastError());
                return 1;
            }
#endif
            break;
        case DELIVERY_MODE_INIT:
            break;
        case DELIVERY_MODE_SIPI:
            break;
        case DELIVERY_MODE_EXT:
            break;
        default:
            exit(-1);
    }

    return 1;
}

static LONG ApicSendIpi(BYTE Vector, DWORD Destination, DWORD DestinationMode, DWORD DeliveryMode, DWORD Raised, DWORD TriggerMode, DWORD DestinationShorthand)
{
    LONG Status;
    BYTE cpuId;
    //LONG ApicBusDeliverInterrupt(BYTE Vector, DWORD Destination, DWORD DestinationMode, DWORD DeliveryMode, DWORD Raised, DWORD TriggerMode)

    Status = 0;
    if (DeliveryMode == DELIVERY_MODE_INIT)
        return ApicBusDeliverLowPriorityInterrupt(Vector, Destination, Raised, TriggerMode);


    switch (DestinationShorthand) {
        case LAPIC_DSH_APICID:
            Status = ApicBusDeliverInterrupt(Vector, Destination, DestinationMode, DeliveryMode, Raised, TriggerMode);
            break;
        case LAPIC_DSH_SELF:
            ApicSetIrq(Destination, Vector, TriggerMode);
            break;
        case LAPIC_DSH_ALL_NO_SELF:
            for (cpuId = 0; cpuId < NUM_LOCAL_APICS; cpuId++) {
                if (cpuId == Lapic[0].ApicId)
                    continue;
                Status = ApicBusDeliverInterrupt(Vector, Destination, DestinationMode, DeliveryMode, Raised, TriggerMode);
            }
            break;
        case LAPIC_DSH_ALL:
            for (cpuId = 0; cpuId < NUM_LOCAL_APICS; cpuId++) {
                Status = ApicBusDeliverInterrupt(Vector, Destination, DestinationMode, DeliveryMode, Raised, TriggerMode);
            }
            break;
    }
    if (Status)
        Lapic[0].ShadowErrorStatus |= APIC_ERR_TX_ACCEPT_ERR;
    return Status;
}

//LONG ApicBusDeliverInterrupt(BYTE Vector, DWORD Destination, DWORD DestinationMode, DWORD DeliveryMode, DWORD Raised, DWORD TriggerMode)

ULONG64 ApicGetNextTimerCount(ULONG64 CurrentTime)
{
    ULONG64 TimePassed;

    TimePassed = (CurrentTime - Lapic[0].TimerInitialLoadTime) >> Lapic[0].TimerDivideConfigurationRegister;
    if (Lapic[0].LocalVectorTable[APIC_LVT_TIMER] & LAPIC_TIMER_PERIODIC) {
        if (!Lapic[0].TimerInitialCountRegister)
            return 0;
        TimePassed = ((TimePassed / ((ULONG64)Lapic[0].TimerInitialCountRegister + 1)) + 1) *
            ((ULONG64)Lapic[0].TimerInitialCountRegister + 1);
    }
    else {
        if (TimePassed >= Lapic[0].TimerInitialCountRegister)
            return 0;

        TimePassed = (ULONG64)Lapic[0].TimerInitialCountRegister + 1;
    }

    return Lapic[0].TimerInitialLoadTime + (TimePassed << Lapic[0].TimerDivideConfigurationRegister);

}

VOID ApicTimerHandler(VOID* Param)
{
    ULONG apicId;

    apicId = (ULONG)Param;
    if (apicId >= NUM_LOCAL_APICS)
        return;
    if (Lapic[apicId].LocalVectorTable[APIC_LVT_TIMER] & LAPIC_LVT_MASKED)
        return;

    ApicBusDeliverInterrupt(
        Lapic[apicId].LocalVectorTable[APIC_LVT_TIMER] & 0xff,
        apicId,
        DESTINATION_MODE_PHYSICAL,
        Lapic[apicId].LocalVectorTable[APIC_LVT_TIMER] & DELIVERY_MODE_MASK,
        LAPIC_RAISED,
        Lapic[apicId].LocalVectorTable[APIC_LVT_TIMER] & LAPIC_TRIGGER_MODE_MASK);
    Lapic[0].TimerNextExpire = ApicGetNextTimerCount(Lapic[0].TimerInitialLoadTime);

    TimerActivate(Lapic[0].TimerId, Lapic[0].TimerNextExpire,
        !(Lapic[0].LocalVectorTable[APIC_LVT_TIMER] & LAPIC_TIMER_PERIODIC));
}


static VOID ApicReset(VOID)
{
    BOOL ret;
    ULONG bytes;
    struct kvm_msrs* msrs;
    int i;

    /* 4 lvt entries */
    Lapic[0].ApicVersion = ((APIC_LVT_ENTRIES - 1) << 16) | LAPIC_VERSION_VALUE;
    Lapic[0].SpuriousVector = 0xff;
    Lapic[0].MsrEnable = 0;
    Lapic[0].SoftwareEnable = 0;

    for (i = 0; i < NUM_LOCAL_APICS; i++) {
        Lapic[i].ApicId = i;
        Lapic[i].TimerId = TimerCreate(ApicTimerHandler, (VOID*)i);
    }



    msrs = (struct kvm_msrs*)calloc(1, sizeof(struct kvm_msrs) + sizeof(struct kvm_msr_entry) * 1);
    if (!msrs) {
        fprintf(stderr, "Error in ApicReset\n");
        exit(-1);
    }
    msrs->nmsrs = 1;
    msrs->entries[0].index = MSR_IA32_APICBASE;
    msrs->entries[0].data = APIC_DEFAULT_MMIO | APIC_MSR_BSP;


    ret = DeviceIoControl(hGvmCpu, GVM_SET_MSRS,
        msrs, sizeof(struct kvm_msrs) + sizeof(struct kvm_msr_entry) * msrs->nmsrs,
        msrs, sizeof(struct kvm_msrs) + sizeof(struct kvm_msr_entry) * msrs->nmsrs, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_SET_MSRS");
        exit(-1);
    }
    free(msrs);
}




VOID ApicSetMsr(ULONG64 Value)
{
    struct kvm_msrs* msrs;
    DWORD bytes;
    BOOL ret;
    
   
    if (Value & APIC_MSR_SW_EN)
        Lapic[0].MsrEnable = 1;
    else
        Lapic[0].MsrEnable = 0;
    msrs = (struct kvm_msrs*)calloc(1, sizeof(struct kvm_msrs) + sizeof(struct kvm_msr_entry) * 1);
    if (!msrs) {
        fprintf(stderr, "Error in ApicSetMsr\n");
        exit(-1);
    }
    msrs->nmsrs = 1;
    msrs->entries[0].index = MSR_IA32_APICBASE;
    msrs->entries[0].data = APIC_DEFAULT_MMIO | APIC_MSR_BSP | ((Value & APIC_MSR_SW_EN) ? APIC_MSR_SW_EN : 0);


    ret = DeviceIoControl(hGvmCpu, GVM_SET_MSRS,
        msrs, sizeof(struct kvm_msrs) + sizeof(struct kvm_msr_entry) * msrs->nmsrs,
        msrs, sizeof(struct kvm_msrs) + sizeof(struct kvm_msr_entry) * msrs->nmsrs, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_SET_MSRS");
        exit(-1);
    }
   
    free(msrs);
}

VOID ApicMMIOWriteHandler(ULONG Address, BYTE* Data, ULONG Length)
{
    ULONG Value;

    if (Length != 4 || Address & 0xf)
        return;

    Value = *(ULONG*)Data;

    switch (Address & 0xff0) {
        case LAPIC_TPR: // task priority
            if (Value > 15)
                return;

            if (Lapic[0].TaskPriority > Value) {
                Lapic[0].TaskPriority = Value & 0xff;
                ApicService(0);
            }
            else
                Lapic[0].TaskPriority = Value & 0xff;
            break;
        case LAPIC_EOI: // EOI
            ApicReceiveEOI(Value);
            break;
        case LAPIC_LDR: // logical destination
            Lapic[0].DestinationLogical = (Value >> 24) & 0xf;
            break;
        case LAPIC_DESTINATION_FORMAT:
            Lapic[0].DestinationFormat = (Value >> 28) & 0xf;
            break;
        case LAPIC_SPURIOUS_VECTOR:
            ApicWriteSpuriousInterrupt(Value);
            break;
        case LAPIC_ESR:
            Lapic[0].ErrorStatus = Lapic[0].ShadowErrorStatus;
            Lapic[0].ShadowErrorStatus = 0;
            break;
        case LAPIC_ICR_LO: // interrupt command reg 0-31
            Lapic[0].InterruptCommandRegisterLow = Value & ~(1UL << 12);  // force delivery status bit = 0(idle)
            ApicSendIpi(
                Lapic[0].InterruptCommandRegisterLow & 0xff,
                Lapic[0].InterruptCommandRegisterHigh >> 24,
                Lapic[0].InterruptCommandRegisterLow & LAPIC_DESTINATION_MODE_MASK, //1 << 11 dest mode
                Lapic[0].InterruptCommandRegisterLow & DELIVERY_MODE_MASK,    //7 << 8 interrupt type 
                Lapic[0].InterruptCommandRegisterLow & LAPIC_RAISED,
                Lapic[0].InterruptCommandRegisterLow & LAPIC_TRIGGER_MODE_MASK,
                Lapic[0].InterruptCommandRegisterLow & LAPIC_DSH_MASK);
            break;
        case LAPIC_ICR_HI: // interrupt command reg 31-63
            Lapic[0].InterruptCommandRegisterHigh = Value & 0xff000000;
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
            //ApicSetInitialTimerCount(Value);
            Lapic[0].TimerInitialCountRegister = Value;
            Lapic[0].TimerInitialLoadTime = TimerGetClockNs();
            Lapic[0].TimerNextExpire = ApicGetNextTimerCount(Lapic[0].TimerInitialLoadTime);
            
            TimerActivate(Lapic[0].TimerId, Lapic[0].TimerNextExpire, 
                !(Lapic[0].LocalVectorTable[APIC_LVT_TIMER] & LAPIC_TIMER_PERIODIC));
            break;
        case LAPIC_TIMER_DIVIDE_CFG:
            // only bits 3, 1, and 0 are writable
            // move bit 3 down to bit 0
            Value = ((Value & 8) >> 1) | (Value & 3);
            Lapic[0].TimerDivideConfigurationRegister = (Value == 7) ? 1 : (2UL << Value);
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
        default:
            Lapic[0].ShadowErrorStatus |= APIC_ERR_ILLEGAL_ADDR;
    }
}


ULONG _ApicMMIOReadHandler(ULONG Address)
{
    ULONG offset;
    switch (Address & 0xff0) {
        case LAPIC_ID:
            return Lapic[0].ApicId << 24;
        case LAPIC_VERSION:
            return Lapic[0].ApicVersion;
        case LAPIC_TPR: // task priority
            return (Lapic[0].TaskPriority << 4) & 0xff;
        case LAPIC_ARBITRATION_PRIORITY:
            return ApicGetArbitrationPriority(0);
        case LAPIC_PPR:     // processor priority
            return ApicGetProcessPriority(0);
        case LAPIC_EOI: // EOI
            return 0;
        case LAPIC_LDR: // logical destination
            return (Lapic[0].DestinationLogical & 0xf) << 24;
        case LAPIC_DESTINATION_FORMAT:
            return (Lapic[0].DestinationFormat & 0xf) << 28;
        case LAPIC_SPURIOUS_VECTOR:
            if (Lapic[0].SoftwareEnable)
                return  Lapic[0].SpuriousVector | 0x100;
            else if (Lapic[0].FocusDisable)
                return  Lapic[0].SpuriousVector | 0x200;
            else
                return Lapic[0].SpuriousVector;
        case LAPIC_ESR:
            return Lapic[0].ErrorStatus;
        case LAPIC_ICR_LO: // interrupt command reg 0-31
            return Lapic[0].InterruptCommandRegisterLow;
        case LAPIC_ICR_HI: // interrupt command reg 31-63
            return Lapic[0].InterruptCommandRegisterHigh;
        case LAPIC_LVT_TIMER:   // LVT Timer Reg
        case LAPIC_LVT_THERMAL: // LVT Thermal Monitor
        case LAPIC_LVT_PERFMON: // LVT Performance Counter
        case LAPIC_LVT_LINT0:   // LVT LINT0 Reg
        case LAPIC_LVT_LINT1:   // LVT LINT1 Reg
        case LAPIC_LVT_ERROR:   // LVT Error Reg
            return Lapic[0].LocalVectorTable[((Address & 0xff0) - LAPIC_LVT_TIMER) >> 4];
        case LAPIC_LVT_CMCI:
            return Lapic[0].LocalVectorTable[APIC_LVT_CMCI];
        case LAPIC_TIMER_INITIAL_COUNT:
            return Lapic[0].TimerInitialCountRegister;
        case LAPIC_TIMER_DIVIDE_CFG:
            return Lapic[0].TimerDivideConfigurationRegister;
        case LAPIC_ISR1: case LAPIC_ISR2:
        case LAPIC_ISR3: case LAPIC_ISR4:
        case LAPIC_ISR5: case LAPIC_ISR6:
        case LAPIC_ISR7: case LAPIC_ISR8:
            offset = ((Address & 0xff0) - LAPIC_ISR1) >> 3;
            return ((DWORD)Lapic[0].InterruptServiceRegister[offset + 1] << 0x10) | Lapic[0].InterruptServiceRegister[offset];
        case LAPIC_TMR1: case LAPIC_TMR2:
        case LAPIC_TMR3: case LAPIC_TMR4:
        case LAPIC_TMR5: case LAPIC_TMR6:
        case LAPIC_TMR7: case LAPIC_TMR8:
            offset = ((Address & 0xff0) - LAPIC_TMR1) >> 3;
            return ((DWORD)Lapic[0].TriggerModeRegister[offset + 1] << 0x10) | Lapic[0].TriggerModeRegister[offset];
        case LAPIC_IRR1: case LAPIC_IRR2:
        case LAPIC_IRR3: case LAPIC_IRR4:
        case LAPIC_IRR5: case LAPIC_IRR6:
        case LAPIC_IRR7: case LAPIC_IRR8:
            offset = ((Address & 0xff0) - LAPIC_IRR1) >> 3;
            //(Lapic[0].InterruptRequestRegister[((Address & 0xff0) - LAPIC_IRR1) >> 4] << 0xf) |
            return ((DWORD)Lapic[0].InterruptRequestRegister[offset + 1] << 0x10) | Lapic[0].InterruptRequestRegister[offset];
        case LAPIC_TIMER_CURRENT_COUNT:
            return 0;

        default:
            Lapic[0].ShadowErrorStatus |= APIC_ERR_ILLEGAL_ADDR;
    }
    return 0;
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

VOID ApicInitialize(VOID)
{
	memset(Lapic, '\0', sizeof(Lapic));
	RegisterMMIOHandler(APIC_DEFAULT_MMIO, ApicMMIOWriteHandler, ApicMMIOReadHandler);

    ApicReset();    
}
