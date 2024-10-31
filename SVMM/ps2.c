#include <stdio.h>
#include "ps2.h"
#include "pci.h"
#include "timer.h"
#include "pic.h"
#include "cmos.h"
#include "scancodes.h"


PS2 Ps2;
extern struct SCANCODE scancodes[BX_KEY_NBKEYS][3];
extern unsigned char translation8042[256];

VOID Ps2EnableMouseClock(BYTE Value)
{
	if (!Value) {
		Ps2.Mouse.MouseClockEnabled = 0;
	}
	else {
		if (!(Ps2.StatusRegister & STATUS_OUTPUT_KB_BUFFER_FULL) && !Ps2.Mouse.MouseClockEnabled) {
			if (!Ps2.TimerPending)
				Ps2.TimerPending = 1;
			TimerActivate(Ps2.TimerId, TimerGetClockNs() + NANOSECONDS_PER_MSECOND * 10, 1);
		}
		Ps2.Mouse.MouseClockEnabled = 1;
	}
}

VOID Ps2EnableKeyboardClock(BYTE Enable)
{
	if (Enable) {
		if (!(Ps2.StatusRegister & STATUS_OUTPUT_KB_BUFFER_FULL) && !Ps2.Keyboard.KeyboardClockEnabled) {
			if (!Ps2.TimerPending)
				Ps2.TimerPending = 1;
			//TimerActivate(Ps2.TimerId, TimerGetClockNs() + NANOSECONDS_PER_MSECOND * 10, 1);
			//TimerActivate(Ps2.TimerId, TICK_PERIOD * 1, 1);
		}
		Ps2.Keyboard.KeyboardClockEnabled = 1;
	}
	else
		Ps2.Keyboard.KeyboardClockEnabled = 0;
}


//controller_enQ
VOID Ps2QueueByte(BYTE Data, BYTE Device)
{
	if (Ps2.StatusRegister & STATUS_OUTPUT_KB_BUFFER_FULL) {
		if (Ps2.ControllerQueueIndex >= KBD_CONTROLLER_QSIZE) {
			fprintf(stderr, "Error Ps2.ControllerQueueIndex > 5\n");
			exit(-1);
		}
		Ps2.ControllerQueue[Ps2.ControllerQueueIndex++] = Data;
	}
	else if (Device == KEYBOARD_DEVICE) {
		Ps2.StatusRegister |= STATUS_OUTPUT_KB_BUFFER_FULL;
		Ps2.StatusRegister &= ~(STATUS_INPUT_BUFFER_FULL | STATUS_OUTPUT_MS_BUFFER_FULL);
		Ps2.KeyboardBuffer = Data;
		Ps2.DataRegister |= DATA_KB_INTERRUPT;
	}
	else if (Device == MOUSE_DEVICE) {
		Ps2.StatusRegister |= STATUS_OUTPUT_KB_BUFFER_FULL | STATUS_OUTPUT_MS_BUFFER_FULL;
		Ps2.StatusRegister &= ~STATUS_INPUT_BUFFER_FULL;
		Ps2.MouseBuffer = Data;
		Ps2.DataRegister |= DATA_MS_INTERRUPT;
	}
}


