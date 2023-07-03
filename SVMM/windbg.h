#ifndef KD_H
#define KD_H
#include <Windows.h>

#define R_PACKED( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop) )

#ifndef NTSTATUS
#define NTSTATUS LONG
#endif

enum {
	KD_E_OK = 0,
	KD_E_BADCHKSUM = -1,
	KD_E_TIMEOUT = -2,
	KD_E_MALFORMED = -3,
	KD_E_IOERR = -4,
};

enum KD_PACKET_TYPE {
	KD_PACKET_TYPE_UNUSED = 0,
	KD_PACKET_TYPE_STATE_CHANGE32 = 1,
	KD_PACKET_TYPE_STATE_MANIPULATE = 2,
	KD_PACKET_TYPE_DEBUG_IO = 3,
	KD_PACKET_TYPE_ACKNOWLEDGE = 4,
	KD_PACKET_TYPE_RESEND = 5,
	KD_PACKET_TYPE_RESET = 6,
	KD_PACKET_TYPE_STATE_CHANGE64 = 7,
	KD_PACKET_TYPE_POLL_BREAKIN = 8,
	KD_PACKET_TYPE_TRACE_IO = 9,
	KD_PACKET_TYPE_CONTROL_REQUEST = 10,
	KD_PACKET_TYPE_FILE_IO = 11
};

enum KD_PACKET_WAIT_STATE_CHANGE {
	DbgKdMinimumStateChange = 0x00003030,
	DbgKdExceptionStateChange = 0x00003030,
	DbgKdLoadSymbolsStateChange = 0x00003031,
	DbgKdCommandStringStateChange = 0x00003032,
	DbgKdMaximumStateChange = 0x00003033
};

enum KD_PACKET_MANIPULATE_TYPE {
	DbgKdMinimumManipulate = 0x00003130,
	DbgKdReadVirtualMemoryApi = 0x00003130,
	DbgKdWriteVirtualMemoryApi = 0x00003131,
	DbgKdGetContextApi = 0x00003132,
	DbgKdSetContextApi = 0x00003133,
	DbgKdWriteBreakPointApi = 0x00003134,
	DbgKdRestoreBreakPointApi = 0x00003135,
	DbgKdContinueApi = 0x00003136,
	DbgKdReadControlSpaceApi = 0x00003137,
	DbgKdWriteControlSpaceApi = 0x00003138,
	DbgKdReadIoSpaceApi = 0x00003139,
	DbgKdWriteIoSpaceApi = 0x0000313A,
	DbgKdRebootApi = 0x0000313B,
	DbgKdContinueApi2 = 0x0000313C,
	DbgKdReadPhysicalMemoryApi = 0x0000313D,
	DbgKdWritePhysicalMemoryApi = 0x0000313E,
	DbgKdQuerySpecialCallsApi = 0x0000313F,
	DbgKdSetSpecialCallApi = 0x00003140,
	DbgKdClearSpecialCallsApi = 0x00003141,
	DbgKdSetInternalBreakPointApi = 0x00003142,
	DbgKdGetInternalBreakPointApi = 0x00003143,
	DbgKdReadIoSpaceExtendedApi = 0x00003144,
	DbgKdWriteIoSpaceExtendedApi = 0x00003145,
	DbgKdGetVersionApi = 0x00003146,
	DbgKdWriteBreakPointExApi = 0x00003147,
	DbgKdRestoreBreakPointExApi = 0x00003148,
	DbgKdCauseBugCheckApi = 0x00003149,
	DbgKdSwitchProcessor = 0x00003150,
	DbgKdPageInApi = 0x00003151,
	DbgKdReadMachineSpecificRegister = 0x00003152,
	DbgKdWriteMachineSpecificRegister = 0x00003153,
	OldVlm1 = 0x00003154,
	OldVlm2 = 0x00003155,
	DbgKdSearchMemoryApi = 0x00003156,
	DbgKdGetBusDataApi = 0x00003157,
	DbgKdSetBusDataApi = 0x00003158,
	DbgKdCheckLowMemoryApi = 0x00003159,
	DbgKdClearAllInternalBreakpointsApi = 0x0000315A,
	DbgKdFillMemoryApi = 0x0000315B,
	DbgKdQueryMemoryApi = 0x0000315C,
	DbgKdSwitchPartition = 0x0000315D,
	DbgKdMidoriApi = 0x0000315E,
	DbgKdGetContextExApi = 0x0000315F,
	DbgKdSetContextExApi = 0x00003160,
	DbgKdWriteCustomBreakPointApi = 0x00003161,
	DbgKdReadPhysicalMemoryLongApi = 0x00003162,
	DbgKdMaximumManipulate = 0x00003163
};
#define KD_VERSION_SIGNATURE 0x11111
#define KD_VERSION_V2		 0x2

#define KD_PACKET_UNUSED 0x00000000
#define KD_PACKET_BREAK 0x62 
#define KD_PACKET_DATA 0x30303030
#define KD_PACKET_CTRL 0x69696969

#define KD_MAX_PAYLOAD	0x800
#define KD_PACKET_MAX_SIZE 4000 // Not used ? What is max payload ?

#define KDDEBUGGER_DATA64_MAX 0x380

#define COM_DATA_REGISTER           0
#define COM_INTERRUPT_REGISTER		1
#define COM_FIFO_CTRL_REGISTER		2
#define COM_LINE_CTRL_REGISTER		3
#define COM_MODEM_CTRL_REGISTER		4
#define COM_LINE_STATUS_REGISTER    5
#define COM_MODEM_STATUS_REGISTER	6
#define COM_SCRATCH_REGISTER		7

#define COM1_ADDR				0x3f8
#define COM2_ADDR				0x2f8
#define COM3_ADDR				0x3e8
#define COM4_ADDR				0x2e8
#define COM_NUM					4

#define SERIAL_TIMEOUT  (ULONG64)-1
#define SERIAL_INTIME   0

// http://msdn.microsoft.com/en-us/library/cc704588.aspx
#define KD_RET_OK		0x00000000
#define KD_RET_ERR		0xC0000001
#define KD_RET_ENOENT	0xC000000F

#define KD_MACH_I386	 0x014C
#define KD_MACH_IA64	 0x0200
#define KD_MACH_AMD64	 0x8664
#define KD_MACH_ARM		 0x01c0
#define KD_MACH_EBC		 0x0EBC

