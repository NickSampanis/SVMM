#include "cmos.h"
#include "pci.h"
#include "pic.h"

CMOS_STATE Cmos;

VOID CmosInitialize()
{
	DWORD i;

	memset(&Cmos, '\0', sizeof(Cmos));
	for (i = 0x70; i <= 0x71; i++)
		RegisterPortIoHandler(i, (WritePortIoHandlerCallback)CmosPortIoWriteHandler, (ReadPortIoHandlerCallback)CmosPortIoReadHandler);
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
            return Cmos.Registers[Cmos.CmosAddress];
			break;
        case 0x73:
            return Cmos.Registers[Cmos.CmosExtAddress];
            break;
	}

	return 0;
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
                /*
                if (Cmos.CmosAddress == REG_IBM_PS2_CENTURY_BYTE) {
                    Cmos.Registers[REG_IBM_CENTURY_BYTE] = Value;
                }
                if (Cmos.Registers[REG_STAT_B] & 0x80) {
                    BX_CMOS_THIS s.timeval_change = 1;
                }
                else {
                    update_timeval();
                }
                */
                break;

            case REG_STAT_A: // Control Register A

                Cmos.Registers[REG_STAT_A] &= 0x80;
                Cmos.Registers[REG_STAT_A] |= (Value & 0x7f);
                //BX_CMOS_THIS CRA_change();
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
                    /*
                    BX_CMOS_THIS s.rtc_mode_12hour = ((Value & 0x02) == 0);
                    update_clock();
                    */
                }
                if ((prev_CRB & 0x04) != (Value & 0x04)) {
                    /*
                    BX_CMOS_THIS s.rtc_mode_binary = ((Value & 0x04) != 0);
                    update_clock();
                    */
                }
                if ((prev_CRB & 0x40) != (Value & 0x40)) {
                    // Periodic Interrupt Enabled changed
                    if (prev_CRB & 0x40) {
                        // transition from 1 to 0, deactivate timer
                        //bx_pc_system.deactivate_timer(BX_CMOS_THIS s.periodic_timer_index);
                    }
                    else {
                        // transition from 0 to 1
                        // if rate select is not 0, activate timer
                        if ((Cmos.Registers[REG_STAT_A] & 0x0f) != 0) {
                            /*
                            bx_pc_system.activate_timer(
                                BX_CMOS_THIS s.periodic_timer_index,
                                BX_CMOS_THIS s.periodic_interval_usec, 1);
                                */
                        }
                    }
                }
                /*
                if ((prev_CRB >= 0x80) && (Value < 0x80) && BX_CMOS_THIS s.timeval_change) {
                    update_timeval();
                    BX_CMOS_THIS s.timeval_change = 0;
                }
                */
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
