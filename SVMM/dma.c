#include "dma.h"
#include "pci.h"
#include "SVMM.h"
#include "gvm.h"

DMA_CONTROLLER Dma[2];

#ifdef GVM
extern struct kvm_run* kvm_run;
#endif

extern DWORD svmm_events;

VOID DmaInitialize(VOID)
{
	ULONG i;

	memset(Dma, '\0', sizeof(Dma));

	/* Register ports */
	for (i = 0; i < 0xF; i++)
		RegisterPortIoHandler(i, (WritePortIoHandlerCallback)DmaPortIoWriteHandler, (ReadPortIoHandlerCallback)DmaPortIoReadHandler);
	
	for (i = 0x80; i <= 0x8F; i++) 
		RegisterPortIoHandler(i, (WritePortIoHandlerCallback)DmaPortIoWriteHandler, (ReadPortIoHandlerCallback)DmaPortIoReadHandler);
	
	for (i = 0xC0; i <= 0xDE; i += 2) 
		RegisterPortIoHandler(i, (WritePortIoHandlerCallback)DmaPortIoWriteHandler, (ReadPortIoHandlerCallback)DmaPortIoReadHandler);
	
}



ULONG DmaPortIoReadHandler(ULONG64 Address, ULONG Length)
{
	ULONG DmaNumber;
	BYTE Channel;
	ULONG Value;

	DmaNumber = (Address >= 0xc0) ? 1 : 0;
	Channel = (Address >> (1 + DmaNumber)) & 0x03;

	switch (Address) {
		case 0x00: /* DMA-1 current address, channel 0 */
		case 0x02: /* DMA-1 current address, channel 1 */
		case 0x04: /* DMA-1 current address, channel 2 */
		case 0x06: /* DMA-1 current address, channel 3 */
		case 0xc0: /* DMA-2 current address, channel 0 */
		case 0xc4: /* DMA-2 current address, channel 1 */
		case 0xc8: /* DMA-2 current address, channel 2 */
		case 0xcc: /* DMA-2 current address, channel 3 */
			if (!Dma[DmaNumber].FlipFlop)
				Value = Dma[DmaNumber].Channels[Channel].CurrentAddressRegister & 0xff;
			
			else
				Value = (Dma[DmaNumber].Channels[Channel].CurrentAddressRegister >> 8) & 0xff;
			Dma[DmaNumber].FlipFlop = !Dma[DmaNumber].FlipFlop;
			return Value;
			break;
		case 0x01: /* DMA-1 current count, channel 0 */
		case 0x03: /* DMA-1 current count, channel 1 */
		case 0x05: /* DMA-1 current count, channel 2 */
		case 0x07: /* DMA-1 current count, channel 3 */
		case 0xc2: /* DMA-2 current count, channel 0 */
		case 0xc6: /* DMA-2 current count, channel 1 */
		case 0xca: /* DMA-2 current count, channel 2 */
		case 0xce: /* DMA-2 current count, channel 3 */
			if (!Dma[DmaNumber].FlipFlop)
				Value = Dma[DmaNumber].Channels[Channel].BaseCountRegister & 0xff;

			else
				Value = (Dma[DmaNumber].Channels[Channel].BaseCountRegister >> 8) & 0xff;
			Dma[DmaNumber].FlipFlop = !Dma[DmaNumber].FlipFlop;
			return Value;
			break;

		case 0x08: // DMA-1 Status Register
		case 0xd0: // DMA-2 Status Register
			Value = Dma[DmaNumber].StatusRegister;
			Dma[DmaNumber].StatusRegister &= 0xf0;
			return Value;
			break;

		case 0x0d: // DMA-1: temporary register
		case 0xda: // DMA-2: temporary register
			return 0;
			break;

		case 0x0081: // DMA-1 page register, channel 2
		case 0x0082: // DMA-1 page register, channel 3
		case 0x0083: // DMA-1 page register, channel 1
		case 0x0087: // DMA-1 page register, channel 0
			Channel = DMA_ADDRESS_TO_CHANNEL_PAGE(Address - 0x81);
			return Dma[0].Channels[Channel].PageRegisters;
			break;
		case 0x0089: // DMA-2 page register, channel 2
		case 0x008a: // DMA-2 page register, channel 3
		case 0x008b: // DMA-2 page register, channel 1
		case 0x008f: // DMA-2 page register, channel 0
			Channel = DMA_ADDRESS_TO_CHANNEL_PAGE(Address - 0x0089);
			return Dma[1].Channels[Channel].PageRegisters;
			break;
		case 0x0080:
		case 0x0084:
		case 0x0085:
		case 0x0086:
			Channel = DMA_ADDRESS_TO_CHANNEL_HIPAGE(Address - 0x0080);
			return Dma[0].Channels[Channel].HiPageRegisters;
			break;
		case 0x0088:
		case 0x008c:
		case 0x008d:
		case 0x008e:
			Channel = DMA_ADDRESS_TO_CHANNEL_HIPAGE(Address - 0x0088);
			return Dma[1].Channels[Channel].HiPageRegisters;
			break;
		case 0x0f: // DMA-1: undocumented: read all mask bits
		case 0xde: // DMA-2: undocumented: read all mask bits
			Value =
				Dma[DmaNumber].Channels[0].MaskRegister |
				(Dma[DmaNumber].Channels[1].MaskRegister << 1) |
				(Dma[DmaNumber].Channels[2].MaskRegister << 2) |
				(Dma[DmaNumber].Channels[3].MaskRegister << 3);
			return Value | 0xf0;
			break;
	}
	return 0;
}

