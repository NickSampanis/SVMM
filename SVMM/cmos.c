#define _CRT_SECURE_NO_WARNINGS

#include "cmos.h"
#include "pci.h"
#include "pic.h"
#include "timer.h"
#include <time.h>
#include <stdio.h>

CMOS_STATE Cmos;
ULONG PeriodicTimerIndex;
ULONG UipTimerIndex;
ULONG OneSecondTimerIndex;

static BYTE bcd_to_bin(BYTE value, BYTE is_binary)
{
    if (is_binary)
        return value;
    else
        return ((value >> 4) * 10) + (value & 0x0f);
}

static BYTE bin_to_bcd(BYTE value, BYTE is_binary)
{
    if (is_binary)
        return value;
    else
        return ((value / 10) << 4) | (value % 10);
}


void CmosUpdateClock()
{
    struct tm* time_calendar;
    unsigned year, month, day, century;
    BYTE val_bcd, hour;

    time_calendar = localtime(&Cmos.timeval);

    // update seconds
    Cmos.Registers[REG_SEC] = bin_to_bcd(time_calendar->tm_sec, Cmos.rtc_mode_binary);

    // update minutes
    Cmos.Registers[REG_MIN] = bin_to_bcd(time_calendar->tm_min,Cmos.rtc_mode_binary);

    // update hours
    if (Cmos.rtc_mode_12hour) {
        hour = time_calendar->tm_hour;
        val_bcd = (hour > 11) ? 0x80 : 0x00;
        if (hour > 11) 
            hour -= 12;
        if (hour == 0)
            hour = 12;
        val_bcd |= bin_to_bcd(hour, Cmos.rtc_mode_binary);
        Cmos.Registers[REG_HOUR] = val_bcd;
    }
    else {
        Cmos.Registers[REG_HOUR] = bin_to_bcd(time_calendar->tm_hour, Cmos.rtc_mode_binary);
    }

    // update day of the week
    day = time_calendar->tm_wday + 1; // 0..6 to 1..7
    Cmos.Registers[REG_WEEK_DAY] = bin_to_bcd(day, Cmos.rtc_mode_binary);

    // update day of the month
    day = time_calendar->tm_mday;
    Cmos.Registers[REG_MONTH_DAY] = bin_to_bcd(day, Cmos.rtc_mode_binary);

    // update month
    month = time_calendar->tm_mon + 1;
    Cmos.Registers[REG_MONTH] = bin_to_bcd(month, Cmos.rtc_mode_binary);

    // update year
    year = time_calendar->tm_year % 100;
    Cmos.Registers[REG_YEAR] = bin_to_bcd(year, Cmos.rtc_mode_binary);

    // update century
    century = (time_calendar->tm_year / 100) + 19;
    Cmos.Registers[REG_IBM_CENTURY_BYTE] = bin_to_bcd(century, Cmos.rtc_mode_binary);

    // Raul Hudea pointed out that some bioses also use Registers 0x37 for the
    // century byte.  Tony Heller says this is critical in getting WinXP to run.
    Cmos.Registers[REG_IBM_PS2_CENTURY_BYTE] = Cmos.Registers[REG_IBM_CENTURY_BYTE];
}

VOID CmosUpdateTimeval()
{
    struct tm time_calendar;
    BYTE val_bin, pm_flag;

    // update seconds
    time_calendar.tm_sec = bcd_to_bin(Cmos.Registers[REG_SEC],
        Cmos.rtc_mode_binary);

    // update minutes
    time_calendar.tm_min = bcd_to_bin(Cmos.Registers[REG_MIN],
        Cmos.rtc_mode_binary);

    // update hours
    if (Cmos.rtc_mode_12hour) {
        pm_flag = Cmos.Registers[REG_HOUR] & 0x80;
        val_bin = bcd_to_bin(Cmos.Registers[REG_HOUR] & 0x70,
            Cmos.rtc_mode_binary);
        if ((val_bin < 12) & (pm_flag > 0)) {
            val_bin += 12;
        }
        else if ((val_bin == 12) & (pm_flag == 0)) {
            val_bin = 0;
        }
        time_calendar.tm_hour = val_bin;
    }
    else {
        time_calendar.tm_hour = bcd_to_bin(Cmos.Registers[REG_HOUR],
            Cmos.rtc_mode_binary);
    }

    // update day of the month
    time_calendar.tm_mday = bcd_to_bin(Cmos.Registers[REG_MONTH_DAY],
        Cmos.rtc_mode_binary);

    // update month
    time_calendar.tm_mon = bcd_to_bin(Cmos.Registers[REG_MONTH],
        Cmos.rtc_mode_binary) - 1;

    // update year
    val_bin = bcd_to_bin(Cmos.Registers[REG_IBM_CENTURY_BYTE],
        Cmos.rtc_mode_binary);
    val_bin = (val_bin - 19) * 100;
    val_bin += bcd_to_bin(Cmos.Registers[REG_YEAR],
        Cmos.rtc_mode_binary);
    time_calendar.tm_year = val_bin;

    Cmos.timeval = mktime(&time_calendar);
}

