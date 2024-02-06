#include "pic.h"
#include "SVMM.h"
#include "haxm.h"
#include "gvm.h"
#include "gvm.h"
#include "pci.h"
#include "ioapic.h"
#include <stdio.h>

static PIC Pic[2];

#ifdef HAXM
extern HANDLE hHaxCpu;
extern volatile struct hax_tunnel* tunnel;
#elif GVM
extern HANDLE hGvmCpu;
extern struct kvm_run* kvm_run;
#endif

extern DWORD svmm_events;
extern BYTE RequestInterruptWindow;


//BX_CPU_C::handleAsyncEvent priority handle event
//Priority 1: Hardware Reset and Machine Checks
//	RESET
//	Machine Check
//Priority 2: Trap on Task Switch
//  T flag in TSS is set
//Priority 3: External Hardware Interventions BX_CPU_C::interrupt
//   FLUSH
//   STOPCLK
//   SMI
//   INIT
//Priority 5: External Interrupts
//   VMX Preemption Timer Expired.
//   NMI Interrupts
//   Maskable Hardware Interrupts (All hardware interrupts) BX_CPU_C::InterruptAcknowledge
//Priority 6: Faults from fetching next instruction
//   Code breakpoint fault
//   Code segment limit violation (priority 7 on 486/Pentium)
//   Code page fault (priority 7 on 486/Pentium)
//
//Priority 7: Faults from decoding next instruction
//   Instruction length > 15 bytes
//   Illegal opcode
//   Coprocessor not available
//Priority 8: Faults on executing an instruction
//   Floating point execution
//   Overflow
//   Bound error
//   Invalid TSS
//   Segment not present
//   Stack fault
//   General protection
//   Data page fault
//   Alignment check



VOID PicInitialize(VOID)
{
	int i;

	memset(Pic, '\0', sizeof(Pic));
	Pic[0].InterruptMask = 0xff;
	Pic[0].CurrentPriority = 7;
	Pic[0].Mode = MODE_FULLY_NESTED;
	Pic[0].CurrentOffset = 0x08;

	Pic[1].InterruptMask = 0xff;
	Pic[1].CurrentPriority = 7;
	Pic[1].Mode = MODE_FULLY_NESTED;
	Pic[1].CurrentOffset = 0x70;

	for (i = 0x20; i <= 0x21; i++)
		RegisterPortIoHandler(i, (WritePortIoHandlerCallback)PicPortIoWriteHandler, (ReadPortIoHandlerCallback)PicPortIoReadHandler);
	for (i = 0xA0; i <= 0xA1; i++)
		RegisterPortIoHandler(i, (WritePortIoHandlerCallback)PicPortIoWriteHandler, (ReadPortIoHandlerCallback)PicPortIoReadHandler);
	
}