ULONG Ps2PortIoReadHandler(ULONG64 Address, ULONG Length)
{
	ULONG Ret;

	Ret = 0;
	switch (Address & 0xff) {
		case DATA_REGISTER:
			if (Ps2.StatusRegister & STATUS_OUTPUT_MS_BUFFER_FULL) {
				Ret = Ps2.MouseBuffer;
				Ps2.StatusRegister &= ~(STATUS_OUTPUT_MS_BUFFER_FULL | STATUS_OUTPUT_KB_BUFFER_FULL | STATUS_INPUT_BUFFER_FULL);
				Ps2.DataRegister &= ~DATA_MS_INTERRUPT;
				Ps2.MouseBuffer = 0;
				if (Ps2.ControllerQueueIndex) {
					Ps2.MouseBuffer = Ps2.ControllerQueue[0];
					Ps2.StatusRegister |= STATUS_OUTPUT_KB_BUFFER_FULL | STATUS_OUTPUT_MS_BUFFER_FULL;
					Ps2.DataRegister |= DATA_MS_INTERRUPT;
					memcpy(Ps2.ControllerQueue, Ps2.ControllerQueue + 1, --Ps2.ControllerQueueIndex);
				}
				PicLowerIrq(12);
				if (!Ps2.TimerPending)
					Ps2.TimerPending = 1;
				//TimerActivate(Ps2.TimerId, TimerGetClockNs() + NANOSECONDS_PER_MSECOND * 10, 1);
				//TimerActivate(Ps2.TimerId, TICK_PERIOD * 1, 1);
				
			}
			else if (Ps2.StatusRegister & STATUS_OUTPUT_KB_BUFFER_FULL) {
				Ret = Ps2.KeyboardBuffer;
				Ps2.StatusRegister &= ~(STATUS_OUTPUT_KB_BUFFER_FULL | STATUS_INPUT_BUFFER_FULL | STATUS_OUTPUT_MS_BUFFER_FULL);
				Ps2.DataRegister &= ~DATA_KB_INTERRUPT;
				Ps2.KeyboardBuffer = 0;
				if (Ps2.ControllerQueueIndex) {
					//BUG OR NOT?
					Ps2.KeyboardBuffer = Ps2.ControllerQueue[0];
					Ps2.StatusRegister |= STATUS_OUTPUT_KB_BUFFER_FULL;
					Ps2.DataRegister |= DATA_KB_INTERRUPT;
					memcpy(Ps2.ControllerQueue, Ps2.ControllerQueue + 1, --Ps2.ControllerQueueIndex);
				}
				PicLowerIrq(1);
				if (!Ps2.TimerPending)
					Ps2.TimerPending = 1;
				//TimerActivate(Ps2.TimerId, TimerGetClockNs() + NANOSECONDS_PER_MSECOND * 10, 1);
				//TimerActivate(Ps2.TimerId, TICK_PERIOD * 1, 1);

			}
			else {
				Ret = Ps2.KeyboardBuffer;
			}
			break;
		case STATUS_REGISTER:
			Ret = Ps2.StatusRegister;
			Ps2.StatusRegister &= ~(STATUS_RECEIVE_TIMEOUT_ERROR);
			break;
	}

	return Ret;
}


//kbd_enQ
/* Write to Mouse Memory */
VOID Ps2WriteMouseMemory(BYTE ScanCode)
{
	if (MOUSE_MEMORY_SIZE <= Ps2.Mouse.MouseMemoryIndex) {
		fprintf(stderr, "mouse internal bufffer is Full\n");
		exit(-1);
	}
	Ps2.Mouse.MouseMemory[Ps2.Mouse.MouseMemoryIndex++] = ScanCode;

	if (!(Ps2.StatusRegister & STATUS_OUTPUT_KB_BUFFER_FULL) && Ps2.Keyboard.KeyboardClockEnabled
		&& !Ps2.TimerPending) {
		Ps2.TimerPending = 1;
		//TimerActivate(Ps2.TimerId, TICK_PERIOD * 1, 1);
	}
	//Ps2EnableKeyboardClock(1);

}

BYTE Ps2ReadMouse(VOID)
{
	BYTE ret;

	if (MOUSE_MEMORY_SIZE <= Ps2.Mouse.MouseMemoryIndex || !Ps2.Mouse.MouseMemoryIndex) {
		fprintf(stderr, "mouse internal bufffer is empty\n");
		exit(-1);
	}
	ret = Ps2.Mouse.MouseMemory[0];
	memcpy(Ps2.Mouse.MouseMemory, Ps2.Mouse.MouseMemory + 1, --Ps2.Mouse.MouseMemoryIndex);

	return ret;
}

VOID Ps2SendMouse(BYTE Value)
{
	switch (Value) {
		case AUX_SET_SCALE11:
			break;
		case AUX_SET_SCALE21:
			break;
		case AUX_SET_RES:
			break;
	}
}

BYTE Ps2ReadKeyboardMemory(VOID)
{
	BYTE ret;

	if (!Ps2.Keyboard.KeyboardMemoryIndex) {
		fprintf(stderr, "KeyboardMemory is empty\n");
		exit(-1);
	}
	ret = Ps2.Keyboard.KeyboardMemory[0];
	memcpy(Ps2.Keyboard.KeyboardMemory, Ps2.Keyboard.KeyboardMemory + 1, --Ps2.Keyboard.KeyboardMemoryIndex);

	return ret;
}