VOID DmaPortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length)
{
	ULONG DmaNumber, i;
	BYTE Channel;

	DmaNumber = (Address >= 0xc0) ? 1 : 0;
	Channel = (Address >> (1 + DmaNumber)) & 0x03;

	switch (Address) {
		case 0x00: /* DMA-1 current address, channel 0 */
		case 0x02: /* DMA-1 current address, channel 1 */
		case 0x04: /* DMA-1 current address, channel 2 */
		case 0x06: /* DMA-1 current address, channel 3 */
		case 0xc0: /* DMA-2 current address, channel 0 */
		case 0xc4: /* DMA-2 current address, channel 1 */
		case 0xc8: /* DMA-2 current address, channel 2 */
		case 0xcc: /* DMA-2 current address, channel 3 */
			if (!Dma[DmaNumber].FlipFlop) {
				Dma[DmaNumber].Channels[Channel].BaseAddressRegister = (Value & 0xff) | (Dma[DmaNumber].Channels[Channel].BaseAddressRegister << 8);
				Dma[DmaNumber].Channels[Channel].CurrentAddressRegister = Dma[DmaNumber].Channels[Channel].BaseAddressRegister;
			}
			else {
				Dma[DmaNumber].Channels[Channel].BaseAddressRegister = ((Value & 0xff) << 8) | (Dma[DmaNumber].Channels[Channel].BaseAddressRegister & 0xff);
				Dma[DmaNumber].Channels[Channel].CurrentAddressRegister = Dma[DmaNumber].Channels[Channel].BaseAddressRegister;
				Dma[DmaNumber].Channels[Channel].CurrentCountRegister = 0;
			}
			
			Dma[DmaNumber].FlipFlop = !Dma[DmaNumber].FlipFlop;
			break;
		case 0x01: /* DMA-1 current count, channel 0 */
		case 0x03: /* DMA-1 current count, channel 1 */
		case 0x05: /* DMA-1 current count, channel 2 */
		case 0x07: /* DMA-1 current count, channel 3 */
		case 0xc2: /* DMA-2 current count, channel 0 */
		case 0xc6: /* DMA-2 current count, channel 1 */
		case 0xca: /* DMA-2 current count, channel 2 */
		case 0xce: /* DMA-2 current count, channel 3 */
			if (!Dma[DmaNumber].FlipFlop) {
				Dma[DmaNumber].Channels[Channel].BaseCountRegister = (Value & 0xff) | (Dma[DmaNumber].Channels[Channel].BaseCountRegister << 8);
				Dma[DmaNumber].Channels[Channel].CurrentCountRegister = Dma[DmaNumber].Channels[Channel].BaseCountRegister;
			}
			else {
				Dma[DmaNumber].Channels[Channel].BaseCountRegister = ((Value & 0xff) << 8) | (Dma[DmaNumber].Channels[Channel].BaseCountRegister & 0xff);
				Dma[DmaNumber].Channels[Channel].CurrentCountRegister = Dma[DmaNumber].Channels[Channel].BaseCountRegister;
				Dma[DmaNumber].Channels[Channel].CurrentAddressRegister = Dma[DmaNumber].Channels[Channel].BaseAddressRegister;
			}
			Dma[DmaNumber].FlipFlop = !Dma[DmaNumber].FlipFlop;
			break;

		case 0x08: /* DMA-1: command register */
		case 0xd0: /* DMA-1: command register */
			Dma[DmaNumber].CommandRegister = Value & 0xff;
			//Dma[DmaNumber].ctrl_disabled
			//control_HRQ(DmaNumber);
			DmaControlHoldRequestLine(DmaNumber);
			break;

		case 0x09: // DMA-1: request register
		case 0xd2: // DMA-2: request register
			if (Value & 0x04) 
				Dma[DmaNumber].StatusRegister |= (1 << (Channel + 4));
			
			else 
				Dma[DmaNumber].StatusRegister &= ~(1 << (Channel + 4));
			DmaControlHoldRequestLine(DmaNumber);
			//control_HRQ(DmaNumber);
			break;

		case 0x0a:
		case 0xd4:
			Channel = Value & 0x03;
			Dma[DmaNumber].Channels[Channel].MaskRegister = (Value & 0x04) > 0;
			//control_HRQ(ma_sl);
			DmaControlHoldRequestLine(DmaNumber);
			break;

		case 0x0b: /* DMA-1 mode register */
		case 0xd6: /* DMA-2 mode register */
			Channel = Value & 0x03;
			Dma[DmaNumber].Channels[Channel].ModeRegister = Value & 0xff;
			break;

		case 0x0c: /* DMA-1 clear byte flip/flop */
		case 0xd8: /* DMA-2 clear byte flip/flop */
			Dma[DmaNumber].FlipFlop = 0;
			break;

		case 0x0d: // DMA-1: temporary register
		case 0xda: // DMA-2: temporary register
			//reset_controller(DmaNumber);
			break;

		case 0x0e: // DMA-1: clear mask register
		case 0xdc: // DMA-2: clear mask register
			for (i = 0; i < 4; i++)
				Dma[DmaNumber].Channels[i].MaskRegister = 0;
			//control_HRQ(DmaNumber);
			DmaControlHoldRequestLine(DmaNumber);
			break;
		case 0x0f: // DMA-1: write all mask bits
		case 0xde: // DMA-2: write all mask bits
			for (i = 0; i < 4; i++)
				Dma[DmaNumber].Channels[i].MaskRegister = (Value >> 0) & 1;
			//control_HRQ(DmaNumber);
			DmaControlHoldRequestLine(DmaNumber);
			break;

		case 0x0081: // DMA-1 page register, channel 2
		case 0x0082: // DMA-1 page register, channel 3
		case 0x0083: // DMA-1 page register, channel 1
		case 0x0087: // DMA-1 page register, channel 0
			Channel = DMA_ADDRESS_TO_CHANNEL_PAGE(Address - 0x81);
			Dma[0].Channels[Channel].PageRegisters = Value & 0xff;
			Dma[0].Channels[Channel].HiPageRegisters = 0;
			break;

		case 0x0089: // DMA-2 page register, channel 2
		case 0x008a: // DMA-2 page register, channel 3
		case 0x008b: // DMA-2 page register, channel 1
		case 0x008f: // DMA-2 page register, channel 0
			Channel = DMA_ADDRESS_TO_CHANNEL_PAGE(Address - 0x89);
			Dma[1].Channels[Channel].PageRegisters = Value & 0xff;
			Dma[1].Channels[Channel].HiPageRegisters = 0;
			break;
		case 0x0080:
		case 0x0084:
		case 0x0085:
		case 0x0086:
			Channel = DMA_ADDRESS_TO_CHANNEL_HIPAGE(Address - 0x80);
			Dma[0].Channels[Channel].HiPageRegisters = Value & 0xff;
			break;
		case 0x0088:
		case 0x008c:
		case 0x008d:
		case 0x008e:
			Channel = DMA_ADDRESS_TO_CHANNEL_HIPAGE(Address - 0x88);
			Dma[1].Channels[Channel].HiPageRegisters = Value & 0xff;
			break;
	}
}