VOID PicService(PIC* Pic)
{
	DWORD Vector, Bytes, Ret;
	BYTE irq, highestPriority;


	if (Pic->Mode & MODE_SPECIAL_MASK) {
		for (irq = 0; irq < 8; irq++) {
			//if the irq is masked, try next
			if (~Pic->InterruptMask & (1 << irq))
				continue;

			//if interrupt is not already serviced and requested, service it and continue
			if (!(Pic->InterruptInService & (1 << irq)) && Pic->InterruptRequest & (1 << irq)) {
				Pic->Raised = 1;
				Pic->InterruptNumber = irq + Pic->CurrentOffset;
				/*
#ifdef HAXM
				Ret = DeviceIoControl(hHaxCpu, HAX_VCPU_IOCTL_INTERRUPT, &Vector, sizeof(Vector), NULL, 0, &Bytes, NULL);
				if (!Ret) {
					fprintf(stderr, "HAX_VCPU_IOCTL_INTERRUPT 0x%x", GetLastError());
					return;
				}
				tunnel->request_interrupt_window = 1;
#elif GVM
				struct kvm_interrupt interrupt;
				interrupt.irq = Pic->InterruptNumber;
				Ret = DeviceIoControl(hGvmCpu, GVM_INTERRUPT, &interrupt, sizeof(interrupt), NULL, 0, &Bytes, NULL);
				if (!Ret) {
					fprintf(stderr, "HAX_VCPU_IOCTL_INTERRUPT 0x%x", GetLastError());
					return;
				}
#endif
				*/
				svmm_events |= EVENT_PENDING_INTR;
				RequestInterruptWindow = 1;

			}
		}
	}
	else if (Pic->Mode & MODE_FULLY_NESTED) {
		for (irq = 0; irq <= Pic->CurrentPriority; irq++) {
			//if the irq is masked, try next
			if (Pic->InterruptMask & (1 << irq))
				continue;

			//if interrupt is not already serviced and requested, service it
			if (!(Pic->InterruptInService & (1 << irq)) && Pic->InterruptRequest & (1 << irq))
				break;
		}
		if (irq == Pic->CurrentPriority + 1)
			return;
		/* if a lower priority interrupt is already set, lower it */
		if (Pic->Raised) {
			Pic->Raised = 0;
			//PicLowerIrq(Pic->InterruptNumber);
		}
		Pic->Raised = 1;
		Pic->InterruptNumber = irq + Pic->CurrentOffset;
		/* master */
		if (Pic->InitCmd[2] & ICW3_CASCADE_ENABLE) {
			/*
#ifdef HAXM
			tunnel->request_interrupt_window = 1;
#elif GVM
			kvm_run->request_interrupt_window = 1;
#endif
			*/
			svmm_events |= EVENT_PENDING_INTR;
			RequestInterruptWindow = 1;

		}
		else {
			/* if slave, raise master's irq 2*/
			PicRaiseIrq(2);
		}
	}
}
BYTE PicIAC()
{
	BYTE vector, irq;

	svmm_events &= ~EVENT_PENDING_INTR;
	irq = Pic[0].InterruptNumber - Pic[0].CurrentOffset;
	Pic[0].Raised = 0;

	if (!(Pic[0].EdgeLevel & (1 << irq)))
		Pic[0].InterruptRequest &= ~(1 << irq);

	if (irq != 2) {
		Pic[0].CurrentPriority = irq;
		Pic[0].InterruptInService |= (1 << irq);
		vector = Pic[0].InterruptNumber;
	}
	else {
		irq = Pic[1].InterruptNumber - Pic[1].CurrentOffset;
		Pic[1].Raised = 0;
		if (!(Pic[1].EdgeLevel & (1 << irq)))
			Pic[1].InterruptRequest &= ~(1 << irq);

		Pic[0].InterruptPin &= ~(1 << 2);
		
		Pic[1].CurrentPriority = irq;
		Pic[1].InterruptInService |= (1 << irq);
		vector = Pic[1].InterruptNumber;

		//service the next interrupt of slave (if any)
		PicService(&Pic[1]);
	}
	//service the next interrupt of master (if any)
	PicService(&Pic[0]);

	return vector;
}



VOID PicLowerIrq(BYTE IrqNumber)
{
	BYTE i, irq;

	/* forward to IOAPIC */
	if (IrqNumber != 2)
		IoApicSetIrq(IrqNumber, 0);

	i = (0x7 < IrqNumber) ? 1 : 0;
	irq = 1 << (IrqNumber & 7);
	if (IrqNumber > 7 || !(Pic[i].InterruptPin & irq))
		return;

	Pic[i].InterruptPin &= ~irq;
	Pic[i].InterruptRequest &= ~irq;
}

VOID PicRaiseIrq(BYTE IrqNumber)
{
	BYTE i, irq;

	/* forward to IOAPIC */
	if (IrqNumber != 2)
		IoApicSetIrq(IrqNumber, 1);

	i = (0x7 < IrqNumber) ? 1 : 0;
	irq = 1 << (IrqNumber & 7);

	if (IrqNumber > 15 || !(Pic[i].InterruptPin & irq))
		return;
	Pic[i].InterruptPin |= irq;
	Pic[i].InterruptRequest |= irq;
	PicService(&Pic[i]);
	
}



VOID PicSetMode(BYTE Master, BYTE EdgeLevel)
{
	if (Master) 
		Pic[0].EdgeLevel = EdgeLevel;
	else 
		Pic[1].EdgeLevel = EdgeLevel;
}

ULONG PicPortIoReadHandler(ULONG64 Address, ULONG Length)
{
	BYTE i;

	i = (Address >= 0xA0) ? 1 : 0;

	switch (Address) {
		case 0x21:
		case 0xA1:
			return Pic[i].InterruptMask; //return IMR
			break;
		case 0x20:
		case 0xA0:		
			if (Pic[i].OperationCmd[2] & OCW3_READ_ISR)
				return Pic[i].InterruptInService;
			else if (Pic[i].OperationCmd[2] & OCW3_READ_IRR)
				return Pic[i].InterruptRequest;
			break;
	}
	return 0;
}