#define KD_MAX_PACKET_TYPE		0x000C
#define KD_MAX_STATE_CHANGE		0x0003
#define KD_MAX_MANIPULATE		0x0058
#define KD_MAX_PACKET_SIZE		0xFA0

#define DBGKD_VERS_FLAG_DATA	0x0002
#define DBGKD_VERS_FLAG_PTR64	0x0004

#define KD_HEADER_SEQ_NUM 8
#define AES_BLOCK_SIZE	16
#define AES_ENCRYPT 1
#define AES_DECRYPT 0
#define KD_NET_HMAC_SIZE 16
#define KD_NET_KEY_SIZE	32

#define KD_PING_SIZE 12

#define KD_OFFER_MESSAGE 1

#define MAJOR_VERSION			0x0F
#define MINOR_VERSION			0x47BA
#define MAJOR_PROT_VERSION		0x06
#define MINOR_PROT_VERSION		0x02

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

#define PE_OFFSET(KernelBase)   (*(DWORD *)KernelBase + 0x3c)
#define NT_HDR_SIZE             0x18
#define CHKSUM_OFFSET           0x40
#define SIZE_IMAGE_OFFSET       0x38

#define CONTEXT_AMD64			0x00100000L
#define CONTEXT_XSTATE          (CONTEXT_AMD64 | 0x00000040L)



#define BREAKPOINT_TABLE_SIZE   32

#define KDP_NATIVE_BREAKPOINT_VALUE 0xcc


/*
R_PACKED(
	typedef struct kd_req_t {
	uint32_t api_number;
	uint16_t cpu_level;
	uint16_t cpu;
	uint32_t ret;
	// Pad to 16-byte boundary (?)
	uint32_t pad;
	union {
		R_PACKED(
			struct {
			uint64_t addr;
			uint32_t length;
			uint32_t read;
		}) r_mem;
		R_PACKED(
			struct {
			uint16_t major;
			uint16_t minor;
			uint8_t  proto_major;
			uint8_t  proto_minor;
			uint16_t flags;
			uint16_t machine;
			uint16_t max_packet_type;
			uint16_t max_state_change;
			uint16_t max_manipulate;
			uint64_t kernel_base;
			uint64_t mod_list;
			uint64_t dbg_list;
		}) r_ver;
		struct {
			uint32_t reason;
			uint32_t tf;
			uint32_t dr7;
			uint32_t css;
			uint32_t cse;
		} r_cont;
		struct {
			uint64_t addr;
			uint32_t handle;
		} r_set_bp;
		struct {
			uint32_t handle;
		} r_del_bp;
		struct {
			uint64_t addr;
			uint32_t flags;
		} r_set_ibp;
		struct {
			uint64_t addr;
			uint32_t flags;
			uint32_t calls;
		} r_get_ibp;
		struct {
			uint32_t flags;
		} r_ctx;
		struct {
			uint64_t addr;
			uint64_t reserved;
			uint32_t address_space;
			uint32_t flags;
		} r_query_mem;
		R_PACKED(
			struct {
			uint32_t offset;
			uint32_t byte_count;
			uint32_t bytes_copied;
		}) r_ctx_ex;
		// Pad the struct to 56 bytes
		uint8_t raw[40];
	};
	uint8_t data[0];
}) kd_req_t;

#define KD_EXC_BKPT 0x80000003
R_PACKED(
	typedef struct kd_stc_64 {
	uint32_t state;
	uint16_t cpu_level;
	uint16_t cpu;
	uint32_t cpu_count;
	uint32_t pad1;
	uint64_t kthread;
	uint64_t pc;
	union {
		R_PACKED(
			struct {
			uint32_t code;
			uint32_t flags;
			uint64_t ex_record;
			uint64_t ex_addr;
		}) exception;
	};
}) kd_stc_64;
*/

typedef struct _DBGKM_EXCEPTION64 {
	EXCEPTION_RECORD64 ExceptionRecord;
	ULONG FirstChance;
} DBGKM_EXCEPTION64, * PDBGKM_EXCEPTION64;

typedef struct _DBGKD_LOAD_SYMBOLS64 {
	ULONG PathNameLength;
	ULONG64 BaseOfDll;
	ULONG64 ProcessId;
	ULONG CheckSum;
	ULONG SizeOfImage;
	BOOL UnloadSymbols;
} DBGKD_LOAD_SYMBOLS64, * PDBGKD_LOAD_SYMBOLS64;

typedef struct _DBGKD_COMMAND_STRING {
	ULONG Flags;
	ULONG Reserved1;
	ULONG64 Reserved2[7];
} DBGKD_COMMAND_STRING, * PDBGKD_COMMAND_STRING;

#define DBGKD_MAXSTREAM 16


typedef struct _AMD64_DBGKD_CONTROL_REPORT {
	ULONG64 Dr6;
	ULONG64 Dr7;
	ULONG EFlags;
	USHORT InstructionCount;
	USHORT ReportFlags;
	UCHAR InstructionStream[DBGKD_MAXSTREAM];
	USHORT SegCs;
	USHORT SegDs;
	USHORT SegEs;
	USHORT SegFs;
} AMD64_DBGKD_CONTROL_REPORT, * PAMD64_DBGKD_CONTROL_REPORT;