VOID CmosPeriodicTimer()
{
    if (Cmos.Registers[REG_STAT_B] & 0x40) {
        Cmos.Registers[REG_STAT_C] |= 0xc0; // Interrupt Request, Periodic Int
        if (Cmos.InterruptEnabled)
            PicRaiseIrq(RTC_IRQ);
        
    }
}

VOID CmosOneSecondTimer()
{
    //printf("cmos 1 second has passed\n");

    // divider chain reset - RTC stopped
    if ((Cmos.Registers[REG_STAT_A] & 0x60) == 0x60)
        return;

    // update internal time/date buffer
    Cmos.timeval++;

    // Dont update CMOS user copy of time/date if CRB bit7 is 1
    // Nothing else do to
    if (Cmos.Registers[REG_STAT_B] & 0x80)
        return;

    Cmos.Registers[REG_STAT_A] |= 0x80; // set UIP bit

    // UIP timer for updating clock & alarm functions
    TimerActivate(UipTimerIndex, TICK_PERIOD, 0);
}

void CmosUipTimer()
{
    BYTE alarm_match;

    alarm_match = 0;
    CmosUpdateClock();

    // if update interrupts are enabled, trip IRQ 8, and
    // update status register C
    if (Cmos.Registers[REG_STAT_B] & 0x10) {
        Cmos.Registers[REG_STAT_C] |= 0x90; // Interrupt Request, Update Ended
        //Cmos.Registers[REG_STAT_C] = 0x41;
        if (Cmos.InterruptEnabled) 
            PicRaiseIrq(RTC_IRQ);
        
    }

    // compare CMOS user copy of time/date to alarm time/date here
    if (Cmos.Registers[REG_STAT_B] & 0x20) {
        // Alarm interrupts enabled
        alarm_match = 1;
        if ((Cmos.Registers[REG_SEC_ALARM] & 0xc0) != 0xc0) {
            // seconds alarm not in dont care mode
            if (Cmos.Registers[REG_SEC] != Cmos.Registers[REG_SEC_ALARM])
                alarm_match = 0;
        }
        if ((Cmos.Registers[REG_MIN_ALARM] & 0xc0) != 0xc0) {
            // minutes alarm not in dont care mode
            if (Cmos.Registers[REG_MIN] != Cmos.Registers[REG_MIN_ALARM])
                alarm_match = 0;
        }
        if ((Cmos.Registers[REG_HOUR_ALARM] & 0xc0) != 0xc0) {
            // hours alarm not in dont care mode
            if (Cmos.Registers[REG_HOUR] != Cmos.Registers[REG_HOUR_ALARM])
                alarm_match = 0;
        }
        if (alarm_match) {
            Cmos.Registers[REG_STAT_C] |= 0xa0; // Interrupt Request, Alarm Int
            if (Cmos.InterruptEnabled) 
                PicRaiseIrq(RTC_IRQ);
            
        }
    }
    Cmos.Registers[REG_STAT_A] &= 0x7f; // clear UIP bit
}




