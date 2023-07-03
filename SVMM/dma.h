#ifndef DMA_H
#define DMA_H
#include <Windows.h>

#define ALIGNED(x) __declspec(align(x))
#define PACKED ALIGNED(1)

typedef ULONG(*DmaWriteMemoryFn)(BYTE Channel, BYTE* Buffer, ULONG64 GuestPhysicalAddress, ULONG Length);
typedef ULONG(*DmaReadMemoryFn)(BYTE Channel, BYTE* Buffer, ULONG64 GuestPhysicalAddress, ULONG Length);

typedef struct _DMA_CHANNEL {
	WORD	CurrentAddressRegister;
	WORD	CurrentCountRegister;
	WORD	BaseAddressRegister;
	WORD	BaseCountRegister;
	BYTE	ModeRegister;
	BYTE	MaskRegister;
	BYTE	PageRegisters;
	BYTE	HiPageRegisters;
	DmaReadMemoryFn DmaReadHandler;
	DmaWriteMemoryFn DmaWriteHandler;
} DMA_CHANNEL, *PDMA_CHANNEL;

typedef struct _DMA_CONTROLLER {
	BYTE	StatusRegister;
	BYTE	CommandRegister;
	BYTE	FlipFlop;

	BYTE	DmaRequest[4];
	BYTE	DmaAck[4];

	BYTE	DmaHoldAck;
	BYTE	DmaTerminalCount;

	DMA_CHANNEL Channels[4];
} DMA_CONTROLLER, *PDMA_CONTROLLER;

/* Status/Command */
enum {
	CMD_MEMTOMEM = 0x01,     /* Enable mem-to-mem trasfers. */
	CMD_ADRHOLD = 0x02,     /* Address hold for mem-to-mem. */
	CMD_DISABLE = 0x04,     /* Disable controller. */
	CMD_COMPRTIME = 0x08,     /* Compressed timing. */
	CMD_ROTPRIO = 0x10,     /* Rotating priority. */
	CMD_EXTWR = 0x20,     /* Extended write. */
	CMD_DREQHI = 0x40,     /* DREQ is active high if set. */
	CMD_DACKHI = 0x80,     /* DACK is active high if set. */
	CMD_UNSUPPORTED = CMD_MEMTOMEM | CMD_ADRHOLD | CMD_COMPRTIME
	| CMD_EXTWR | CMD_DREQHI | CMD_DACKHI
};

/* DMA transfer modes. */
enum {
	DMODE_DEMAND,   /* Demand transfer mode. */
	DMODE_SINGLE,   /* Single transfer mode. */
	DMODE_BLOCK,    /* Block transfer mode. */
	DMODE_CASCADE   /* Cascade mode. */
};
/* DMA transfer types. */
enum {
	DTYPE_VERIFY,   /* Verify transfer type. */
	DTYPE_WRITE,    /* Write transfer type. */
	DTYPE_READ,     /* Read transfer type. */
	DTYPE_ILLEGAL   /* Undefined. */
};

#define DMA_MODE_VERIFY		(0 << 2)
#define DMA_MODE_WRITE		(1 << 2) 
#define DMA_MODE_READ		(1 << 3)
#define DMA_MODE_AUTO_INIT  (1 << 4)
#define DMA_MODE_DECREMENT  (1 << 5)

static int const DmaChannelPage[7] = { 2, 3, 1, 0, 0, 0, 0 };
static int const DmaChannelHipage[7] = { 0, 0, 0, 2, 3, 4, 0 };

/* Channel to Controller */
#define	 DMA_CHANNEL_TO_CONTROLLER(c)          (c < 4 ? 0 : 1)

#define DMA_ADDRESS_TO_CHANNEL_PAGE(i)		(DmaChannelPage[i])
#define DMA_ADDRESS_TO_CHANNEL_HIPAGE(i)	(DmaChannelHipage[i])

VOID DmaInitialize(VOID);
ULONG DmaWriteMemory(BYTE Channel, BYTE* Buffer, ULONG64 GuestPhysicalAddress, ULONG Length);
ULONG DmaReadMemory(BYTE Channel, BYTE* Buffer, ULONG64 GuestPhysicalAddress, ULONG Length);
ULONG DmaPortIoReadHandler(ULONG64 Address, ULONG Length);
VOID DmaPortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length);

static VOID DmaControlHoldRequestLine(ULONG DmaNumber);
VOID DmaSetHoldRequest(ULONG Channel, BYTE Value);

#endif