/* Write to Keyboard Memory */
VOID Ps2WriteKeyboardMemory(BYTE ScanCode)
{
	if (KEYBOARD_MEMORY_SIZE <= Ps2.Keyboard.KeyboardMemoryIndex) {
		fprintf(stderr, "KeyboardMemory is Full %d\n", Ps2.Keyboard.KeyboardMemoryIndex);
		//exit(-1);
		return;
	}
	Ps2.Keyboard.KeyboardMemory[Ps2.Keyboard.KeyboardMemoryIndex++] = ScanCode;
	if (!(Ps2.StatusRegister & STATUS_OUTPUT_KB_BUFFER_FULL) && Ps2.Keyboard.KeyboardClockEnabled
		&& !Ps2.TimerPending) {
		Ps2.TimerPending = 1;
		//TimerActivate(Ps2.TimerId, TICK_PERIOD * 1, 1);
	}
	/*
	if (!(Ps2.StatusRegister & STATUS_OUTPUT_KB_BUFFER_FULL) && Ps2.Keyboard.KeyboardClockEnabled)
		Ps2EnableKeyboardClock(1);
	*/
}

VOID Ps2KeyboardReset(VOID)
{
	memset(Ps2.Keyboard.KeyboardMemory, '\0', sizeof(Ps2.Keyboard.KeyboardMemory));
	Ps2.Keyboard.KeyboardMemoryIndex = 0;
	Ps2.Keyboard.Typematic = 0;
	Ps2.Keyboard.ScanCodeEnabled = 0;
	Ps2.Keyboard.ScanCodeSet = 1;
	Ps2.Keyboard.LedState = 0;
	Ps2.Keyboard.Typematic = 0;
	Ps2.StatusRegister &= ~STATUS_OUTPUT_KB_BUFFER_FULL;
}

//kbd_ctrl_to_kbd
VOID Ps2SendKeyboard(BYTE Value)
{
	
	if (Ps2.Keyboard.ScanCodeEnabled) {
		Ps2.Keyboard.ScanCodeEnabled = 0;
		if (Value) {
			if (Value < 4) {
				Ps2.Keyboard.ScanCodeSet = Value - 1;
				Ps2WriteKeyboardMemory(KBD_REPLY_ACK);
			}
			else 
				Ps2WriteKeyboardMemory(0xFF);
		}
		else {
			Ps2WriteKeyboardMemory(KBD_REPLY_ACK);
			Ps2WriteKeyboardMemory(1 + Ps2.Keyboard.ScanCodeSet);
		}
		return;
	}
	
	switch (Value) {
		case 0x05:
			break;
		case KBD_CMD_SET_LEDS:
			Ps2QueueByte(KBD_REPLY_ACK, KEYBOARD_DEVICE);
			break;
		case KBD_CMD_ECHO:
			Ps2WriteKeyboardMemory(0xEE);
			break;
		case KBD_CMD_SCANCODE:
			Ps2.Keyboard.ScanCodeEnabled = 1;
			Ps2WriteKeyboardMemory(KBD_REPLY_ACK);
			break;
		case KBD_CMD_GET_ID:
			Ps2WriteKeyboardMemory(KBD_REPLY_ACK);
			Ps2WriteKeyboardMemory(KBD_REPLY_ID);
			if (Ps2.DataRegister & DATA_SCAN_CODE) 
				Ps2WriteKeyboardMemory(0x41);
			else 
				Ps2WriteKeyboardMemory(0x83);
			
			break;
		case KBD_CMD_SET_RATE:
			Ps2WriteKeyboardMemory(KBD_REPLY_ACK);
			break;
		case KBD_CMD_ENABLE:

			Ps2WriteKeyboardMemory(KBD_REPLY_ACK);
			break;
		case KBD_CMD_RESET_DISABLE:
			Ps2KeyboardReset();
			Ps2WriteKeyboardMemory(KBD_REPLY_ACK);
			break;
		case KBD_CMD_RESET_ENABLE:
			Ps2KeyboardReset();
			Ps2WriteKeyboardMemory(KBD_REPLY_ACK);
			break;
		case KBD_CMD_RESET:
			
			Ps2KeyboardReset();
			Ps2WriteKeyboardMemory(KBD_REPLY_ACK);
			Ps2WriteKeyboardMemory(KBD_REPLY_POR);
			break;
		case CMD_WRITE_MS_OBUF:
			Ps2WriteKeyboardMemory(KBD_REPLY_ACK);
			break;
		case 0xf7:  // PS/2 Set All Keys To Typematic
		case 0xf8:  // PS/2 Set All Keys to Make/Break
		case 0xf9:  // PS/2 PS/2 Set All Keys to Make
		case 0xfa:  // PS/2 Set All Keys to Typematic Make/Break
		case 0xfb:  // PS/2 Set Key Type to Typematic
		case 0xfc:  // PS/2 Set Key Type to Make/Break
		case 0xfd:  // PS/2 Set Key Type to Make
		default:
			Ps2WriteKeyboardMemory(KBD_REPLY_RESEND);
			break;
	}
}

