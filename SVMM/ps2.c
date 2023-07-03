#include <stdio.h>
#include "ps2.h"
#include "pci.h"
#include "timer.h"
#include "pic.h"

PS2 Ps2;

VOID Ps2Initialize()
{
	memset(&Ps2, '\0', sizeof(Ps2));
	RegisterPortIoHandler(0x60, (WritePortIoHandlerCallback)Ps2PortIoWriteHandler, (ReadPortIoHandlerCallback)Ps2PortIoReadHandler);
	RegisterPortIoHandler(0x64, (WritePortIoHandlerCallback)Ps2PortIoWriteHandler, (ReadPortIoHandlerCallback)Ps2PortIoReadHandler);
	TimerRegister(0x034fb5e3 * 6, Ps2TimerHandler);
}

VOID Ps2EnableMouseClock(BYTE Value)
{
	if (!Value) {
		Ps2.Mouse.MouseClockEnabled = 0;
	}
	else {
		if (!(Ps2.StatusRegister & STATUS_OUTPUT_KB_BUFFER_FULL) && !Ps2.Mouse.MouseClockEnabled)
			Ps2ActivateTimer();
		Ps2.Mouse.MouseClockEnabled = 1;
	}
}

VOID Ps2EnableKeyboardClock(BYTE Value)
{
	if (!Value) {
		Ps2.Keyboard.KeyboardClockEnabled = 0;
	}
	else {
		if (!(Ps2.StatusRegister & STATUS_OUTPUT_KB_BUFFER_FULL) && !Ps2.Keyboard.KeyboardClockEnabled)
			Ps2ActivateTimer();
		Ps2.Keyboard.KeyboardClockEnabled = 1;
	}
}