ULONG DmaWriteMemory(BYTE Channel, BYTE *Buffer, ULONG64 GuestPhysicalAddress, ULONG Length)
{
	ULONG64 Offset;
	ULONG DmaNumber, Page, PageHi;

	if (Channel > 7)
		return 0;

	DmaNumber = DMA_CHANNEL_TO_CONTROLLER(Channel);
	if (Channel > 3)
		Channel &= 3;
	if (!Dma[DmaNumber].Channels[Channel].DmaWriteHandler)
		return 0;
	Page = Dma[DmaNumber].Channels[Channel].PageRegisters & ~DmaNumber;
	PageHi = Dma[DmaNumber].Channels[Channel].HiPageRegisters;
	Offset = (PageHi << 24) | (Page << 16) | (Dma[DmaNumber].Channels[Channel].CurrentAddressRegister << DmaNumber);
	Dma[DmaNumber].Channels[Channel].DmaWriteHandler(Channel, Buffer, Offset + GuestPhysicalAddress, Length);

	return 0;
}

ULONG DmaReadMemory(BYTE Channel, BYTE* Buffer, ULONG64 GuestPhysicalAddress, ULONG Length)
{
	ULONG64 Offset;
	ULONG DmaNumber, Page, PageHi;

	if (Channel > 7)
		return 0;

	DmaNumber = DMA_CHANNEL_TO_CONTROLLER(Channel);
	if (Channel > 3)
		Channel &= 3;

	if (!Dma[DmaNumber].Channels[Channel].DmaReadHandler)
		return 0;
	Page = Dma[DmaNumber].Channels[Channel].PageRegisters & ~DmaNumber;
	PageHi = Dma[DmaNumber].Channels[Channel].HiPageRegisters;
	Offset = (PageHi << 24) | (Page << 16) | (Dma[DmaNumber].Channels[Channel].CurrentAddressRegister << DmaNumber);
	Dma[DmaNumber].Channels[Channel].DmaReadHandler(Channel, Buffer, Offset + GuestPhysicalAddress, Length);
	return 0;
}