VOID Ps2PortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length)
{
	switch (Address & 0xff) {
		case DATA_REGISTER:
			if (Ps2.StatusRegister & STATUS_COMMAND_LAST) {
				switch (Ps2.CommandBuffer) {
					case CMD_WRITE_BYTE:
						Ps2.DataRegister = Value & 0xff;
						//TODO: Enable clocks
						Ps2EnableKeyboardClock(!(Ps2.DataRegister & DATA_DISABLE_KEYBOARD));
						Ps2EnableMouseClock(!(Ps2.DataRegister & DATA_DISABLE_MOUSE));
						break;
					case CMD_WRITE_KB_OBUF:
						Ps2QueueByte(Value & 0xff, KEYBOARD_DEVICE);
						break;
					case CMD_WRITE_MS_OBUF:
						Ps2QueueByte(Value & 0xff, MOUSE_DEVICE);
						break;
					case CMD_WRITE_OUTPUT_PORT:
						Ps2.OutputPort = Value & 0xff;
						//TODO: Check Reset

						break;
					case CMD_WRITE_MOUSE:
						//TODO: Send to mouse
						Ps2SendMouse(Value & 0xff);
						break;
				}
			}
			else {
				Ps2SendKeyboard(Value & 0xff);
			}
			Ps2.CommandBuffer = 0;
			Ps2.StatusRegister &= ~STATUS_COMMAND_LAST;
			break;
		case COMMAND_REGISTER:
			switch (Value & 0xff) {
				case CMD_READ_BYTE:
					/* Read mode */
					Ps2QueueByte(Ps2.OutputPort, KEYBOARD_DEVICE);
					break;
				case CMD_READ_OUTPUT_PORT:
					Ps2QueueByte(0x55, KEYBOARD_DEVICE);
					break;
				case CMD_WRITE_BYTE:
				case CMD_WRITE_OUTPUT_PORT:
				case CMD_WRITE_MOUSE:
				case CMD_WRITE_KB_OBUF:
				case CMD_WRITE_MS_OBUF:
					Ps2.StatusRegister |= STATUS_COMMAND_LAST;
					Ps2.CommandBuffer = Value & 0xff;
					break;
				case CMD_DISABLE_2ND_PS2PORT:
					Ps2EnableMouseClock(0);
					//Ps2.DataRegister |= DATA_DISABLE_MOUSE;
					break;
				case CMD_ENABLE_2ND_PS2PORT:
					Ps2EnableMouseClock(1);
					//Ps2.DataRegister &= ~DATA_DISABLE_MOUSE;
					break;
				case CMD_TEST_2ND_PS2PORT:
					if (Ps2.StatusRegister & STATUS_OUTPUT_KB_BUFFER_FULL) {
						fprintf(stderr, "Error CMD_TEST_2ND_PS2PORT output buffer must be empty\n");
						exit(-1);
					}
					Ps2QueueByte(0x00, MOUSE_DEVICE);
					break;
				case CMD_TEST_CONTROLLER:
					if (!(Ps2.StatusRegister & STATUS_SYSTEM_TEST)) {
						//empty output buffer
						Ps2.StatusRegister &= ~STATUS_OUTPUT_KB_BUFFER_FULL;
						Ps2.ControllerQueueIndex = 0;
					}
					//output buffer must be empty
					if (Ps2.StatusRegister & STATUS_OUTPUT_KB_BUFFER_FULL)
						break;
					Ps2.StatusRegister |= STATUS_SYSTEM_TEST;
					Ps2QueueByte(0x55, KEYBOARD_DEVICE);
					break;
				case CMD_TEST_1ST_PS2PORT:
					//output buffer must be empty
					if (Ps2.StatusRegister & STATUS_OUTPUT_KB_BUFFER_FULL) {
						fprintf(stderr, "Error CMD_TEST_1ST_PS2PORT output buffer must be empty\n");
						exit(-1);
					}
					Ps2QueueByte(0x00, KEYBOARD_DEVICE);
					break;
				case CMD_DISABLE_1ST_PS2PORT:
					Ps2EnableKeyboardClock(0);
					//Ps2.DataRegister |= DATA_DISABLE_KEYBOARD;
					break;
				case CMD_ENABLE_1ST_PS2PORT:
					Ps2EnableKeyboardClock(1);
					//Ps2.DataRegister &= ~DATA_DISABLE_KEYBOARD;
					break;
				case CMD_READ_INPUT_PORT:
					Ps2QueueByte(0x80, KEYBOARD_DEVICE);
					break;
				case 0xCA:
					Ps2QueueByte(0x1, KEYBOARD_DEVICE);
					break;
				case CMD_DISABLE_A20:
					Ps2.OutputPort &= ~OUTPUT_PORT_A20;
					break;
				case CMD_ENABLE_A20:
					Ps2.OutputPort |= OUTPUT_PORT_A20;
					break;
			}
			break;
	}
}

