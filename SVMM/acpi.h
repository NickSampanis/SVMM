#pragma once
#include <Windows.h>

#define PCI_DEVICE_ID_INTEL_82371AB_3   0x7113
#define ACPI_DBG_IO_ADDR				0xb044

#define ACPI_PM_REGISTERS_SIZE			56
#define ACPI_SM_REGISTERS_SIZE			56

#define PM_FREQ							3579545
#define RSM_STS			(1 << 15)
#define PWRBTN_STS		(1 << 8)
#define RTC_EN			(1 << 10)
#define PWRBTN_EN		(1 << 8)
#define GBL_EN			(1 << 5)
#define TMROF_EN		(1 << 0)

#define SCI_EN			(1 << 0)
#define SUS_EN			(1 << 13)

struct SmBus {
	BYTE HostStatus;
	BYTE SlaveStatus;
	BYTE HostControl;
	BYTE HostCommand;
	BYTE HostAddress;
	BYTE HostData0;
	BYTE HostData1;
	BYTE SlaveControl;
	BYTE Index;
	BYTE Data[ACPI_SM_REGISTERS_SIZE];

};

struct ACPI {
	ULONG PmBase;
	ULONG SmBase;
	BYTE PmRegisters[ACPI_PM_REGISTERS_SIZE];
	struct SmBus SmBus;
	ULONG TimerOverflow;
	USHORT PmStatus;
	USHORT PmEnable;
	USHORT PmControl;
	BYTE PmTimeReg;
	ULONG TimerId;
}; 

VOID AcpiInitialize();
VOID AcpiGenerateAcpi(BYTE Value);