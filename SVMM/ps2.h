#ifndef PS2_H
#define PS2_H
#include <Windows.h>


/* Status Register */
#define STATUS_OUTPUT_KB_BUFFER_FULL			(1 << 0)
#define STATUS_INPUT_BUFFER_FULL				(1 << 1)
#define STATUS_SYSTEM_TEST						(1 << 2)
#define STATUS_COMMAND_LAST						(1 << 3)
#define STATUS_TRANSMIT_TIMEOUT_ERROR			(1 << 5)
#define STATUS_OUTPUT_MS_BUFFER_FULL			(1 << 5)
#define STATUS_RECEIVE_TIMEOUT_ERROR			(1 << 6)
#define STATUS_PARITY_ERROR						(1 << 7)

/* Data Register */
#define DATA_KB_INTERRUPT				(1 << 0)	//IRQ1
#define DATA_MS_INTERRUPT				(1 << 1)	//IRQ12
#define DATA_SYSTEM_TEST				(1 << 2)
#define DATA_NO_KEYLOCK					(1 << 3)
#define DATA_DISABLE_KEYBOARD			(1 << 4)
#define DATA_DISABLE_MOUSE				(1 << 5)
#define DATA_SCAN_CODE					(1 << 6)

/* Output Port */
#define OUTPUT_PORT_RESET				(1 << 0)
#define OUTPUT_PORT_A20					(1 << 1)
#define OUTPUT_PORT_KB_OBF				(1 << 4)
#define OUTPUT_PORT_MOUSE_OBF			(1 << 5)


/* Controller Command Register */
#define CMD_READ_BYTE						0x20
#define CMD_READ_NBYTE						0x21
#define CMD_WRITE_BYTE						0x60
#define CMD_WRITE_NBYTE						0x61
#define CMD_DISABLE_2ND_PS2PORT				0xA7
#define CMD_ENABLE_2ND_PS2PORT				0xA8
#define CMD_TEST_2ND_PS2PORT				0xA9
#define CMD_TEST_CONTROLLER					0xAA
#define CMD_TEST_1ST_PS2PORT				0xAB
#define CMD_DISABLE_1ST_PS2PORT				0xAD
#define CMD_ENABLE_1ST_PS2PORT				0xAE
#define CMD_READ_INPUT_PORT					0xC0
#define CMD_READ_OUTPUT_PORT				0xD0
#define CMD_WRITE_OUTPUT_PORT				0xD1
#define CMD_WRITE_KB_OBUF					0xD2
#define CMD_WRITE_MS_OBUF					0xD3
#define CMD_WRITE_MOUSE						0xD4
#define CMD_DISABLE_A20						0xDD
#define CMD_ENABLE_A20						0xDF

/* Keyboard Commands */
#define CMD_WRITE_MS_OBUF					0xD3
#define KBD_CMD_SET_LEDS					0xED
#define KBD_CMD_ECHO						0xEE
#define KBD_CMD_SCANCODE					0xF0
#define KBD_CMD_GET_ID						0xF2
#define KBD_CMD_SET_RATE					0xF3
#define KBD_CMD_ENABLE						0xF4
#define KBD_CMD_RESET_DISABLE				0xF5
#define KBD_CMD_RESET_ENABLE				0xF6
#define KBD_CMD_SET_TYPEMATIC				0xFA
#define KBD_CMD_SET_MAKE_BREAK				0xFC
#define KBD_CMD_RESET						0xFF

/* Mouse Commands */
#define AUX_SET_SCALE11		0xE6	/* Set 1:1 scaling */
#define AUX_SET_SCALE21		0xE7	/* Set 2:1 scaling */
#define AUX_SET_RES			0xE8	/* Set resolution */
#define AUX_GET_SCALE		0xE9	/* Get scaling factor */
#define AUX_SET_STREAM		0xEA	/* Set stream mode */
#define AUX_POLL			0xEB	/* Poll */
#define AUX_RESET_WRAP		0xEC	/* Reset wrap mode */
#define AUX_SET_WRAP		0xEE	/* Set wrap mode */
#define AUX_SET_REMOTE		0xF0	/* Set remote mode */
#define AUX_GET_TYPE		0xF2	/* Get type */
#define AUX_SET_SAMPLE		0xF3	/* Set sample rate */
#define AUX_ENABLE_DEV		0xF4	/* Enable aux device */
#define AUX_DISABLE_DEV		0xF5	/* Disable aux device */
#define AUX_SET_DEFAULT		0xF6
#define AUX_RESET			0xFF	/* Reset aux device */
#define AUX_ACK				0xFA	/* Command byte ACK. */

/* Keyboard Replies */
#define KBD_REPLY_POR		0xAA	/* Power on reset */
#define KBD_REPLY_ID		0xAB	/* Keyboard ID */
#define KBD_REPLY_ACK		0xFA	/* Command ACK */
#define KBD_REPLY_RESEND	0xFE	/* Command NACK, send the cmd again */

#define BX_KBD_XT_TYPE        0
#define BX_KBD_AT_TYPE        1
#define BX_KBD_MF_TYPE        2


#define KBD_CONTROLLER_QSIZE			5					
#define KEYBOARD_DEVICE					0
#define MOUSE_DEVICE					1
#define DATA_REGISTER					0x60
#define COMMAND_REGISTER				0x64
#define STATUS_REGISTER					0x64

#define KEYBOARD_MEMORY_SIZE			16
#define MOUSE_MEMORY_SIZE				48

typedef struct _KEYBOARD {
	BYTE KeyboardMemory[KEYBOARD_MEMORY_SIZE];
	BYTE KeyboardMemoryIndex;
	BYTE KeyboardClockEnabled;
	BYTE ScanCodeEnabled;
	BYTE ScanCodeSet;
	BYTE LedState;
	BYTE Typematic;
} KEYBOARD, *PKEYBOARD;

typedef struct _MOUSE {
	BYTE MouseMemory[MOUSE_MEMORY_SIZE];
	BYTE MouseMemoryIndex;
	BYTE MouseClockEnabled;
} MOUSE, *PMOUSE;

typedef struct _PS2 {
	BYTE DataRegister;			//Read/Write 0x60 (Mode Register)
	BYTE CommandRegister;		//Write 0x64
	BYTE StatusRegister;		//Read 0x64
	BYTE ControllerQueueIndex;		
	BYTE ControllerQueue[KBD_CONTROLLER_QSIZE];
	DWORD TimerPending;
	BYTE KeyboardBuffer;
	BYTE MouseBuffer;
	BYTE CommandBuffer;
	BYTE OutputPort;
	KEYBOARD Keyboard;
	MOUSE Mouse;
} PS2, PPS2;

VOID Ps2Initialize();
ULONG Ps2PortIoReadHandler(ULONG64 Address, ULONG Length);
VOID Ps2PortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length);
VOID Ps2TimerHandler(VOID);
ULONG Ps2TimerPeriodic(DWORD Seconds);
VOID Ps2QueueByte(BYTE Data, BYTE Device);
VOID Ps2ActivateTimer(VOID);
VOID Ps2DeactivateTimer(VOID);

#endif