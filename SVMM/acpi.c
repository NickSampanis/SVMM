#include "acpi.h"
#include "pci.h"
#include "gvm.h"
#include "timer.h"

struct ACPI Acpi;
extern PCI PciDevices[MAX_BUSES][MAX_PCI_DEVICES_PER_BUS];
static DWORD TimerId;

#define ACPI_PM_PORT_MASK ((1 << 0) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 9) | (1 << 5) | (1 << 11) | (1 << 13) | (1 << 14) | (1 << 15))
#define ACPI_SM_PORT_MASK ((1 << 1) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 9) | (1 << 5) | (1 << 11) | (1 << 13) | (1 << 14) | (1 << 15))

USHORT GetPmStatus(void)
{
	USHORT status;
	ULONG64 value;

	status = Acpi.PmStatus;
	value = (TimerGetElapsed(TimerId) * PM_FREQ) / 1000000;
	if (value >= Acpi.TimerOverflow)
		Acpi.PmStatus |= TMROF_EN;
	return status;
}

void PmUpdateSci(void)
{
	USHORT status;
	BYTE sci_level;
	
	status = GetPmStatus();
	sci_level = (status & Acpi.PmStatus) & (RTC_EN | PWRBTN_EN | GBL_EN | TMROF_EN);


	//PciSetIrq()
	/*
	// schedule a timer interruption if needed
	if ((BX_ACPI_THIS s.pmen & TMROF_EN) && !(pmsts & TMROF_EN)) {
		Bit64u expire_time = muldiv64(BX_ACPI_THIS s.tmr_overflow_time, 1000000, PM_FREQ);
		bx_pc_system.activate_timer(BX_ACPI_THIS s.timer_index, (Bit32u)expire_time, 0);
	}
	else {
		bx_pc_system.deactivate_timer(BX_ACPI_THIS s.timer_index);
	}
	*/
}

void AcpiTimerHandler()
{
	PmUpdateSci();
}




static VOID AcpiPciConfWriteHandler(PCI* Pci, ULONG64 Address, ULONG Value, ULONG Length)
{
	ULONG i;

	if ((Address >= 0x10) && (Address < 0x34))
		return;

	if (Address + Length >= 0xff)
		return;

	for (i = 0; i < Length; i++) {
		switch (Address + i) {
		case 0x04:
			Pci->conf[Address + i] = (Value & 0xfe) | (Value & 1);
			break;
		case 0x06: // disallowing write to status lo-byte (is that expected?)
			break;
		case 0x40:
			Pci->conf[Address + i] = (Value & 0xc0) | 0x01;
			break;
		case 0x43:
			Pci->conf[Address + i] = (Value >> (i * 8)) & 0xff;
			PciWriteConfHandler(PCI_CONF_BAR0, Value, 4);
			return;
		case 0x90:
			Pci->conf[Address + i] = (Value & 0xf0) | 0x01;
			break;
		case 0x93:
			Pci->conf[Address + i] = (Value >> (i * 8)) & 0xff;
			PciWriteConfHandler(PCI_CONF_BAR1, Value, 4);
			return;
		default:
			Pci->conf[Address + i] = (Value >> (i * 8)) & 0xff;
		}
	}
}

static ULONG AcpiPciConfReadHandler(ULONG64 Address, ULONG Length)
{

}