//SetDRQ
VOID DmaSetHoldRequest(ULONG Channel, BYTE Value)
{
	ULONG DmaNumber;

	if (Channel > 7)
		return;
	DmaNumber = DMA_CHANNEL_TO_CONTROLLER(Channel);
	if (Channel > 3)
		Channel &= 3;
	if (!Value) 
		Dma[DmaNumber].StatusRegister &= ~(1 << (Channel + 4));
	else 
		Dma[DmaNumber].StatusRegister |= (1 << (Channel + 4));

	DmaControlHoldRequestLine(DmaNumber);
}

//control_HRQ
/* Raise/Low HOLD from cpu in case of the first 8 byte Dma Controller or forward the request from cascade channel */
static VOID DmaControlHoldRequestLine(ULONG DmaNumber)
{
	ULONG i;

	if (Dma[DmaNumber].StatusRegister & CMD_DISABLE)
		return;
	if (!(Dma[DmaNumber].StatusRegister & 0xf0)) {
		if (DmaNumber) {
#ifdef GVM
			kvm_run->user_event_pending = 0;
			svmm_events &= ~EVENT_DMA;

#endif
		}
		else 
			DmaSetHoldRequest(4, 0);
		
		return;
	}

	for (i = 0; i < 4; i++) {
		if (Dma[DmaNumber].StatusRegister & (1 << (i + 4)) &&
			!Dma[DmaNumber].Channels[i].MaskRegister) {
			if (DmaNumber) {
#ifdef GVM
				kvm_run->user_event_pending = 1;
				svmm_events |= EVENT_DMA;
#endif
			}
			else 
				DmaSetHoldRequest(4, 1);
			break;
		}
	}
}

