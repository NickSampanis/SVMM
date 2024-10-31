#include "acpi.h"
#include "pci.h"
#include "gvm.h"
#include "timer.h"
#include "cmos.h"
#include "apic.h"

struct ACPI Acpi;
extern PCI PciDevices[MAX_BUSES][MAX_PCI_DEVICES_PER_BUS];

#define ACPI_PM_PORT_MASK ((1 << 0) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 9) | (1 << 5) | (1 << 11) | (1 << 13) | (1 << 14) | (1 << 15))
#define ACPI_SM_PORT_MASK ((1 << 1) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 9) | (1 << 5) | (1 << 11) | (1 << 13) | (1 << 14) | (1 << 15))
#define ACPI_ENABLE		0xf1
#define ACPI_DISABLE	0xf0

VOID AcpiGenerateAcpi(BYTE Value)
{
	ULONG  devFunc;

	if (Value == ACPI_ENABLE) 
		Acpi.PmControl |= SCI_EN;
	else if (Value == ACPI_DISABLE) 
		Acpi.PmControl &= ~SCI_EN;
	devFunc = BX_PCI_DEVICE(1, 3);
	if (PciDevices[0][devFunc].conf[0x5b] & 2)
		ApicBusDeliverInterrupt(0, 0, 0, DELIVERY_MODE_SMI, 1, 1);
	
}

USHORT AcpiGetPmStatus(void)
{
	USHORT Status;
	ULONG64 value;

	Status = Acpi.PmStatus;
	value = muldiv64(TimerGetTotalUseconds(), PM_FREQ, 1000000);
	if (value >= Acpi.TimerOverflow)
		Acpi.PmStatus |= TMROF_EN;

	return Status;
}

VOID AcpiPmUpdateSci(void)
{
	ULONG64 ExpireTime;
	ULONG  devFunc, PciAddress;
	USHORT Status;
	BYTE SciLevel;

	devFunc = BX_PCI_DEVICE(1, 3);
	PciAddress = PCI_DEVFUNC_OFFSET_TO_ADDRESS(0, devFunc, 0);
	Status = AcpiGetPmStatus();
	SciLevel = (Status & Acpi.PmStatus) & (RTC_EN | PWRBTN_EN | GBL_EN | TMROF_EN);

	
	PciSetIrq(PciAddress, PciDevices[0][devFunc].conf[0x3d], SciLevel);
	if ((Acpi.PmEnable & TMROF_EN) && !(Status & TMROF_EN)) {
		ExpireTime = muldiv64(Acpi.TimerOverflow, 1000000, PM_FREQ);
		TimerActivate(Acpi.TimerId, ExpireTime, 0);
	}
	else 
		TimerDeactivate(Acpi.TimerId);
}

void AcpiTimerHandler()
{
	AcpiPmUpdateSci();
}




static VOID AcpiPciConfWriteHandler(PCI* Pci, ULONG64 Address, ULONG Value, ULONG Length)
{

	if ((Address >= 0x10) && (Address < 0x34))
		return;

	if (Address + Length >= 0xff)
		return;

	
	switch (Address) {
		case 0x06: // disallowing write to status lo-byte 
			break;
		case 0x40:
			PciWriteConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(0, BX_PCI_DEVICE(1, 3), PCI_CONF_BAR0), Value & ~1, 4);
			Acpi.PmBase = Value & ~1;
			break;
		case 0x90:
			PciWriteConfHandler(PCI_DEVFUNC_OFFSET_TO_ADDRESS(0, BX_PCI_DEVICE(1, 3), PCI_CONF_BAR1), Value & ~1, 4);
			Acpi.SmBase = Value & ~1;
			break;
		default:
			*(ULONG*)&Pci->conf[Address] = Value;
			break;
	}
	
}

static ULONG AcpiPciConfReadHandler(ULONG64 Address, ULONG Length)
{
	return 0xff;
}

