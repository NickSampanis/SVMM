#ifndef CMOS_H
#define CMOS_H
#include <Windows.h>



#define  REG_SEC                     0x00
#define  REG_SEC_ALARM               0x01
#define  REG_MIN                     0x02
#define  REG_MIN_ALARM               0x03
#define  REG_HOUR                    0x04
#define  REG_HOUR_ALARM              0x05
#define  REG_WEEK_DAY                0x06
#define  REG_MONTH_DAY               0x07
#define  REG_MONTH                   0x08
#define  REG_YEAR                    0x09
#define  REG_STAT_A                  0x0a
#define  REG_STAT_B                  0x0b
#define  REG_STAT_C                  0x0c
#define  REG_STAT_D                  0x0d
#define  REG_DIAGNOSTIC_STATUS       0x0e  /* alternatives */
#define  REG_SHUTDOWN_STATUS         0x0f
#define  REG_EQUIPMENT_BYTE          0x14
#define  REG_CSUM_HIGH               0x2e
#define  REG_CSUM_LOW                0x2f
#define  REG_IBM_CENTURY_BYTE        0x32  /* alternatives */
#define  REG_IBM_PS2_CENTURY_BYTE    0x37  /* alternatives */

//   boot device codes:
//     0x00 : not defined
//     0x01 : first floppy
//     0x02 : first harddrive
//     0x03 : first cdrom
//     0x04 - 0x0f : PnP expansion ROMs (e.g. Etherboot)
//     else : boot failure

#define CMOS_BOOT_CD						3
#define CMOS_BOOT_HD						2

#define CMOS_BOOT_REG						0x3d
#define CMOS_FAST_BOOT						0x3f

#define BASE_MEMORY_IN_K  640

typedef struct _CMOS_STATE {
	BYTE Registers[256];
	BYTE CmosAddress;
	BYTE CmosExtAddress;
	BYTE    timeval_change;
	BYTE    rtc_mode_12hour;
	BYTE    rtc_mode_binary;
	BYTE    rtc_sync;
	BYTE irq_enabled;
	time_t    timeval;
	DWORD PeriodicIntervalUsec;

} CMOS_STATE;

VOID CmosInitialize();
ULONG CmosPortIoReadHandler(ULONG64 Address, ULONG Length);
VOID CmosPortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length);
VOID CmosSetRegister(BYTE Address, BYTE Value);
BYTE CmosGetRegister(BYTE Address);
VOID CmosSetupMemory(SIZE_T MemorySize);
VOID CmosCheckSum(VOID);
#endif