VOID CmosSetupMemory(SIZE_T MemorySize)
{
    ULONG64 memory_in_k, extended_memory_in_k, extended_memory_in_64k, memory_above_4gb;


    memory_in_k = MemorySize / 1024;
    extended_memory_in_k = memory_in_k > 1024 ? (memory_in_k - 1024) : 0;

    if (extended_memory_in_k > 0xfc00)
        extended_memory_in_k = 0xfc00;

    CmosSetRegister(0x15, (BYTE)BASE_MEMORY_IN_K);
    CmosSetRegister(0x16, (BYTE)(BASE_MEMORY_IN_K >> 8));
    CmosSetRegister(0x17, (BYTE)(extended_memory_in_k & 0xff));
    CmosSetRegister(0x18, (BYTE)((extended_memory_in_k >> 8) & 0xff));
    CmosSetRegister(0x30, (BYTE)(extended_memory_in_k & 0xff));
    CmosSetRegister(0x31, (BYTE)((extended_memory_in_k >> 8) & 0xff));

    extended_memory_in_64k = memory_in_k > 16384 ? (memory_in_k - 16384) / 64 : 0;
    // Limit to 3 GB - 16 MB. PCI Memory Address Space starts at 3 GB.
    if (extended_memory_in_64k > 0xbf00)
        extended_memory_in_64k = 0xbf00;

    CmosSetRegister(0x34, (BYTE)(extended_memory_in_64k & 0xff));
    CmosSetRegister(0x35, (BYTE)((extended_memory_in_64k >> 8) & 0xff));

    memory_above_4gb = (MemorySize > 0x100000000ULL) ? (MemorySize - 0x100000000ULL) : 0;
    if (memory_above_4gb) {
        CmosSetRegister(0x5b, (BYTE)(memory_above_4gb >> 16));
        CmosSetRegister(0x5c, (BYTE)(memory_above_4gb >> 24));
        CmosSetRegister(0x5d, (BYTE)memory_above_4gb >> 32);
    }
    CmosCheckSum();
}

VOID CmosSetRegister(BYTE Address, BYTE Value)
{
    Cmos.Registers[Address] = Value;
}

BYTE CmosGetRegister(BYTE Address)
{
    return Cmos.Registers[Address];
}

ULONG CmosPortIoReadHandler(ULONG64 Address, ULONG Length)
{
	switch (Address) {
		case 0x70:
        case 0x72:
            return 0xff;
			break;
		case 0x71:
            if (Cmos.CmosAddress == REG_STAT_C) {
                Cmos.Registers[REG_STAT_C] = 0x00;
                PicLowerIrq(8);
                
            }
            //printf("cmos[0x%x] = 0x%x\n", Cmos.CmosAddress, Cmos.Registers[Cmos.CmosAddress]);
            return Cmos.Registers[Cmos.CmosAddress];
			break;
        case 0x73:
            return Cmos.Registers[Cmos.CmosExtAddress];
            break;
	}

	return 0;
}

VOID CraChange()
{
    BYTE nibble, dcc;

    // Periodic Interrupt timer
    nibble = Cmos.Registers[REG_STAT_A] & 0x0f;
    dcc = (Cmos.Registers[REG_STAT_A] >> 4) & 0x07;
    if ((nibble == 0) || ((dcc & 0x06) == 0)) {
        // No Periodic Interrupt Rate when 0, deactivate timer
        TimerDeactivate(PeriodicTimerIndex);
        Cmos.PeriodicIntervalUsec = (DWORD)-1; // max value
    }
    else {
        // values 0001b and 0010b are the same as 1000b and 1001b
        if (nibble <= 2)
            nibble += 7;
        Cmos.PeriodicIntervalUsec = (1000000.0L / (32768.0L / (1ULL << (nibble - 1))));
        Cmos.PeriodicIntervalUsec *= TICK_PERIOD;
        // if Periodic Interrupt Enable bit set, activate timer
        if (Cmos.Registers[REG_STAT_B] & 0x40)
            TimerActivate(PeriodicTimerIndex, Cmos.PeriodicIntervalUsec, 0);

        else
            TimerDeactivate(PeriodicTimerIndex);
    }
}