VOID AcpiWritePortIoHandler(ULONG Address, ULONG Value, ULONG Length)
{
	ULONG  devFunc, reg;

	devFunc = BX_PCI_DEVICE(3, 0);
	reg = Address & 0x3f;
	if ((Address & 0xffc0) == Acpi.PmBase) {
		if (!(PciDevices[0][devFunc].conf[0x80] & 1))
			return;
		switch (reg) {
			case 0:
				//div64_u64()
				break;
			case 2:
				break;
			case 4:
				break;
			case 5:
				break;
			case 6:
				break;
			case 7:
				break;
			case 0x0c: // GPSTS
			case 0x0d: // GPSTS
			case 0x14: // PLVL2
			case 0x15: // PLVL3
			case 0x18: // GLBSTS
			case 0x19: // GLBSTS
			case 0x1c: // DEVSTS
			case 0x1d: // DEVSTS
			case 0x1e: // DEVSTS
			case 0x1f: // DEVSTS
			case 0x30: // GPIREG
			case 0x31: // GPIREG
			case 0x32: // GPIREG
				break;
			default:
				if (reg >= ACPI_PM_REGISTERS_SIZE)
					return;
				Acpi.PmRegisters[reg] = Value;
				if (Length >= 2 && reg < ACPI_PM_REGISTERS_SIZE - 1) {
					Acpi.PmRegisters[reg + 1] = (Value >> 8);
				}
				if (Length == 4 && reg < ACPI_PM_REGISTERS_SIZE - 3) {
					Acpi.PmRegisters[reg + 2] = (Value >> 16);
					Acpi.PmRegisters[reg + 3] = (Value >> 24);
				}
				break;
		}
	}
	else if ((Address & 0xfff0) == Acpi.SmBase) {
		if (!(PciDevices[0][devFunc].conf[0x04] & 0x01) &&
			!(PciDevices[0][devFunc].conf[0xd2] & 0x01)) {
			return;
		}
		switch (reg) {
			case 0:
				break;
			case 2:
				break;
			case 3:
				break;
			case 4:
				break;
			case 5:
				break;
			case 6:
				break;
			case 7:
				break;
		}
	}
}

ULONG AcpiReadPortIoHandler(ULONG Address, ULONG Length)
{

}

VOID AcpiInitialize()
{
	ULONG  devFunc, PciAddress;

	memset(&Acpi, '\0', sizeof(Acpi));
	devFunc = BX_PCI_DEVICE(3, 0);
	PciAddress = PCI_DEVFUNC_OFFSET_TO_ADDRESS(0, devFunc, 0);
	PciRegisterConfigHandler(PciAddress, AcpiPciConfWriteHandler, AcpiPciConfReadHandler);
	PciInitConfig(PciAddress, 0x8086, PCI_DEVICE_ID_INTEL_82371AB_3, 0x03, 0x0680, 0x00, 0x00, PCI_INTA);
	PciSetBarIo(PciAddress, 0, 0, 16, AcpiReadPortIoHandler, AcpiWritePortIoHandler, ACPI_PM_PORT_MASK);
	PciSetBarIo(PciAddress, 1, 0, 64, AcpiReadPortIoHandler, AcpiWritePortIoHandler, ACPI_SM_PORT_MASK);
	//RegisterPortIoHandler(ACPI_DBG_IO_ADDR, AcpiWritePortIoHandler, AcpiReadPortIoHandler);
	PciDevices[0][devFunc].conf[0x06] = 0x80; // status_devsel_medium
	PciDevices[0][devFunc].conf[0x07] = 0x02;
	// PM base 0x40 - 0x43
	PciDevices[0][devFunc].conf[0x40] = 0x01;
	PciDevices[0][devFunc].conf[0x41] = 0x00;
	PciDevices[0][devFunc].conf[0x42] = 0x00;
	PciDevices[0][devFunc].conf[0x43] = 0x00;

	PciDevices[0][devFunc].conf[0x5f] = 0x90;
	PciDevices[0][devFunc].conf[0x63] = 0x60;
	PciDevices[0][devFunc].conf[0x67] = 0x98;

	// SM base 0x90 - 0x93
	PciDevices[0][devFunc].conf[0x90] = 0x01;
	PciDevices[0][devFunc].conf[0x91] = 0x00;
	PciDevices[0][devFunc].conf[0x92] = 0x00;
	PciDevices[0][devFunc].conf[0x93] = 0x00;

	TimerId = TimerRegister(TICK_PERIOD * 10, AcpiTimerHandler, NULL);
}