typedef struct DECLSPEC_ALIGN(16) __CONTEXT {

	//
	// Register parameter home addresses.
	//
	// N.B. These fields are for convience - they could be used to extend the
	//      context record in the future.
	//

	DWORD64 P1Home;
	DWORD64 P2Home;
	DWORD64 P3Home;
	DWORD64 P4Home;
	DWORD64 P5Home;
	DWORD64 P6Home;

	//
	// Control flags.
	//

	DWORD ContextFlags;
	DWORD MxCsr;

	//
	// Segment Registers and processor flags.
	//

	WORD   SegCs;
	WORD   SegDs;
	WORD   SegEs;
	WORD   SegFs;
	WORD   SegGs;
	WORD   SegSs;
	DWORD EFlags;

	//
	// Debug registers
	//

	DWORD64 Dr0;
	DWORD64 Dr1;
	DWORD64 Dr2;
	DWORD64 Dr3;
	DWORD64 Dr6;
	DWORD64 Dr7;

	//
	// Integer registers.
	//

	DWORD64 Rax;
	DWORD64 Rcx;
	DWORD64 Rdx;
	DWORD64 Rbx;
	DWORD64 Rsp;
	DWORD64 Rbp;
	DWORD64 Rsi;
	DWORD64 Rdi;
	DWORD64 r8;
	DWORD64 r9;
	DWORD64 r10;
	DWORD64 r11;
	DWORD64 r12;
	DWORD64 r13;
	DWORD64 r14;
	DWORD64 r15;

	//
	// Program counter.
	//

	DWORD64 Rip;

	//
	// Floating point state.
	//

	union {
		XMM_SAVE_AREA32 FltSave;
		struct {
			M128A Header[2];
			M128A Legacy[8];
			M128A Xmm0;
			M128A Xmm1;
			M128A Xmm2;
			M128A Xmm3;
			M128A Xmm4;
			M128A Xmm5;
			M128A Xmm6;
			M128A Xmm7;
			M128A Xmm8;
			M128A Xmm9;
			M128A Xmm10;
			M128A Xmm11;
			M128A Xmm12;
			M128A Xmm13;
			M128A Xmm14;
			M128A Xmm15;
		} DUMMYSTRUCTNAME;
	} DUMMYUNIONNAME;

	//
	// Vector registers.
	//

	M128A VectorRegister[26];
	DWORD64 VectorControl;

	//
	// Special debug control registers.
	//

	DWORD64 DebugControl;
	DWORD64 LastBranchToRip;
	DWORD64 LastBranchFromRip;
	DWORD64 LastExceptionToRip;
	DWORD64 LastExceptionFromRip;
} __CONTEXT, * __PCONTEXT;

typedef struct _DBGKD_ANY_WAIT_STATE_CHANGE {
	ULONG NewState;
	USHORT ProcessorLevel;
	USHORT Processor;
	ULONG NumberProcessors;
	ULONG64 Thread;
	ULONG64 ProgramCounter;
	union {
		DBGKM_EXCEPTION64 Exception;
		DBGKD_LOAD_SYMBOLS64 LoadSymbols;
		DBGKD_COMMAND_STRING CommandString;
	} u;
	// The ANY control report is unioned here to
	// ensure that this structure is always large
	// enough to hold any possible state change.
	AMD64_DBGKD_CONTROL_REPORT ControlReport;
} DBGKD_ANY_WAIT_STATE_CHANGE, * PDBGKD_ANY_WAIT_STATE_CHANGE;


typedef struct _DBGKD_READ_MEMORY64 {
	ULONG64 TargetBaseAddress;
	ULONG TransferCount;
	ULONG ActualBytesRead;
} DBGKD_READ_MEMORY64, * PDBGKD_READ_MEMORY64;

typedef struct _DBGKD_WRITE_MEMORY64 {
	ULONG64 TargetBaseAddress;
	ULONG TransferCount;
	ULONG ActualBytesWritten;
} DBGKD_WRITE_MEMORY64, * PDBGKD_WRITE_MEMORY64;

typedef struct _DBGKD_GET_CONTEXT {
	ULONG Unused;
} DBGKD_GET_CONTEXT, * PDBGKD_GET_CONTEXT;

//
// Full Context record follows
//

typedef struct _DBGKD_SET_CONTEXT {
	ULONG ContextFlags;
} DBGKD_SET_CONTEXT, * PDBGKD_SET_CONTEXT;

typedef struct _DBGKD_CONTEXT_EX {
	ULONG Offset;
	ULONG ByteCount;
	ULONG BytesCopied;
} DBGKD_CONTEXT_EX, * PDBGKD_CONTEXT_EX;

typedef struct _DBGKD_WRITE_BREAKPOINT64 {
	ULONG64 BreakPointAddress;
	ULONG BreakPointHandle;
} DBGKD_WRITE_BREAKPOINT64, * PDBGKD_WRITE_BREAKPOINT64;

typedef struct _DBGKD_WRITE_CUSTOM_BREAKPOINT {
	ULONG64 BreakPointAddress;
	ULONG64 BreakPointInstruction;
	ULONG BreakPointHandle;
	UCHAR BreakPointInstructionSize;
	UCHAR BreakPointInstructionAlignment;
} DBGKD_WRITE_CUSTOM_BREAKPOINT, * PDBGKD_WRITE_CUSTOM_BREAKPOINT;

typedef struct _DBGKD_RESTORE_BREAKPOINT {
	ULONG BreakPointHandle;
} DBGKD_RESTORE_BREAKPOINT, * PDBGKD_RESTORE_BREAKPOINT;

typedef struct _DBGKD_BREAKPOINTEX {
	ULONG     BreakPointCount;
	NTSTATUS  ContinueStatus;
} DBGKD_BREAKPOINTEX, * PDBGKD_BREAKPOINTEX;

typedef struct _DBGKD_CONTINUE {
	NTSTATUS ContinueStatus;
} DBGKD_CONTINUE, * PDBGKD_CONTINUE;

#pragma pack(push,4)
typedef struct _AMD64_DBGKD_CONTROL_SET {
	ULONG   TraceFlag;
	ULONG64 Dr7;
	ULONG64 CurrentSymbolStart;
	ULONG64 CurrentSymbolEnd;
} AMD64_DBGKD_CONTROL_SET, * PAMD64_DBGKD_CONTROL_SET;

typedef struct _DBGKD_CONTINUE2 {
	NTSTATUS ContinueStatus;
	union {
		AMD64_DBGKD_CONTROL_SET ControlSet;
		BYTE Pad[28];
	};
} DBGKD_CONTINUE2, * PDBGKD_CONTINUE2;
#pragma pack(pop)

typedef struct _DBGKD_READ_WRITE_IO64 {
	ULONG64 IoAddress;
	ULONG DataSize;                     // 1, 2, 4
	ULONG DataValue;
} DBGKD_READ_WRITE_IO64, * PDBGKD_READ_WRITE_IO64;

typedef struct _DBGKD_READ_WRITE_IO_EXTENDED64 {
	ULONG DataSize;                     // 1, 2, 4
	ULONG InterfaceType;
	ULONG BusNumber;
	ULONG AddressSpace;
	ULONG64 IoAddress;
	ULONG DataValue;
} DBGKD_READ_WRITE_IO_EXTENDED64, * PDBGKD_READ_WRITE_IO_EXTENDED64;