VOID Ps2GenerateScancode(ULONG Key)
{
	BYTE* Scancode, Escaped;
	ULONG i;

	if (Key >= 119)
		return;

	if (Key & BX_KEY_RELEASED)
		Scancode = (BYTE*)scancodes[Key][Ps2.Keyboard.ScanCodeSet].Break;
	else
		Scancode = (BYTE*)scancodes[Key][Ps2.Keyboard.ScanCodeSet].Make;

	if (Ps2.DataRegister & DATA_SCAN_CODE) { //scancodes_translate
		Escaped = 0x00;
		for (i = 0; i < strlen((const char*)Scancode); i++) {
			if (Scancode[i] != 0xF0) {
				Ps2WriteKeyboardMemory(translation8042[Scancode[i]] | Escaped);
				Escaped = 0x00;
			}
			else 
				Escaped = 0x80;
			
		}
	}
	else {
		// Send raw data
		for (i = 0; i < strlen((const char*)Scancode); i++)
			Ps2WriteKeyboardMemory(Scancode[i]);
	}
}



ULONG Ps2TimerPeriodic(DWORD Seconds)
{
	ULONG Ret;

	Ret = Ps2.DataRegister & (DATA_MS_INTERRUPT | DATA_KB_INTERRUPT);
	Ps2.DataRegister &= ~(DATA_MS_INTERRUPT | DATA_KB_INTERRUPT);
	if (!Ps2.TimerPending)
		return Ret;
	if (Seconds >= Ps2.TimerPending) {
		Ps2.TimerPending = 0;
	}
	else {
		Ps2.TimerPending -= Seconds;
		return Ret;
	}
	if (Ps2.StatusRegister & STATUS_OUTPUT_KB_BUFFER_FULL)
		return Ret;

	//check if there is something in internal buffer 
	if (Ps2.Keyboard.KeyboardMemoryIndex && Ps2.Keyboard.KeyboardClockEnabled) {
		Ps2.KeyboardBuffer = Ps2ReadKeyboardMemory();
		Ps2.StatusRegister |= STATUS_OUTPUT_KB_BUFFER_FULL;
		Ps2.DataRegister |= DATA_KB_INTERRUPT;
	}
	else {
		//TODO create_mouse_packet
		if (Ps2.Mouse.MouseMemoryIndex && Ps2.Mouse.MouseClockEnabled) {
			Ps2.MouseBuffer = Ps2ReadMouse();
			Ps2.StatusRegister |= STATUS_OUTPUT_KB_BUFFER_FULL;
			Ps2.StatusRegister |= STATUS_OUTPUT_MS_BUFFER_FULL;
			Ps2.DataRegister |= DATA_MS_INTERRUPT;
		}
	}
	return Ret;
}

VOID Ps2TimerHandler(VOID *Param)
{
	ULONG Ret;

	Ret = Ps2TimerPeriodic(1);
	if (Ret & DATA_KB_INTERRUPT)
		PicRaiseIrq(1);
	if (Ret & DATA_MS_INTERRUPT)
		PicRaiseIrq(12);
}

VOID Ps2Initialize()
{
	memset(&Ps2, '\0', sizeof(Ps2));
	//Ps2KeyboardReset();
	RegisterPortIoHandler(0x60, (WritePortIoHandlerCallback)Ps2PortIoWriteHandler, (ReadPortIoHandlerCallback)Ps2PortIoReadHandler);
	RegisterPortIoHandler(0x64, (WritePortIoHandlerCallback)Ps2PortIoWriteHandler, (ReadPortIoHandlerCallback)Ps2PortIoReadHandler);
	//TimerRegister(MSECONDS_TO_NS(10), Ps2TimerHandler, NULL);
	//TimerRegister(TICK_PERIOD, Ps2TimerHandler, NULL);
	//Ps2.TimerId = TimerCreate(Ps2TimerHandler, (VOID *)NULL);
	TimerRegister(TICK_PERIOD * 10, Ps2TimerHandler, NULL);
	CmosSetRegister(REG_EQUIPMENT_BYTE, CmosGetRegister(REG_EQUIPMENT_BYTE) | 0x04);
}