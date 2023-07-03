#include "bios.h"
#include "pci.h"
#include <stdio.h>

VOID BiosInitialize()
{
	DWORD i;

	for (i = 0x400; i <= 0x0403; i++)
		RegisterPortIoHandler(i, (WritePortIoHandlerCallback)BiosWriteHandler, (ReadPortIoHandlerCallback)BiosReadHandler);
	for (i = 0x500; i <= 0x0503; i++)
		RegisterPortIoHandler(i, (WritePortIoHandlerCallback)BiosWriteHandler, (ReadPortIoHandlerCallback)BiosReadHandler);

}

ULONG BiosReadHandler(ULONG64 Address, ULONG Length)
{

}

VOID BiosWriteHandler(ULONG64 Address, ULONG Value, ULONG Length)
{
    switch (Address) {
        // 0x400-0x401 are used as fprintf(stderr,  ports for the rombios
    case 0x0401:
        if (Value == 0) {
            // The next message sent to the info port will cause a fprintf(stderr, 
            bios_panic_flag = 1;
            break;
        }
        else if (bios_message_i > 0) {
            // if there are bits of message in the buffer, print them as the
            // fprintf(stderr,  message.  Otherwise fall into the next case.
            if (bios_message_i >= BX_BIOS_MESSAGE_SIZE)
                bios_message_i = BX_BIOS_MESSAGE_SIZE - 1;
            bios_message[bios_message_i] = 0;
            bios_message_i = 0;
            fprintf(stderr, "%s\n", bios_message);
            break;
        }
    case 0x0400:
        if (Value > 0) {
            fprintf(stderr, "BIOS at rombios.c, line %d\n", Value);
        }
        break;

        // 0x0402 is used as the info port for the rombios
        // 0x0403 is used as the debug port for the rombios
    case 0x0402:
    case 0x0403:
        bios_message[bios_message_i] =
            (BYTE)Value;
        bios_message_i++;
        if (bios_message_i >= BX_BIOS_MESSAGE_SIZE) {
            bios_message[BX_BIOS_MESSAGE_SIZE - 1] = 0;
            bios_message_i = 0;
            if (Address == 0x403)
                fprintf(stdout,"%s\n", bios_message);
            else
                fprintf(stdout, "%s\n", bios_message);
        }
        else if ((Value & 0xff) == '\n') {
            bios_message[bios_message_i - 1] = 0;
            bios_message_i = 0;
            if (bios_panic_flag == 1)
                fprintf(stderr, "%s\n", bios_message);
            else if (Address == 0x403)
                fprintf(stdout,"%s\n", bios_message);
            else
                fprintf(stdout, "%s\n", bios_message);
            bios_panic_flag = 0;
        }
        break;

        // 0x501-0x502 are used as fprintf(stderr,  ports for the vgabios
    case 0x0502:
        if (Value == 0) {
            vgabios_panic_flag = 1;
            break;
        }
        else if (vgabios_message_i > 0) {
            // if there are bits of message in the buffer, print them as the
            // fprintf(stderr,  message.  Otherwise fall into the next case.
            if (vgabios_message_i >= BX_BIOS_MESSAGE_SIZE)
                vgabios_message_i = BX_BIOS_MESSAGE_SIZE - 1;
            vgabios_message[vgabios_message_i] = 0;
            vgabios_message_i = 0;
            fprintf(stderr, "%s\n", vgabios_message);
            break;
        }
    case 0x0501:
        if (Value > 0) {
            fprintf(stderr, "VGABIOS at vgabios.c, line %d\n", Value);
        }
        break;

        // 0x0500 is used as the message port for the vgabios
    case 0x0500:
    case 0x0503:
        vgabios_message[vgabios_message_i] =
            (BYTE)Value;
        vgabios_message_i++;
        if (vgabios_message_i >= BX_BIOS_MESSAGE_SIZE) {
            vgabios_message[BX_BIOS_MESSAGE_SIZE - 1] = 0;
            vgabios_message_i = 0;
            if (Address == 0x503)
                fprintf(stdout,"%s\n", vgabios_message);
            else
                fprintf(stdout, "%s\n", vgabios_message);
        }
        else if ((Value & 0xff) == '\n') {
            vgabios_message[vgabios_message_i - 1] = 0;
            vgabios_message_i = 0;
            if (vgabios_panic_flag == 1)
                fprintf(stderr, "%s\n", vgabios_message);
            else if (Address == 0x503)
                fprintf(stdout,"%s\n", vgabios_message);
            else
                fprintf(stdout, "%s\n", vgabios_message);
            vgabios_panic_flag = 0;
        }
        break;

    default:
        break;
    }
}