typedef struct _DBGKD_QUERY_SPECIAL_CALLS {
	ULONG NumberOfSpecialCalls;
	// ULONG64 SpecialCalls[];
} DBGKD_QUERY_SPECIAL_CALLS, * PDBGKD_QUERY_SPECIAL_CALLS;
typedef struct _DBGKD_SET_SPECIAL_CALL64 {
	ULONG64 SpecialCall;
} DBGKD_SET_SPECIAL_CALL64, * PDBGKD_SET_SPECIAL_CALL64;

typedef struct _DBGKD_SET_INTERNAL_BREAKPOINT64 {
	ULONG64 BreakpointAddress;
	ULONG Flags;
} DBGKD_SET_INTERNAL_BREAKPOINT64, * PDBGKD_SET_INTERNAL_BREAKPOINT64;

typedef struct _DBGKD_GET_INTERNAL_BREAKPOINT64 {
	ULONG64 BreakpointAddress;
	ULONG Flags;
	ULONG Calls;
	ULONG MaxCallsPerPeriod;
	ULONG MinInstructions;
	ULONG MaxInstructions;
	ULONG TotalInstructions;
} DBGKD_GET_INTERNAL_BREAKPOINT64, * PDBGKD_GET_INTERNAL_BREAKPOINT64;

struct _DBGKD_GET_VERSION64
{
	USHORT  MajorVersion;
	USHORT  MinorVersion;
	UCHAR   ProtocolVersion;
	UCHAR   KdSecondaryVersion;
	USHORT  Flags;
	USHORT  MachineType;

	// One beyond highest packet type understood, zero based.
	UCHAR   MaxPacketType;
	// One beyond highest state change understood, zero based.
	UCHAR   MaxStateChange;
	// One beyond highest state manipulate message understood, zero based.
	UCHAR   MaxManipulate;

	// Kind of execution environment the kernel is running in,
	// such as a real machine or a simulator.  Written back
	// by the simulation if one exists.
	UCHAR   Simulation;

	USHORT  Unused[1];

	ULONG64 KernBase;
	ULONG64 PsLoadedModuleList;

	//
	// Components may register a debug data block for use by
	// debugger extensions.  This is the address of the list head.
	//
	// There will always be an entry for the debugger.
	//

	ULONG64 DebuggerDataList;

};

typedef struct _DBGKD_GET_VERSION64 DBGKD_GET_VERSION64;
typedef struct _DBGKD_GET_VERSION64* PDBGKD_GET_VERSION64;

typedef struct _DBGKD_READ_WRITE_MSR
{
	ULONG Msr;
	ULONG DataValueLow;
	ULONG DataValueHigh;
} DBGKD_READ_WRITE_MSR, * PDBGKD_READ_WRITE_MSR;

typedef struct _DBGKD_SEARCH_MEMORY
{
	union
	{
		ULONG64 SearchAddress;
		ULONG64 FoundAddress;
	};
	ULONG64 SearchLength;
	ULONG PatternLength;
} DBGKD_SEARCH_MEMORY, * PDBGKD_SEARCH_MEMORY;


typedef struct _DBGKD_GET_SET_BUS_DATA
{
	ULONG BusDataType;
	ULONG BusNumber;
	ULONG SlotNumber;
	ULONG Offset;
	ULONG Length;
} DBGKD_GET_SET_BUS_DATA, * PDBGKD_GET_SET_BUS_DATA;

typedef struct _DBGKD_FILL_MEMORY
{
	ULONG64 Address;
	ULONG Length;
	USHORT Flags;
	USHORT PatternLength;
} DBGKD_FILL_MEMORY, * PDBGKD_FILL_MEMORY;

// Input AddressSpace values.
#define DBGKD_QUERY_MEMORY_VIRTUAL 0x00000000

// Output AddressSpace values.
#define DBGKD_QUERY_MEMORY_PROCESS 0x00000000
#define DBGKD_QUERY_MEMORY_SESSION 0x00000001
#define DBGKD_QUERY_MEMORY_KERNEL  0x00000002

// Output Flags.
// Currently the kernel always returns rwx.
#define DBGKD_QUERY_MEMORY_READ    0x00000001
#define DBGKD_QUERY_MEMORY_WRITE   0x00000002
#define DBGKD_QUERY_MEMORY_EXECUTE 0x00000004
#define DBGKD_QUERY_MEMORY_FIXED   0x00000008

#define XSAVE_ALIGN                64

#define ALIGN_DOWN_POINTER_BY(address, alignment) \
    ((PVOID)((ULONG_PTR)(address) & ~((ULONG_PTR)alignment - 1)))

#define ALIGN_UP_POINTER_BY(address, alignment) \
    (ALIGN_DOWN_POINTER_BY(((ULONG_PTR)(address) + alignment - 1), alignment))

#ifndef FIELD_OFFSET
#define FIELD_OFFSET(type, field)	((ULONG64)&(((type *)0)->field))
#endif

#define AMD64_USER_SHARED_DATA		0xfffff78000000000
#define AMD64_XSAVE_SIZE_OFFSET		0x03e8

#define ALIGN_DOWN_BY(length, alignment) \
    ((ULONG_PTR)(length) & ~((ULONG_PTR)(alignment) - 1))

#define ALIGN_UP_BY(length, alignment) \
    (ALIGN_DOWN_BY(((ULONG_PTR)(length) + (alignment) - 1), alignment))

typedef struct _DBGKD_QUERY_MEMORY {
	ULONG64 Address;
	ULONG64 Reserved;
	ULONG AddressSpace;
	ULONG Flags;
} DBGKD_QUERY_MEMORY, * PDBGKD_QUERY_MEMORY;


#define DBGKD_PARTITION_DEFAULT   0x00000000
#define DBGKD_PARTITION_ALTERNATE 0x00000001

typedef struct _DBGKD_SWITCH_PARTITION {
	ULONG Partition;
} DBGKD_SWITCH_PARTITION;


