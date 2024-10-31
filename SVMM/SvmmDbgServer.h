#pragma once
#include <Windows.h>

#define DBG_MAGIC	0x11223344

#define DBG_TYPE_READ_REGISTERS			0
#define DBG_TYPE_WRITE_REGISTERS		1
#define DBG_TYPE_READ_MEMORY			2
#define DBG_TYPE_WRITE_MEMORY			3
#define DBG_TYPE_STEP_IN				4
#define DBG_TYPE_STEP_OUT				5
#define DBG_TYPE_INIT_REQ    			6
#define DBG_TYPE_INIT_REP				7
#define DBG_TYPE_STEP_INTO				8
#define DBG_TYPE_STEP_OVER				9
#define DBG_TYPE_RUN					10
#define DBG_TYPE_ON_BREAK				11
#define DBG_TYPE_PRINT					12
#define DBG_TYPE_READ_MSR_REGISTERS		13
#define DBG_TYPE_WRITE_MSR_REGISTERS	14
#define DBG_TYPE_STATE_CHANGE_REQ		15
#define DBG_TYPE_STATE_CHANGE_REP       16

#define DBG_MAX_PACKET_DATA_SIZE	2048


#define EFLAGS_CF_MASK 0x00000001       // carry flag
#define EFLAGS_PF_MASK 0x00000004       // parity flag
#define EFLAGS_AF_MASK 0x00000010       // auxiliary carry flag
#define EFLAGS_ZF_MASK 0x00000040       // zero flag
#define EFLAGS_SF_MASK 0x00000080       // sign flag
#define EFLAGS_TF_MASK 0x00000100       // trap flag
#define EFLAGS_IF_MASK 0x00000200       // interrupt flag
#define EFLAGS_DF_MASK 0x00000400       // direction flag
#define EFLAGS_OF_MASK 0x00000800       // overflow flag
#define EFLAGS_IOPL_MASK 0x00003000     // I/O privilege level
#define EFLAGS_NT_MASK 0x00004000       // nested task
#define EFLAGS_RF_MASK 0x00010000       // resume flag
#define EFLAGS_VM_MASK 0x00020000       // virtual 8086 mode
#define EFLAGS_AC_MASK 0x00040000       // alignment check
#define EFLAGS_VIF_MASK 0x00080000      // virtual interrupt flag
#define EFLAGS_VIP_MASK 0x00100000      // virtual interrupt pending
#define EFLAGS_ID_MASK 0x00200000       // identification flag

#define DR1_ENABLED	(1 << 1)
#define DR2_ENABLED	(1 << 3)
#define DR3_ENABLED	(1 << 5)
#define DR4_ENABLED	(1 << 7)

#define DR6_RESERVED	(0xFFFF0FF0)


__pragma(pack(push, 1))
typedef struct _DBG_PACKET_HEADER {
	ULONG Magic;
	USHORT Type;
	ULONG Id;
	ULONG Checksum;
	ULONG Size;
	BYTE Data[0];
} DBG_PACKET_HEADER, *PDBG_PACKET_HEADER;

typedef struct _DBG_PACKET_READ_REQUEST {
	ULONG64 Addr;
	ULONG64 Size;
} DBG_PACKET_READ_REQUEST, * PDBG_PACKET_READ_REQUEST;

typedef struct _DBG_PACKET_WRITE_REQUEST {
	ULONG64 Addr;
	ULONG64 Size;
} DBG_PACKET_WRITE_REQUEST, * PDBG_PACKET_WRITE_REQUEST;

typedef struct _DBG_PACKET_INIT_REQUEST {
	ULONG Id;
} DBG_PACKET_INIT_REQUEST, * PDBG_PACKET_INIT_REQUEST;

typedef struct _DBG_PACKET_INIT_REPLY {
	ULONG Id;
	ULONG64 MemorySize;
} DBG_PACKET_INIT_REPLY, * PDBG_PACKET_INIT_REPLY;

typedef struct _DBG_PACKET_ACK_REQUEST {
	ULONG Id;
} DBG_PACKET_ACK_REQUEST, * PDBG_PACKET_ACK_REQUEST;

typedef struct _DBG_PACKET_ACK_REPLY {
	ULONG Id;
} DBG_PACKET_ACK_REPLY, * PDBG_PACKET_ACK_REPLY;


/*
typedef struct _DBG_PACKET_WRITE_REGISTERS_REQUEST {

} DBG_PACKET_WRITE_REGISTERS_REQUEST, * PDBG_PACKET_WRITE_REGISTERS_REQUEST;
*/
/*
typedef struct _DBG_PACKET_READ_REGISTERS_REQUEST {
} DBG_PACKET_READ_REGISTERS_REQUEST, * PDBG_PACKET_READ_REGISTERS_REQUEST;
*/

typedef struct _DBG_PACKET_PRINT_REQUEST {
	ULONG64 Size;
	CHAR Message[1];
} DBG_PACKET_PRINT_REQUEST, * PDBG_PACKET_PRINT_REQUEST;

__pragma(pack(pop))

VOID SvmmDbgInit(PCSTR DbgCommandLine);
BYTE SvmmDbgLoop();
LONG SvmmDbgSend(USHORT Type, BYTE* Buffer, SIZE_T Size);
LONG SvmmDbgRecv(USHORT Type, BYTE* Buffer, SIZE_T Size);