VOID CmosPortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length)
{
	switch (Address) {
		case 0x70:
			Cmos.CmosAddress = Value & 0x7f;
			break;
		case 0x71:
            switch (Cmos.CmosAddress) {
            case REG_SEC_ALARM:             // seconds alarm
            case REG_MIN_ALARM:             // minutes alarm
            case REG_HOUR_ALARM:            // hours alarm
                Cmos.Registers[Cmos.CmosAddress] = Value;
                break;

            case REG_SEC:                   // seconds
            case REG_MIN:                   // minutes
            case REG_HOUR:                  // hours
            case REG_WEEK_DAY:              // day of the week
            case REG_MONTH_DAY:             // day of the month
            case REG_MONTH:                 // month
            case REG_YEAR:                  // year
            case REG_IBM_CENTURY_BYTE:      // century
            case REG_IBM_PS2_CENTURY_BYTE:  // century (PS/2)
                Cmos.Registers[Cmos.CmosAddress] = Value;
                
                if (Cmos.CmosAddress == REG_IBM_PS2_CENTURY_BYTE) {
                    Cmos.Registers[REG_IBM_CENTURY_BYTE] = Value;
                }
                if (Cmos.Registers[REG_STAT_B] & 0x80) {
                    Cmos.timeval_change = 1;
                }
                else {
                    CmosUpdateTimeval();
                }
                
                break;

            case REG_STAT_A: // Control Register A

                Cmos.Registers[REG_STAT_A] &= 0x80;
                Cmos.Registers[REG_STAT_A] |= (Value & 0x7f);
                CraChange();
                break;

            case REG_STAT_B:

                Value &= 0xf7; // bit3 always 0
                // Note: setting bit 7 clears bit 4
                if (Value & 0x80)
                    Value &= 0xef;

                unsigned prev_CRB;
                prev_CRB = Cmos.Registers[REG_STAT_B];
                Cmos.Registers[REG_STAT_B] = Value;
                if ((prev_CRB & 0x02) != (Value & 0x02)) {
                    
                    Cmos.rtc_mode_12hour = ((Value & 0x02) == 0);
                    CmosUpdateClock();
                    
                }
                if ((prev_CRB & 0x04) != (Value & 0x04)) {
                    
                    Cmos.rtc_mode_binary = ((Value & 0x04) != 0);
                    CmosUpdateClock();
                    
                }
                if ((prev_CRB & 0x40) != (Value & 0x40)) {
                    // Periodic Interrupt Enabled changed
                    if (prev_CRB & 0x40) {
                        // transition from 1 to 0, deactivate timer
                        TimerDeactivate(PeriodicTimerIndex);
                    }
                    else {
                        // transition from 0 to 1
                        // if rate select is not 0, activate timer
                        if ((Cmos.Registers[REG_STAT_A] & 0x0f) != 0) {
                            TimerActivate(PeriodicTimerIndex, Cmos.PeriodicIntervalUsec, 0);
                        }
                    }
                }
                
                if ((prev_CRB >= 0x80) && (Value < 0x80) && Cmos.timeval_change) {
                    CmosUpdateTimeval();
                    Cmos.timeval_change = 0;
                }
                
                break;


            case REG_DIAGNOSTIC_STATUS:
                Cmos.Registers[REG_DIAGNOSTIC_STATUS] = Value;
                break;

            case REG_SHUTDOWN_STATUS:

                Cmos.Registers[REG_SHUTDOWN_STATUS] = Value;
                break;
            }
			break;
		case 0x72:
			Cmos.CmosExtAddress = Value | 0x80;
			break;

	}
}

VOID CmosCheckSum(VOID)
{
    WORD sum = 0;
    for (unsigned i = 0x10; i <= 0x2d; i++)
        sum += Cmos.Registers[i];

    Cmos.Registers[REG_CSUM_HIGH] = (sum >> 8) & 0xff; /* checksum high */
    Cmos.Registers[REG_CSUM_LOW] = (sum & 0xff);      /* checksum low */
}

VOID CmosSetInterrupt(BYTE Enable)
{
    Cmos.InterruptEnabled = Enable;

}


VOID CmosInitialize()
{
    char* tmptime;
    DWORD i;

    memset(&Cmos, '\0', sizeof(Cmos));
    for (i = 0x70; i <= 0x71; i++)
        RegisterPortIoHandler(i, (WritePortIoHandlerCallback)CmosPortIoWriteHandler, (ReadPortIoHandlerCallback)CmosPortIoReadHandler);

    //OneSecondTimerIndex = TimerRegister(TICK_PERIOD * 60, CmosOneSecondTimer, NULL);
    //PeriodicTimerIndex = TimerRegister(TICK_PERIOD * 60, CmosPeriodicTimer, NULL);
    UipTimerIndex = TimerRegister(TICK_PERIOD, CmosUipTimer, NULL);
    TimerDeactivate(UipTimerIndex);

    // CMOS values generated
    Cmos.Registers[REG_STAT_A] = 0x26;
    Cmos.Registers[REG_STAT_B] = 0x02;
    Cmos.Registers[REG_STAT_C] = 0x00;
    Cmos.Registers[REG_STAT_D] = 0x80;
    Cmos.Registers[REG_EQUIPMENT_BYTE] |= 0x02;

    Cmos.rtc_mode_12hour = 0;
    Cmos.rtc_mode_binary = 0;
    Cmos.timeval = time(NULL);

    CmosUpdateClock();
    Cmos.InterruptEnabled = 1;

    while ((tmptime = _strdup(ctime(&(Cmos.timeval)))) == NULL) {
        exit(-1);
    }
    tmptime[strlen(tmptime) - 1] = '\0';
    free(tmptime);

}