typedef struct _DBGKD_MANIPULATE_STATE64 {
	ULONG ApiNumber;
	USHORT ProcessorLevel;
	USHORT Processor;
	NTSTATUS ReturnStatus;
	union {
		DBGKD_READ_MEMORY64 ReadMemory;
		DBGKD_WRITE_MEMORY64 WriteMemory;
		DBGKD_GET_CONTEXT GetContext;
		DBGKD_SET_CONTEXT SetContext;
		DBGKD_WRITE_BREAKPOINT64 WriteBreakPoint;
		DBGKD_RESTORE_BREAKPOINT RestoreBreakPoint;
		DBGKD_CONTINUE Continue;
		DBGKD_CONTINUE2 Continue2;
		DBGKD_READ_WRITE_IO64 ReadWriteIo;
		DBGKD_READ_WRITE_IO_EXTENDED64 ReadWriteIoExtended;
		DBGKD_QUERY_SPECIAL_CALLS QuerySpecialCalls;
		DBGKD_SET_SPECIAL_CALL64 SetSpecialCall;
		DBGKD_SET_INTERNAL_BREAKPOINT64 SetInternalBreakpoint;
		DBGKD_GET_INTERNAL_BREAKPOINT64 GetInternalBreakpoint;
		DBGKD_GET_VERSION64 GetVersion64;
		DBGKD_BREAKPOINTEX BreakPointEx;
		DBGKD_READ_WRITE_MSR ReadWriteMsr;
		DBGKD_SEARCH_MEMORY SearchMemory;
		DBGKD_GET_SET_BUS_DATA GetSetBusData;
		DBGKD_FILL_MEMORY FillMemory;
		DBGKD_QUERY_MEMORY QueryMemory;
		DBGKD_SWITCH_PARTITION SwitchPartition;
		DBGKD_CONTEXT_EX GetContextEx;
		DBGKD_CONTEXT_EX SetContextEx;
		DBGKD_WRITE_CUSTOM_BREAKPOINT WriteCustomBreakPoint;
		BYTE RAW[40];
	} u;
} DBGKD_MANIPULATE_STATE64, * PDBGKD_MANIPULATE_STATE64;

/*
typedef struct _XSAVE_FORMAT {
	WORD   ControlWord;
	WORD   StatusWord;
	BYTE  TagWord;
	BYTE  Reserved1;
	WORD   ErrorOpcode;
	DWORD ErrorOffset;
	WORD   ErrorSelector;
	WORD   Reserved2;
	DWORD DataOffset;
	WORD   DataSelector;
	WORD   Reserved3;
	DWORD MxCsr;
	DWORD MxCsr_Mask;
	M128A FloatRegisters[8];
	M128A XmmRegisters[16];
	BYTE  Reserved4[96];
} XSAVE_FORMAT, * PXSAVE_FORMAT;

typedef XSAVE_FORMAT XMM_SAVE_AREA32, * PXMM_SAVE_AREA32;

struct _CONTEXT {

	//
	// Register parameter home addresses.
	//
	// N.B. These fields are for convience - they could be used to extend the
	//      context record in the future.
	//

	DWORD64 P1Home;
	DWORD64 P2Home;
	DWORD64 P3Home;
	DWORD64 P4Home;
	DWORD64 P5Home;
	DWORD64 P6Home;

	//
	// Control flags.
	//

	DWORD ContextFlags;
	DWORD MxCsr;

	//
	// Segment Registers and processor flags.
	//

	WORD   SegCs;
	WORD   SegDs;
	WORD   SegEs;
	WORD   SegFs;
	WORD   SegGs;
	WORD   SegSs;
	DWORD EFlags;

	//
	// Debug registers
	//

	DWORD64 Dr0;
	DWORD64 Dr1;
	DWORD64 Dr2;
	DWORD64 Dr3;
	DWORD64 Dr6;
	DWORD64 Dr7;

	//
	// Integer registers.
	//

	DWORD64 Rax;
	DWORD64 Rcx;
	DWORD64 Rdx;
	DWORD64 Rbx;
	DWORD64 Rsp;
	DWORD64 Rbp;
	DWORD64 Rsi;
	DWORD64 Rdi;
	DWORD64 R8;
	DWORD64 R9;
	DWORD64 R10;
	DWORD64 R11;
	DWORD64 R12;
	DWORD64 R13;
	DWORD64 R14;
	DWORD64 R15;

	//
	// Program counter.
	//

	DWORD64 Rip;

	//
	// Floating point state.
	//

	union {
		XMM_SAVE_AREA32 FltSave;
		struct {
			M128A Header[2];
			M128A Legacy[8];
			M128A Xmm0;
			M128A Xmm1;
			M128A Xmm2;
			M128A Xmm3;
			M128A Xmm4;
			M128A Xmm5;
			M128A Xmm6;
			M128A Xmm7;
			M128A Xmm8;
			M128A Xmm9;
			M128A Xmm10;
			M128A Xmm11;
			M128A Xmm12;
			M128A Xmm13;
			M128A Xmm14;
			M128A Xmm15;
		} DUMMYSTRUCTNAME;
	} DUMMYUNIONNAME;

	//
	// Vector registers.
	//

	M128A VectorRegister[26];
	DWORD64 VectorControl;

	//
	// Special debug control registers.
	//

	DWORD64 DebugControl;
	DWORD64 LastBranchToRip;
	DWORD64 LastBranchFromRip;
	DWORD64 LastExceptionToRip;
	DWORD64 LastExceptionFromRip;
};

typedef struct _CONTEXT CONTEXT;
typedef struct _CONTEXT* PCONTEXT;



#define CHUNK_CONTAINS(Outer, Inner) ( \
	(Outer.Offset <= Inner.Offset) && \
	((LONG)(Inner.Offset + Inner.Length) <= (LONG)(Outer.Offset + Outer.Length)) && \
	(Inner.Offset <= (LONG)(Inner.Offset + Inner.Length)))

typedef struct _XSAVE_AREA_HEADER {
	DWORD64 Mask;
	DWORD64 CompactionMask;
	DWORD64 Reserved2[6];
} XSAVE_AREA_HEADER, * PXSAVE_AREA_HEADER;

typedef struct _XSAVE_AREA {
	XSAVE_FORMAT LegacyState;
	XSAVE_AREA_HEADER Header;
} XSAVE_AREA, * PXSAVE_AREA;

typedef struct _XSTATE_CONTEXT {
	DWORD64 Mask;
	DWORD Length;
	DWORD Reserved1;
	PXSAVE_AREA Area;
	PVOID Buffer;
} XSTATE_CONTEXT, * PXSTATE_CONTEXT;
*/
typedef struct _CONTEXT_CHUNK {
	LONG Offset;
	ULONG Length;
} CONTEXT_CHUNK, * PCONTEXT_CHUNK;