VOID PicPortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length)
{
	BYTE i;
	CHAR irq;

	i = (Address >= 0xA0) ? 1 : 0;

	switch (Address) {
		case 0x20:
		case 0xA0:
			if (Value & ICW1_INIT) {
				Pic[i].InitCmd[0] = Value & ICW1_REQ_ICW4;
				Pic[i].InterruptRequest = 0;
				Pic[i].InterruptMask = 0;
				Pic[i].State = STATE_INITIALIZED_ICW1;
				Pic[i].CurrentPriority = 7;
				Pic[i].Raised = 0;
				Pic[i].Mode = 0;
				if (!i) //master
					Pic[i].InterruptPin = ~0x02; //all but IRQ2
			}
			else if ((Value & 0x18) == OCW3_SELECT_DEFAULT) {
				//select to read ISR or IRR
				Pic[i].OperationCmd[2] = Value & OCW3_MASK;
				if (Value & 3) {
					return;
				}
				//set mode
				else if (Pic[i].OperationCmd[2] & OCW3_SPECIAL_MASK_MODE &&
					Pic[i].OperationCmd[2] & OCW3_ESPECIAL_MASK_MODE) {
					Pic[i].Mode = MODE_SPECIAL_MASK;
					PicService(&Pic[i]);
				}
				else if (Pic[i].OperationCmd[2] & OCW3_SPECIAL_MASK_MODE &&
					!(Pic[i].OperationCmd[2] & OCW3_ESPECIAL_MASK_MODE)) {
					Pic[i].Mode = MODE_SPECIAL_FULLY_NESTED;
					
				}
				else if (Pic[i].OperationCmd[2] & OCW3_POLL_MODE) {
					Pic[i].Mode = MODE_POLL;
				}
			}
			else {
				switch (Value) {
					case 0u:
					case 0x80u:
						Pic[i].Mode = Value ? MODE_AUTO_EOI : MODE_NORMAL_EOI;
						break;
					case 2u:
						return;
					case 0x20u:
					case 0xA0u:
						for (irq = Pic[i].CurrentPriority; irq >= 0; irq--) {
							if (Pic[i].InterruptInService & (1 << irq)) {
								Pic[i].InterruptInService &= ~(1 << irq);
								Pic[i].CurrentPriority = 7;
								break;
							}
						}
						PicService(&Pic[i]);
						/*
						bx_pic_c::clear_highest_interrupt(&p_s->master_pic);
						if (Value == 0xA0 && ++p_s->master_pic.lowest_priority > 7u)
							p_s->master_pic.lowest_priority = 0;
						bx_pic_c::pic_service(&p_s->master_pic);
						*/
						break;
					case 0x60u:
					case 0x61u:
					case 0x62u:
					case 0x63u:
					case 0x64u:
					case 0x65u:
					case 0x66u:
					case 0x67u:
						//NORMAL EOI
						Pic[i].InterruptInService &= ~(1 << (Value - 0x60));
						PicService(&Pic[i]);
						break;
					case 0xC0u:
					case 0xC1u:
					case 0xC2u:
					case 0xC3u:
					case 0xC4u:
					case 0xC5u:
					case 0xC6u:
					case 0xC7u:
						Pic[i].CurrentPriority = Value - 0xC0;
						break;
					case 0xE0u:
					case 0xE1u:
					case 0xE2u:
					case 0xE3u:
					case 0xE4u:
					case 0xE5u:
					case 0xE6u:
					case 0xE7u:
						Pic[i].InterruptInService &= ~(1 << (Value - 0xE0));
						Pic[i].CurrentPriority = (Value - 0xE0);
						PicService(&Pic[i]);
						break;
				}
			}
			break;
		case 0x21:
		case 0xA1:
			if (Pic[i].State == STATE_INITIALIZED_ICW1) {
				//Pic[i].InitCmd[1] = Value & ICW2_OFFSET_MASK;
				Pic[i].State = STATE_OFFSET_SET_ICW2;
				Pic[i].CurrentOffset = Value & 0xff;
			}
			else if (Pic[i].State == STATE_OFFSET_SET_ICW2) {
				if (!i) {
					Pic[i].InitCmd[2] = Value & ICW3_CASCADE_ENABLE;
					Pic[i].State = STATE_SLAVE_SET_ICW3;
				}
				else {
					Pic[i].InitCmd[2] = ICW3_SLAVE_ID;
					Pic[i].State = STATE_SLAVE_SET_ICW3;
				}
			}
			else if (Pic[i].State < STATE_ARCH_SET_ICW4 &&
				Pic[i].InitCmd[0] & ICW1_REQ_ICW4) {
				
				Pic[i].State = STATE_ARCH_SET_ICW4;
				
				if ((Value & ICW4_MASK) & ICW4_SPECIAL_FULLY_NESTED) {
					Pic[i].Mode |= MODE_SPECIAL_FULLY_NESTED;
				}
				else if ((Value & ICW4_MASK) & MODE_FULLY_NESTED) {
					Pic[i].Mode |= MODE_FULLY_NESTED;
				}

				if ((Value & ICW4_MASK) & ICW4_AUTO_EOI) {
					Pic[i].Mode |= MODE_AUTO_EOI;
				}
				else {
					Pic[i].Mode |= MODE_NORMAL_EOI;
				}
			}
			else if (Pic[i].State == STATE_ARCH_SET_ICW4) {
				Pic[i].InterruptMask = Value & 0xff; //Set IMR
				PicService(&Pic[i]);
			}
			break;
	}
}