VOID Ps2QueueByte(BYTE Data, BYTE Device)
{
	if (Ps2.StatusRegister & STATUS_OUTPUT_KB_BUFFER_FULL) {
		if (Ps2.ControllerQueueIndex >= KBD_CONTROLLER_QSIZE) {
			fprintf(stderr, "Error Ps2.ControllerQueueIndex > 5\n");
			exit(-1);
		}
		Ps2.ControllerQueue[Ps2.ControllerQueueIndex++] = Data;
		return;
	}
	if (Device == KEYBOARD_DEVICE) {
		Ps2.StatusRegister |= STATUS_OUTPUT_KB_BUFFER_FULL;
		Ps2.StatusRegister &= ~STATUS_INPUT_BUFFER_FULL;
		Ps2.StatusRegister &= ~STATUS_OUTPUT_MS_BUFFER_FULL;
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

VOID Ps2ActivateTimer(VOID)
{
	if (!Ps2.TimerPending)
		Ps2.TimerPending = 1;
}

VOID Ps2DeactivateTimer(VOID)
{
	if (Ps2.TimerPending)
		Ps2.TimerPending = 0;
}

ULONG Ps2PortIoReadHandler(ULONG64 Address, ULONG Length)
{
	ULONG Ret;

	Ret = 0;
	switch (Address & 0xff) {
		case DATA_REGISTER:
			if (Ps2.StatusRegister & STATUS_OUTPUT_MS_BUFFER_FULL) {
				Ret = Ps2.MouseBuffer;
				Ps2.StatusRegister &= ~STATUS_OUTPUT_MS_BUFFER_FULL;
				Ps2.StatusRegister &= ~STATUS_OUTPUT_KB_BUFFER_FULL;
				Ps2.StatusRegister &= ~STATUS_INPUT_BUFFER_FULL;
				Ps2.DataRegister &= ~DATA_MS_INTERRUPT;
				Ps2.MouseBuffer = 0;
				if (Ps2.ControllerQueueIndex) {
					Ps2.MouseBuffer = Ps2.ControllerQueue[0];
					Ps2.ControllerQueueIndex--;
					Ps2.StatusRegister |= STATUS_OUTPUT_KB_BUFFER_FULL;
					Ps2.StatusRegister |= STATUS_OUTPUT_MS_BUFFER_FULL;
					Ps2.DataRegister |= DATA_MS_INTERRUPT;
					memcpy(Ps2.ControllerQueue, Ps2.ControllerQueue + 1, Ps2.ControllerQueueIndex);
				}
				PicLowerIrq(12);
				Ps2ActivateTimer();
			}
			else if (Ps2.StatusRegister & STATUS_OUTPUT_KB_BUFFER_FULL) {
				Ret = Ps2.KeyboardBuffer;
				Ps2.StatusRegister &= ~STATUS_OUTPUT_KB_BUFFER_FULL;
				Ps2.StatusRegister &= ~STATUS_INPUT_BUFFER_FULL;
				Ps2.StatusRegister &= ~STATUS_OUTPUT_MS_BUFFER_FULL;
				Ps2.DataRegister &= ~DATA_KB_INTERRUPT;
				Ps2.KeyboardBuffer = 0;
				if (Ps2.ControllerQueueIndex) {
					Ps2.KeyboardBuffer = Ps2.ControllerQueue[0];
					Ps2.ControllerQueueIndex--;
					Ps2.StatusRegister |= STATUS_OUTPUT_KB_BUFFER_FULL;
					Ps2.DataRegister |= DATA_KB_INTERRUPT;
					memcpy(Ps2.ControllerQueue, Ps2.ControllerQueue + 1, Ps2.ControllerQueueIndex);
				}
				PicLowerIrq(1);
				Ps2ActivateTimer();
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



/* Write to Mouse Memory */
VOID Ps2WriteMouse(BYTE ScanCode)
{
	if (MOUSE_MEMORY_SIZE <= Ps2.Mouse.MouseMemoryIndex) {
		fprintf(stderr, "mouse internal bufffer is Full\n");
		exit(-1);
	}
	Ps2.Mouse.MouseMemory[Ps2.Mouse.MouseMemoryIndex++] = ScanCode;
	Ps2EnableKeyboardClock(1);

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

BYTE Ps2ReadKeyboard(VOID)
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
VOID Ps2WriteKeyboard(BYTE ScanCode)
{
	if (KEYBOARD_MEMORY_SIZE <= Ps2.Keyboard.KeyboardMemoryIndex) {
		fprintf(stderr, "KeyboardMemory is Full %d\n", Ps2.Keyboard.KeyboardMemoryIndex);
		exit(-1);
	}
	Ps2.Keyboard.KeyboardMemory[Ps2.Keyboard.KeyboardMemoryIndex++] = ScanCode;
	//TODO: Clock Enable
	if (!(Ps2.StatusRegister & STATUS_OUTPUT_KB_BUFFER_FULL) && Ps2.Keyboard.KeyboardClockEnabled)
		Ps2EnableKeyboardClock(1);
}

VOID KeyboardReset(VOID)
{
	memset(Ps2.Keyboard.KeyboardMemory, '\0', sizeof(Ps2.Keyboard.KeyboardMemory));
	Ps2.Keyboard.KeyboardMemoryIndex = 0;
	Ps2.Keyboard.Typematic = 0;
	Ps2.Keyboard.ScanCodeEnabled = 1;
	Ps2.Keyboard.ScanCodeSet = 0;
	Ps2.Keyboard.LedState = 0;
	Ps2.Keyboard.Typematic = 0;

}

VOID Ps2SendKeyboard(BYTE Value)
{
	switch (Value) {
		case 0x05:
			break;
		case KBD_CMD_SET_LEDS:
			Ps2QueueByte(KBD_REPLY_ACK, KEYBOARD_DEVICE);
			break;
		case KBD_CMD_ECHO:
			Ps2WriteKeyboard(0xEE);
			break;
		case KBD_CMD_SCANCODE:
			Ps2WriteKeyboard(KBD_REPLY_ACK);
			break;
		case KBD_CMD_GET_ID:

			break;
		case KBD_CMD_SET_RATE:
			Ps2WriteKeyboard(KBD_REPLY_ACK);
			break;
		case KBD_CMD_ENABLE:

			Ps2WriteKeyboard(KBD_REPLY_ACK);
			break;
		case KBD_CMD_RESET_DISABLE:
			//TODO: Reset internals
			KeyboardReset();
			Ps2WriteKeyboard(KBD_REPLY_ACK);
			break;
		case KBD_CMD_RESET_ENABLE:
			//TODO: Reset internals
			Ps2WriteKeyboard(KBD_REPLY_ACK);
			break;
		case KBD_CMD_RESET:
			//TODO: Reset internals
			Ps2WriteKeyboard(KBD_REPLY_ACK);
			Ps2WriteKeyboard(KBD_REPLY_POR);
			break;
		case CMD_WRITE_MS_OBUF:
			Ps2WriteKeyboard(KBD_REPLY_ACK);
			break;
		case 0xf7:  // PS/2 Set All Keys To Typematic
		case 0xf8:  // PS/2 Set All Keys to Make/Break
		case 0xf9:  // PS/2 PS/2 Set All Keys to Make
		case 0xfa:  // PS/2 Set All Keys to Typematic Make/Break
		case 0xfb:  // PS/2 Set Key Type to Typematic
		case 0xfc:  // PS/2 Set Key Type to Make/Break
		case 0xfd:  // PS/2 Set Key Type to Make
		default:
			Ps2WriteKeyboard(KBD_REPLY_RESEND);
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

VOID Ps2TimerHandler(VOID)
{
	ULONG Ret;

	Ret = Ps2TimerPeriodic(1);
	if (Ret & DATA_KB_INTERRUPT)
		PicRaiseIrq(1);
	if (Ret & DATA_MS_INTERRUPT)
		PicRaiseIrq(12);
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
		Ps2.KeyboardBuffer = Ps2ReadKeyboard();
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