typedef struct _CONTEXT_EX {

	CONTEXT_CHUNK All;
	CONTEXT_CHUNK Legacy;
	CONTEXT_CHUNK XState;

} CONTEXT_EX, * PCONTEXT_EX;
//ct_assert(sizeof(DBGKD_MANIPULATE_STATE64) == sizeof(kd_req))

#define KDNET_MAGIC 0x4742444D  // MDBG
#define KDNET_HMACKEY_SIZE 32
#define KDNET_HMAC_SIZE 16

#define KDNET_PACKET_TYPE_DATA 0
#define KDNET_PACKET_TYPE_CONTROL 1

#define NET_PACKET_MAX_SIZE 1152
#define KDBG_TAG    'GBDK'

__pragma(pack(push, 1))
typedef struct _KD_PACKET {
	ULONG packetLeader;
	USHORT packetType;
	USHORT length;
	ULONG id;
	ULONG checksum;
	BYTE data[0];
} KD_PACKET, * PKD_PACKET;
__pragma(pack(pop))

typedef enum _DEBUG_CONTROL_SPACE_ITEM {
	DEBUG_CONTROL_SPACE_PCR,
	DEBUG_CONTROL_SPACE_PRCB,
	DEBUG_CONTROL_SPACE_KSPECIAL,
	DEBUG_CONTROL_SPACE_THREAD,
	DEBUG_CONTROL_SPACE_MAXIMUM
} DEBUG_CONTROL_SPACE_ITEM;


// KDNet Data mask
#define KDNET_DATA_SIZE 8
#define KDNET_DATA_DIRECTION_MASK 0x80
#define KDNET_DATA_PADSIZE_MASK 0x7F
#define KDNET_DATA_SEQNO_MASK 0xFFFFFF00

// Compile time assertions macros taken from :
// http://www.pixelbeat.org/programming/gcc/static_assert.html
#define ASSERT_CONCAT_(a, b) a##b
#define ASSERT_CONCAT(a, b) ASSERT_CONCAT_(a, b)
#define ct_assert(e) enum { ASSERT_CONCAT(assert_line_, __LINE__) = 1/(!!(e)) }

__declspec(dllexport) VOID __stdcall KdComSendString(BYTE buffer[]);
__declspec(dllexport) VOID __stdcall KdComInitialize(ULONG64 rip);
__declspec(dllexport) BYTE __stdcall KdComReceiveByte(ULONG64* timeout);
__declspec(dllexport) LONG __stdcall KdComReceivePacket(KD_PACKET* pkt);
__declspec(dllexport) VOID __stdcall KdComSendByte(ULONG64* timeout, BYTE val);
__declspec(dllexport) LONG __stdcall KdComSendPacket(KD_PACKET* pkt);
__declspec(dllexport) LONG __stdcall KdComParsePacket(KD_PACKET* req);


#define VER_PRODUCTBUILD            10011
#define SkBuildNumber				(VER_PRODUCTBUILD | 0xC0000000)
#define NtBuildNumber				(VER_PRODUCTBUILD | 0xC0000000)
#define INITIAL_PACKET_ID			0x80800000 
#define PACKET_TRAILING_BYTE        0xAA

#define DBGKD_64BIT_PROTOCOL_VERSION1	5
#define DBGKD_64BIT_PROTOCOL_VERSION2	6
#define CURRENT_KD_SECONDARY_VERSION  KD_SECONDARY_VERSION_AMD64_CONTEXT
#define KD_SECONDARY_VERSION_AMD64_CONTEXT            2
#define IMAGE_FILE_MACHINE_AMD64             0x8664  // AMD64 (K8)
#define DBGKD_VERS_FLAG_MP         0x0001   // kernel is MP built
#define PACKET_TYPE_MAX                 12


#define AMD64_KGDT64_NULL (0 * 16)            // NULL descriptor
#define AMD64_KGDT64_R0_CODE (1 * 16)         // kernel mode 64-bit code
#define AMD64_KGDT64_R0_DATA (1 * 16) + 8     // kernel mode 64-bit data (stack)
#define AMD64_KGDT64_R3_CMCODE (2 * 16)       // user mode 32-bit code
#define AMD64_KGDT64_R3_DATA (2 * 16) + 8     // user mode 32-bit data
#define AMD64_KGDT64_R3_CODE (3 * 16)         // user mode 64-bit code
#define AMD64_KGDT64_SYS_TSS (4 * 16)         // kernel mode system task state
#define AMD64_KGDT64_R3_CMTEB (5 * 16)        // user mode 32-bit TEB
#define AMD64_KGDT64_LAST (6 * 16)

typedef enum _KDP_BREAKPOINT_FLAGS
{
	KdpBreakpointActive = 1,
	KdpBreakpointPending = 2,
	KdpBreakpointSuspended = 4,
	KdpBreakpointExpired = 8
} KDP_BREAKPOINT_FLAGS;

#define KD_BREAKPOINT_IN_USE         0x00000001
#define KD_BREAKPOINT_NEEDS_WRITE    0x00000002
#define KD_BREAKPOINT_SUSPENDED      0x00000004
#define KD_BREAKPOINT_NEEDS_REPLACE  0x00000008
#define KD_BREAKPOINT_PRELUDE_EDITED 0x00000010   // MOVL instruction displaced
#define KD_BREAKPOINT_USER_CUSTOM    0x00000020   // User space instruction supplied by debugger


#define RTL_CONTEXT_EX_OFFSET(ContextEx, Chunk) ((ContextEx)->Chunk.Offset)

#define RTL_CONTEXT_EX_LENGTH(ContextEx, Chunk) ((ContextEx)->Chunk.Length)

#define RTL_CONTEXT_EX_CHUNK(Base, Layout, Chunk)     ((PVOID)((PCHAR)(Base) + RTL_CONTEXT_EX_OFFSET(Layout, Chunk)))

#define RTL_CONTEXT_OFFSET(Context, Chunk)    RTL_CONTEXT_EX_OFFSET((PCONTEXT_EX)(Context + 1), Chunk)

#define RTL_CONTEXT_LENGTH(Context, Chunk) RTL_CONTEXT_EX_LENGTH((PCONTEXT_EX)(Context + 1), Chunk)

#define RTL_CONTEXT_CHUNK(Context, Chunk)               \
    RTL_CONTEXT_EX_CHUNK((PCONTEXT_EX)(Context + 1),    \
                         (PCONTEXT_EX)(Context + 1),    \
                         Chunk)