VOID DmaRaiseHoldAcknowledge()
{
	ULONG i, GuestPa, Page, PageHi, Length, Ret;
	BYTE DmaNumber, DmaBuffer[512];

	for (i = 0; i < 4; i++) {
		if (Dma[1].StatusRegister & (1 << (i + 4)) &&
			!Dma[1].Channels[i].MaskRegister) {
			DmaNumber = 1;
			break;
		}	
	}
	if (!i) {
		Dma[1].DmaAck[0] = 1;
		for (i = 0; i < 4; i++) {
			if (Dma[0].StatusRegister & (1 << (i + 4)) &&
				!Dma[0].Channels[i].MaskRegister) {
				DmaNumber = 0;
				break;
			}
		}
	}
	if (i > 3)
		return;
	Page = Dma[DmaNumber].Channels[i].PageRegisters & ~DmaNumber;
	PageHi = Dma[DmaNumber].Channels[i].HiPageRegisters;
	GuestPa = (PageHi << 24) | (Page << 16) | (Dma[DmaNumber].Channels[i].CurrentAddressRegister << DmaNumber);
	Length = 0;
	if (Dma[DmaNumber].Channels[i].ModeRegister & DMA_MODE_DECREMENT) {
		Length = (Dma[DmaNumber].Channels[i].CurrentCountRegister + 1) << DmaNumber;
		if (Length > 512)
			Length = 512;
	}
	else {
		Length = 1 << DmaNumber;
	}
	if (Dma[DmaNumber].Channels[i].ModeRegister & DMA_MODE_WRITE) {
		Ret = Dma[DmaNumber].Channels[i].DmaWriteHandler(i, DmaBuffer, GuestPa, Length);

	}
	else if (Dma[DmaNumber].Channels[i].ModeRegister & DMA_MODE_READ) {
		Ret = Dma[DmaNumber].Channels[i].DmaReadHandler(i, DmaBuffer, GuestPa, Length);

	}
	else if (Dma[DmaNumber].Channels[i].ModeRegister & DMA_MODE_VERIFY) {
		Ret = Dma[DmaNumber].Channels[i].DmaWriteHandler(i, DmaBuffer, GuestPa, 1);
	}
	else
		Ret = 0;

	Dma[DmaNumber].DmaAck[i] = 1;
	if (!(Dma[DmaNumber].Channels[i].ModeRegister & DMA_MODE_DECREMENT))
		Dma[DmaNumber].Channels[i].CurrentAddressRegister += Ret;
	else
		Dma[DmaNumber].Channels[i].CurrentAddressRegister--;
	Dma[DmaNumber].Channels[i].CurrentCountRegister -= Length;

	if (Dma[DmaNumber].Channels[i].CurrentCountRegister == 0xffff) {
		Dma[DmaNumber].StatusRegister |= (1 << i);
		if (!(Dma[DmaNumber].Channels[i].ModeRegister & DMA_MODE_AUTO_INIT)) {
			Dma[DmaNumber].Channels[i].MaskRegister = 1;
		}
		else {
			Dma[DmaNumber].Channels[i].CurrentAddressRegister = Dma[DmaNumber].Channels[i].BaseAddressRegister;
			Dma[DmaNumber].Channels[i].CurrentCountRegister = Dma[DmaNumber].Channels[i].BaseCountRegister;
		}
		svmm_events &= ~EVENT_DMA;
		Dma[DmaNumber].DmaAck[i] = 0;
		if (!DmaNumber) {
			DmaSetHoldRequest(4, 0);
			Dma[1].DmaAck[i] = 0;
		}
	}
}