VOID AcpiWritePortIoHandler(ULONG Address, ULONG Value, ULONG Length)
{
	ULONG  devFunc, reg;
	ULONG64 tmp;

	devFunc = BX_PCI_DEVICE(1, 3);
	reg = Address & 0x3f;
	if ((Address & 0xffc0) == Acpi.PmBase) {
		if (!(PciDevices[0][devFunc].conf[0x80] & 1))
			return;
		switch (reg) {
			case 0:
				tmp = muldiv64(TimerGetTotalUseconds(), PM_FREQ, 1000000);
				if (tmp >= Acpi.TimerOverflow) {
					Acpi.PmStatus |= TMROF_EN;
					tmp = (tmp + 0x800000ULL) & ~0x7fffffULL;
				}
				Acpi.PmStatus = 0;
				AcpiPmUpdateSci();
				break;
			case 2:
				Acpi.PmEnable = Value;
				AcpiPmUpdateSci();
				break;
			case 4:
				Acpi.PmControl = Value & ~(1UL << 13);
				if (Acpi.PmControl & (1UL << 13)) {
					if ((Value >> 10) & 7) {
						Acpi.PmStatus |= (RSM_STS | PWRBTN_STS);
						CmosSetRegister(0x0F, 0xFE);
					}
				}
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
				if (reg > ACPI_PM_REGISTERS_SIZE - Length)
					return;
				switch (Length) {
					case 1:
						*(BYTE*)&Acpi.PmRegisters[reg] = Value;
					case 2:
						*(USHORT*)&Acpi.PmRegisters[reg] = Value;
					case 4:
						*(ULONG*)&Acpi.PmRegisters[reg] = Value;
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
			case 0x00:
				Acpi.SmBus.HostStatus = 0;
			case 0x01:
				Acpi.SmBus.SlaveStatus = 0;
			case 0x02:
				Acpi.SmBus.HostControl = 0;
			case 0x03:
				Acpi.SmBus.HostCommand = 0;
			case 0x04:
				Acpi.SmBus.HostAddress = 0;
			case 0x05:
				Acpi.SmBus.HostData0 = 0;
			case 0x06:
				Acpi.SmBus.HostData1 = 0;
			case 0x07:
				Acpi.SmBus.Data[Acpi.SmBus.Index++] = Value;
				if (Acpi.SmBus.Index > 31)
					Acpi.SmBus.Index = 0;
				return Value & 0xffffffff;
			case 0x08:
				Acpi.SmBus.SlaveControl = 0;
		}
	}
}

ULONG AcpiReadPortIoHandler(ULONG Address, ULONG Length)
{
	ULONG  devFunc, reg;
	ULONG64 value;

	reg = Address & 0x3f;
	if (reg > ACPI_PM_REGISTERS_SIZE - Length)
		return 0;
	devFunc = BX_PCI_DEVICE(1, 3);
	if ((Address & 0xffc0) == Acpi.PmBase) {
		if (!(PciDevices[0][devFunc].conf[0x80] & 1))
			return;
		switch (reg) {
			case 0x00:
				value = muldiv64(TimerGetTotalUseconds(), PM_FREQ, 1000000);
				if (value >= Acpi.TimerOverflow)
					Acpi.PmStatus |= TMROF_EN;
				return Acpi.PmStatus;
			case 0x02:
				return Acpi.PmEnable;
			case 0x04:
				return Acpi.PmControl;
			case 0x08:
				return muldiv64(TimerGetTotalUseconds(), PM_FREQ, 1000000) & 0xffffffff;
		}
		switch (Length) {
			case 1:
				return *(BYTE*)&Acpi.PmRegisters[reg];
			case 2:
				return *(USHORT*)&Acpi.PmRegisters[reg];
			case 4:
				return *(ULONG*)&Acpi.PmRegisters[reg];
		}

	}
	else if ((Address & 0xfff0) == Acpi.SmBase) {
		switch (reg) {
			case 0x00:
				return Acpi.SmBus.HostStatus;
			case 0x01:
				return Acpi.SmBus.SlaveStatus;
			case 0x02:
				Acpi.SmBus.Index = 0;
				return Acpi.SmBus.HostControl & 0x1f;
			case 0x03:
				return Acpi.SmBus.HostCommand;
			case 0x04:
				return Acpi.SmBus.HostAddress;
			case 0x05:
				return Acpi.SmBus.HostData0;
			case 0x06:
				return Acpi.SmBus.HostData1;
			case 0x07:
				value = Acpi.SmBus.Data[Acpi.SmBus.Index++];
				if (Acpi.SmBus.Index > 31) 
					Acpi.SmBus.Index = 0;
				return value & 0xffffffff;
			case 0x08:
				return Acpi.SmBus.SlaveControl;
		}
	}
	return 0;
}

VOID AcpiInitialize()
{
	ULONG  devFunc, PciAddress;

	memset(&Acpi, '\0', sizeof(Acpi));
	devFunc = BX_PCI_DEVICE(1, 3);
	PciAddress = PCI_DEVFUNC_OFFSET_TO_ADDRESS(0, devFunc, 0);
	PciRegisterConfigHandler(PciAddress, AcpiPciConfWriteHandler, AcpiPciConfReadHandler);
	PciInitConfig(PciAddress, 0x8086, PCI_DEVICE_ID_INTEL_82371AB_3, 0x03, 0x0680, 0x00, PCI_HDR_TYPE_DEVICE, PCI_INTA);
	PciSetBarIo(PciAddress, 0, 0, 16, AcpiReadPortIoHandler, AcpiWritePortIoHandler, 0);
	PciSetBarIo(PciAddress, 1, 0, 64, AcpiReadPortIoHandler, AcpiWritePortIoHandler, 0);
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

	Acpi.TimerOverflow = 0xffffffff;
	Acpi.TimerId = TimerRegister(TICK_PERIOD * 10, AcpiTimerHandler, NULL);
}