typedef struct _DBGKD_DEBUG_DATA_HEADER64 {

	//
	// Link to other blocks
	//

	LIST_ENTRY List;

	//
	// This is a unique tag to identify the owner of the block.
	// If your component only uses one pool tag, use it for this, too.
	//

	ULONG           OwnerTag;

	//
	// This must be initialized to the size of the data block,
	// including this structure.
	//

	ULONG           Size;

} DBGKD_DEBUG_DATA_HEADER64, * PDBGKD_DEBUG_DATA_HEADER64;

typedef struct _KDDEBUGGER_DATA64 {

	DBGKD_DEBUG_DATA_HEADER64 Header;

	//
	// Base address of kernel image
	//

	ULONG64   KernBase;

	//
	// DbgBreakPointWithStatus is a function which takes an argument
	// and hits a breakpoint.  This field contains the address of the
	// breakpoint instruction.  When the debugger sees a breakpoint
	// at this address, it may retrieve the argument from the first
	// argument register, or on x86 the eax register.
	//

	ULONG64   BreakpointWithStatus;       // address of breakpoint

	//
	// Address of the saved context record during a bugcheck
	//
	// N.B. This is an automatic in KeBugcheckEx's frame, and
	// is only valid after a bugcheck.
	//

	ULONG64   SavedContext;

	//
	// help for walking stacks with user callbacks:
	//

	//
	// The address of the thread structure is provided in the
	// WAIT_STATE_CHANGE packet.  This is the offset from the base of
	// the thread structure to the pointer to the kernel stack frame
	// for the currently active usermode callback.
	//

	USHORT  ThCallbackStack;            // offset in thread data

	//
	// these values are offsets into that frame:
	//

	USHORT  NextCallback;               // saved pointer to next callback frame
	USHORT  FramePointer;               // saved frame pointer

	//
	// pad to a quad boundary
	//
	USHORT  PaeEnabled : 1;
	USHORT  KiBugCheckRecoveryActive : 1; // Windows 10 Manganese Addition
	USHORT  PagingLevels : 4;

	//
	// Address of the kernel callout routine.
	//

	ULONG64   KiCallUserMode;             // kernel routine

	//
	// Address of the usermode entry point for callbacks.
	//

	ULONG64   KeUserCallbackDispatcher;   // address in ntdll


	//
	// Addresses of various kernel data structures and lists
	// that are of interest to the kernel debugger.
	//

	ULONG64   PsLoadedModuleList;
	ULONG64   PsActiveProcessHead;
	ULONG64   PspCidTable;

	ULONG64   ExpSystemResourcesList;
	ULONG64   ExpPagedPoolDescriptor;
	ULONG64   ExpNumberOfPagedPools;

	ULONG64   KeTimeIncrement;
	ULONG64   KeBugCheckCallbackListHead;
	ULONG64   KiBugcheckData;

	ULONG64   IopErrorLogListHead;

	ULONG64   ObpRootDirectoryObject;
	ULONG64   ObpTypeObjectType;

	ULONG64   MmSystemCacheStart;
	ULONG64   MmSystemCacheEnd;
	ULONG64   MmSystemCacheWs;

	ULONG64   MmPfnDatabase;
	ULONG64   MmSystemPtesStart;
	ULONG64   MmSystemPtesEnd;
	ULONG64   MmSubsectionBase;
	ULONG64   MmNumberOfPagingFiles;

	ULONG64   MmLowestPhysicalPage;
	ULONG64   MmHighestPhysicalPage;
	ULONG64   MmNumberOfPhysicalPages;

	ULONG64   MmMaximumNonPagedPoolInBytes;
	ULONG64   MmNonPagedSystemStart;
	ULONG64   MmNonPagedPoolStart;
	ULONG64   MmNonPagedPoolEnd;

	ULONG64   MmPagedPoolStart;
	ULONG64   MmPagedPoolEnd;
	ULONG64   MmPagedPoolInformation;
	ULONG64   MmPageSize;

	ULONG64   MmSizeOfPagedPoolInBytes;

	ULONG64   MmTotalCommitLimit;
	ULONG64   MmTotalCommittedPages;
	ULONG64   MmSharedCommit;
	ULONG64   MmDriverCommit;
	ULONG64   MmProcessCommit;
	ULONG64   MmPagedPoolCommit;
	ULONG64   MmExtendedCommit;

	ULONG64   MmZeroedPageListHead;
	ULONG64   MmFreePageListHead;
	ULONG64   MmStandbyPageListHead;
	ULONG64   MmModifiedPageListHead;
	ULONG64   MmModifiedNoWritePageListHead;
	ULONG64   MmAvailablePages;
	ULONG64   MmResidentAvailablePages;

	ULONG64   PoolTrackTable;
	ULONG64   NonPagedPoolDescriptor;

	ULONG64   MmHighestUserAddress;
	ULONG64   MmSystemRangeStart;
	ULONG64   MmUserProbeAddress;

	ULONG64   KdPrintCircularBuffer;
	ULONG64   KdPrintCircularBufferEnd;
	ULONG64   KdPrintWritePointer;
	ULONG64   KdPrintRolloverCount;

	ULONG64   MmLoadedUserImageList;

	// NT 5.1 Addition

	ULONG64   NtBuildLab;
	ULONG64   KiNormalSystemCall;

	// NT 5.0 hotfix addition

	ULONG64   KiProcessorBlock;
	ULONG64   MmUnloadedDrivers;
	ULONG64   MmLastUnloadedDriver;
	ULONG64   MmTriageActionTaken;
	ULONG64   MmSpecialPoolTag;
	ULONG64   KernelVerifier;
	ULONG64   MmVerifierData;
	ULONG64   MmAllocatedNonPagedPool;
	ULONG64   MmPeakCommitment;
	ULONG64   MmTotalCommitLimitMaximum;
	ULONG64   CmNtCSDVersion;

	// NT 5.1 Addition

	ULONG64   MmPhysicalMemoryBlock;
	ULONG64   MmSessionBase;
	ULONG64   MmSessionSize;
	ULONG64   MmSystemParentTablePage;

	// Server 2003 addition

	ULONG64   MmVirtualTranslationBase;

	USHORT    OffsetKThreadNextProcessor;
	USHORT    OffsetKThreadTeb;
	USHORT    OffsetKThreadKernelStack;
	USHORT    OffsetKThreadInitialStack;

	USHORT    OffsetKThreadApcProcess;
	USHORT    OffsetKThreadState;
	USHORT    OffsetKThreadBStore;
	USHORT    OffsetKThreadBStoreLimit;

	USHORT    SizeEProcess;
	USHORT    OffsetEprocessPeb;
	USHORT    OffsetEprocessParentCID;
	USHORT    OffsetEprocessDirectoryTableBase;

	USHORT    SizePrcb;
	USHORT    OffsetPrcbDpcRoutine;
	USHORT    OffsetPrcbCurrentThread;
	USHORT    OffsetPrcbMhz;

	USHORT    OffsetPrcbCpuType;
	USHORT    OffsetPrcbVendorString;
	USHORT    OffsetPrcbProcStateContext;
	USHORT    OffsetPrcbNumber;

	USHORT    SizeEThread;

	UCHAR     L1tfHighPhysicalBitIndex;  // Windows 10 19H1 Addition
	UCHAR     L1tfSwizzleBitIndex;       // Windows 10 19H1 Addition

	ULONG     Padding0;

	ULONG64   KdPrintCircularBufferPtr;
	ULONG64   KdPrintBufferSize;

	ULONG64   KeLoaderBlock;

	USHORT    SizePcr;
	USHORT    OffsetPcrSelfPcr;
	USHORT    OffsetPcrCurrentPrcb;
	USHORT    OffsetPcrContainedPrcb;

	USHORT    OffsetPcrInitialBStore;
	USHORT    OffsetPcrBStoreLimit;
	USHORT    OffsetPcrInitialStack;
	USHORT    OffsetPcrStackLimit;

	USHORT    OffsetPrcbPcrPage;
	USHORT    OffsetPrcbProcStateSpecialReg;
	USHORT    GdtR0Code;
	USHORT    GdtR0Data;

	USHORT    GdtR0Pcr;
	USHORT    GdtR3Code;
	USHORT    GdtR3Data;
	USHORT    GdtR3Teb;

	USHORT    GdtLdt;
	USHORT    GdtTss;
	USHORT    Gdt64R3CmCode;
	USHORT    Gdt64R3CmTeb;

	ULONG64   IopNumTriageDumpDataBlocks;
	ULONG64   IopTriageDumpDataBlocks;

	// Longhorn addition

	ULONG64   VfCrashDataBlock;
	ULONG64   MmBadPagesDetected;
	ULONG64   MmZeroedPageSingleBitErrorsDetected;

	// Windows 7 addition

	ULONG64   EtwpDebuggerData;
	USHORT    OffsetPrcbContext;

	// Windows 8 addition

	USHORT    OffsetPrcbMaxBreakpoints;
	USHORT    OffsetPrcbMaxWatchpoints;

	ULONG     OffsetKThreadStackLimit;
	ULONG     OffsetKThreadStackBase;
	ULONG     OffsetKThreadQueueListEntry;
	ULONG     OffsetEThreadIrpList;

	USHORT    OffsetPrcbIdleThread;
	USHORT    OffsetPrcbNormalDpcState;
	USHORT    OffsetPrcbDpcStack;
	USHORT    OffsetPrcbIsrStack;

	USHORT    SizeKDPC_STACK_FRAME;

	// Windows 8.1 Addition

	USHORT    OffsetKPriQueueThreadListHead;
	USHORT    OffsetKThreadWaitReason;

	// Windows 10 RS1 Addition

	USHORT    Padding1;
	ULONG64   PteBase;

	// Windows 10 RS5 Addition

	ULONG64   RetpolineStubFunctionTable;
	ULONG     RetpolineStubFunctionTableSize;
	ULONG     RetpolineStubOffset;
	ULONG     RetpolineStubSize;

	// Windows 10 Iron Addition

	USHORT OffsetEProcessMmHotPatchContext;

} KDDEBUGGER_DATA64, * PKDDEBUGGER_DATA64;

typedef struct _STRING {
	WORD Length;
	WORD MaximumLength;
	BYTE* Buffer;
} STRING;

typedef STRING* PSTRING;

typedef struct _BREAKPOINT_ENTRY {
	PVOID Address;
	ULONG64* Process;
	ULONG64 BreakInstruction;
	ULONG64 Content;
	ULONG Flags;
	BYTE BreakInstructionSize;
	BYTE BreakInstructionAlignment;
} BREAKPOINT_ENTRY, * PBREAKPOINT_ENTRY;


typedef struct _AMD64_DESCRIPTOR {
	USHORT  Pad[3];
	USHORT  Limit;
	ULONG64 Base;
} AMD64_DESCRIPTOR, * PAMD64_DESCRIPTOR;

typedef struct _AMD64_KSPECIAL_REGISTERS {
	ULONG64 Cr0;
	ULONG64 Cr2;
	ULONG64 Cr3;
	ULONG64 Cr4;
	ULONG64 KernelDr0;
	ULONG64 KernelDr1;
	ULONG64 KernelDr2;
	ULONG64 KernelDr3;
	ULONG64 KernelDr6;
	ULONG64 KernelDr7;
	AMD64_DESCRIPTOR Gdtr;
	AMD64_DESCRIPTOR Idtr;
	USHORT Tr;
	USHORT Ldtr;
	ULONG MxCsr;
	ULONG64 DebugControl;
	ULONG64 LastBranchToRip;
	ULONG64 LastBranchFromRip;
	ULONG64 LastExceptionToRip;
	ULONG64 LastExceptionFromRip;
	ULONG64 Cr8;
	ULONG64 MsrGsBase;
	ULONG64 MsrGsSwap;
	ULONG64 MsrStar;
	ULONG64 MsrLStar;
	ULONG64 MsrCStar;
	ULONG64 MsrSyscallMask;
	ULONG64 Xcr0;
} AMD64_KSPECIAL_REGISTERS, * PAMD64_KSPECIAL_REGISTERS;

VOID
SkdInitDebuggerDataBlock(
	VOID
);

VOID KdInitDebuggerDataBlock(
	VOID
);

NTSTATUS WindbgHandshake(PCSTR windbg_host);
LONG KdParsePacket(KD_PACKET* req);
VOID KdSendSingleStep();
VOID KdLoadSymbols(CHAR* ImageName, ULONG64 ImageBase, ULONG64 ImageSize, ULONG64 CheckSum);

#endif
