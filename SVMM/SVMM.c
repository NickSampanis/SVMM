#include <stdio.h>
#include "SVMM.h"
#include "windbg.h"
#include "i440fx.h"
#include "dma.h"
#include "pci.h"
#include "pic.h"
#include "cmos.h"
#include "ps2.h"
#include "bios.h"
#include "pit.h"
#include "timer.h"
#include "parallel.h"
#include "serial.h"
#include "gvm.h"
#include "haxm.h"
#include "gui.h"
#include "vgacore.h"
#include <intrin.h>
#include <CommCtrl.h>

//#define __DEBUG__ 1
//#define HAXM	1
#ifdef HAXM
extern volatile struct hax_tunnel* tunnel;
extern volatile unsigned char* iobuf;
extern HANDLE hHaxCpu;
struct hax_fastmmio* mmio;
#endif

#ifdef GVM
extern struct kvm_run* kvm_run;
extern HANDLE hGvm, hGvmCpu, hGvmVm;
extern volatile unsigned char* pio_data;
extern DWORD kvm_events;
#endif

extern SOCKET windbg_sock;
extern BYTE Stepping;

//GLOBALS
PVOID Ram;
ULONG64 RamSize;
struct Registers Registers;

BYTE* SvmmGetHostAddress(ULONG64 GuestPa)
{
	ULONG64 gpaAddress;
	/*
	if (Registers.context._cr0 & 1) {
		gpaAddress = GuestPa;
	}
	else {
		//gvaAddress = GuestVa + Registers.context._cs.selector * 16;
		gpaAddress = GuestPa | (ULONG64)Registers.context._cs.selector << 4;
	}
	*/
	gpaAddress = GuestPa;
	if (gpaAddress >= 0xFFFE0000 && gpaAddress <= 0xFFFFFFFF) {
		//0xFFFE0000 = Ram + RamSize, 0xFFFF0000 = Ram + RamSize + 0x10000
		gpaAddress &= 0x000FFFFF;
		gpaAddress = ((gpaAddress >> 16) & 1) << 4;
		gpaAddress += (ULONG64)Ram + RamSize;
	}
	else if (gpaAddress >= 0xE0000 && gpaAddress <= 0xFFFFF) {
		gpaAddress += (ULONG64)Ram + RamSize;
	}
	else if (gpaAddress >= 0x0000 && gpaAddress <= 0x20000) { //TODO CHANGE IT TO RAM
		if (!(Registers.context._cr0 & 1)) {
			gpaAddress = GuestPa | (ULONG64)Registers.context._cs.selector << 4;
		}
		gpaAddress += (ULONG64)Ram + RamSize + 0xE0000;
	}
	else {
		gpaAddress += (ULONG64)Ram;
	}

	return (BYTE*)gpaAddress;
}


NTSTATUS SvmmLoadRom(LPCSTR Path, BYTE Type)
{
	ULONG64 offset, RomAddress;
	BYTE *HostRomAddress;
	HANDLE hFile;
	DWORD fileSize, retSize;
	BOOL status;

	HostRomAddress = 0;
	hFile = CreateFileA(Path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Error in CreateFileA 0x%lx\n", GetLastError());
		return -1;
	}
	fileSize = GetFileSize(hFile, NULL);
	if (fileSize == INVALID_FILE_SIZE || fileSize > ROM_SIZE) {
		fprintf(stderr, "Error in GetFileSize fileSize > ROM_SIZE \n");
		goto error;
	}
	offset = 0;
	if (Type == TYPE_SYSTEM_BIOS) {
		RomAddress = ~(fileSize - 1);
		offset = RomAddress & BIOS_MASK;
		HostRomAddress = (ULONG64)Ram + RamSize + 0xe0000;

	}
	else {
		RomAddress = ~(fileSize - 1);
		HostRomAddress = (ULONG64)Ram + RamSize + 0xc0000;
	}
	if (fileSize + offset < offset || ROM_SIZE < offset + fileSize) {
		fprintf(stderr, "ROM_SIZE < offset\n");
		goto error;
	}
	retSize = 0;
	status = ReadFile(hFile, HostRomAddress, fileSize, &retSize, NULL);
	if (!status) {
		fprintf(stderr, "Error in ReadFile 0x%lx\n", GetLastError());
		goto error;
	}
	/*
	if (Type == TYPE_SYSTEM_BIOS || (HostRomAddress[offset] == 0x55 && HostRomAddress[offset + 1] == 0xaa)) {
		if (Type == TYPE_SYSTEM_BIOS && (RomAddress & 0xfffff) == 0xe0000) {
			offset += 0x10000;
			retSize = 0x10000;
		}
	}
	checksum = 0;
	for (i = 0; i < (INT)retSize; i++) {
		checksum += HostRomAddress[offset + i];
	}
	if (checksum) {
		fprintf(stderr, "Error invalid checksum\n");
		goto error;
	}
	*/
	CloseHandle(hFile);
	return 0;
error:
	CloseHandle(hFile);
	return -1;
}


/*

NTSTATUS SvmmLoadRom(LPCSTR Path, BYTE Type)
{
	ULONG64 offset, RomAddress;
	BYTE checksum, *HostRomAddress;
	HANDLE hFile;
	DWORD fileSize, retSize;
	BOOL status;
	INT i;

	hFile = CreateFileA(Path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Error in CreateFileA 0x%lx\n", GetLastError());
		return -1;
	}
	fileSize = GetFileSize(hFile, NULL);
	if (fileSize == INVALID_FILE_SIZE || fileSize > ROM_SIZE) {
		fprintf(stderr, "Error in GetFileSize fileSize > ROM_SIZE \n");
		goto error;
	}
	offset = 0;
	if (Type == TYPE_SYSTEM_BIOS) {
		//bios_rom_addr = 0xfffe0000 offset = 0x1e0000
		RomAddress = ~(fileSize - 1);
		offset = RomAddress & BIOS_MASK;

	}
	else {
		RomAddress = ~(fileSize - 1);
	}
	HostRomAddress = SvmmGetHostAddress(RomAddress);
	if (fileSize + offset < offset || ROM_SIZE < offset + fileSize) {
		fprintf(stderr, "ROM_SIZE < offset\n");
		goto error;
	}
	retSize = 0;
	status = ReadFile(hFile, &HostRomAddress[offset], fileSize, &retSize, NULL);
	if (!status) {
		fprintf(stderr, "Error in ReadFile 0x%lx\n", GetLastError());
		goto error;
	}
	if (Type == TYPE_SYSTEM_BIOS || (HostRomAddress[offset] == 0x55 && HostRomAddress[offset + 1] == 0xaa)) {
		if (Type == TYPE_SYSTEM_BIOS && (RomAddress & 0xfffff) == 0xe0000) {
			offset += 0x10000;
			retSize = 0x10000;
		}
	}
	checksum = 0;
	for (i = 0; i < (INT)retSize; i++) {
		checksum += HostRomAddress[offset + i];
	}
	if (checksum) {
		fprintf(stderr, "Error invalid checksum\n");
		goto error;
	}

	CloseHandle(hFile);
	return 0;
error:
	CloseHandle(hFile);
	return -1;
}
*/


void SvmmPrintRegisters(struct Registers* Registers)
{
	int i;
	ULONG64 gvaAddress;

	printf("RAX = 0x%llx\n", Registers->context._rax);
	printf("RCX = 0x%llx\n", Registers->context._rcx);
	printf("RDX = 0x%llx\n", Registers->context._rdx);
	printf("RBX = 0x%llx\n", Registers->context._rbx);
	printf("RSP = 0x%llx\n", Registers->context._rsp);
	printf("RBP = 0x%llx\n", Registers->context._rbp);
	printf("RSI = 0x%llx\n", Registers->context._rsi);
	printf("RDI = 0x%llx\n", Registers->context._rdi);
	printf("R8 = 0x%llx\n", Registers->context._r8);
	printf("R9 = 0x%llx\n", Registers->context._r9);
	printf("R10 = 0x%llx\n", Registers->context._r10);
	printf("R11 = 0x%llx\n", Registers->context._r11);
	printf("R12 = 0x%llx\n", Registers->context._r12);
	printf("R13 = 0x%llx\n", Registers->context._r13);
	printf("R14 = 0x%llx\n", Registers->context._r14);
	printf("R15 = 0x%llx\n", Registers->context._r15);
	printf("RIP = 0x%llx\n", Registers->context._rip);
	printf("eflgs = 0x%llx\n", Registers->context._rflags);

	printf("idtr.base = 0x%llx\n", Registers->context._idt.base);
	printf("idtr.limit = 0x%lx\n", Registers->context._idt.limit);
	printf("gdtr.base = 0x%llx\n", Registers->context._gdt.base);
	printf("gdtr.limit = 0x%lx\n", Registers->context._gdt.limit);

	printf("cr0 = 0x%llx\n", Registers->context._cr0);
	printf("cr2 = 0x%llx\n", Registers->context._cr2);
	printf("cr3 = 0x%llx\n", Registers->context._cr3);
	printf("cr4 = 0x%llx\n", Registers->context._cr4);

	//try with type = 1
	//try with avail = 1
	printf("\ncs selector = 0x%x\n", Registers->context._cs.selector);
	printf("cs base = 0x%llx\n", Registers->context._cs.base);
	printf("cs limit = 0x%lx\n", Registers->context._cs.limit);
	printf("cs type = 0x%x\n", Registers->context._cs.type);
	printf("cs desc = 0x%x\n", Registers->context._cs.desc);
	printf("cs dpl = 0x%x\n", Registers->context._cs.dpl);
	printf("cs present = 0x%x\n", Registers->context._cs.present);
	printf("cs available = 0x%x\n", Registers->context._cs.available);
	printf("cs long_mode = 0x%x\n", Registers->context._cs.long_mode);
	printf("cs granularity = 0x%x\n\n", Registers->context._cs.granularity);

	printf("\nds selector = 0x%x\n", Registers->context._ds.selector);
	printf("ds base = 0x%llx\n", Registers->context._ds.base);
	printf("ds limit = 0x%lx\n", Registers->context._ds.limit);
	printf("ds type = 0x%x\n", Registers->context._ds.type);
	printf("ds desc = 0x%x\n", Registers->context._ds.desc);
	printf("ds dpl = 0x%x\n", Registers->context._ds.dpl);
	printf("ds present = 0x%x\n", Registers->context._ds.present);
	printf("ds available = 0x%x\n", Registers->context._ds.available);
	printf("ds long_mode = 0x%x\n", Registers->context._ds.long_mode);
	printf("ds granularity = 0x%x\n\n", Registers->context._ds.granularity);

	printf("\ngs selector = 0x%x\n", Registers->context._gs.selector);
	printf("gs base = 0x%llx\n", Registers->context._gs.base);
	printf("gs limit = 0x%lx\n", Registers->context._gs.limit);
	printf("gs type = 0x%x\n", Registers->context._gs.type);
	printf("gs desc = 0x%x\n", Registers->context._gs.desc);
	printf("gs dpl = 0x%x\n", Registers->context._gs.dpl);
	printf("gs present = 0x%x\n", Registers->context._gs.present);
	printf("gs available = 0x%x\n", Registers->context._gs.available);
	printf("gs long_mode = 0x%x\n", Registers->context._gs.long_mode);
	printf("gs granularity = 0x%x\n\n", Registers->context._gs.granularity);


	printf("fs selector = 0x%x\n", Registers->context._fs.selector);
	printf("fs base = 0x%llx\n", Registers->context._fs.base);
	printf("fs limit = 0x%lx\n", Registers->context._fs.limit);
	printf("fs type = 0x%x\n", Registers->context._fs.type);
	printf("fs desc = 0x%x\n", Registers->context._fs.desc);
	printf("fs dpl = 0x%x\n", Registers->context._fs.dpl);
	printf("fs present = 0x%x\n", Registers->context._fs.present);
	printf("fs available = 0x%x\n", Registers->context._fs.available);
	printf("fs long_mode = 0x%x\n", Registers->context._fs.long_mode);
	printf("fs granularity = 0x%x\n\n", Registers->context._fs.granularity);
	
	gvaAddress = SvmmGetHostAddress(Registers->context._rip);
	printf("GUEST RIP = 0x%llx\n", Registers->context._rip);
	printf("HOST  RIP = 0x%llx\n", (ULONG64)gvaAddress);
	for (i = 0; i < 8; i++)
		printf("0x%02x ", ((BYTE *)gvaAddress)[i]);
	printf("\n");

}


//Table 14-1. Initial Processor State BX_CPU_C::reset
VOID SvmmInitializeRegisterState(struct Registers* Registers)
{
	memset(Registers, '\0', sizeof(struct Registers));
	Registers->context._rip = 0x0000FFF0;
	Registers->context._dr6 = 0xFFFF0FF0;
	Registers->context._dr7 = 0x00000400;
	Registers->context._cr0 = 0x60000010;
	Registers->context._rflags = 0x2;

	Registers->context._cs.selector = 0xF000;
	Registers->context._cs.base = 0xFFFF0000;
	Registers->context._cs.limit = 0xFFFF;
	Registers->context._cs.present = 1;
	Registers->context._cs.type = 3;
	Registers->context._cs.desc = 1;

	Registers->context._ds.selector = 0x0000;
	Registers->context._ds.base = 0;
	Registers->context._ds.limit = 0xFFFF;
	Registers->context._ds.desc = 1;
	Registers->context._ds.type = 3;
	Registers->context._ds.present = 1;

	Registers->context._es.selector = 0x0000;
	Registers->context._es.base = 0;
	Registers->context._es.limit = 0xFFFF;
	Registers->context._es.desc = 1;
	Registers->context._es.type = 3;
	Registers->context._es.present = 1;

	Registers->context._fs.selector = 0x0000;
	Registers->context._fs.base = 0;
	Registers->context._fs.limit = 0xFFFF;

	Registers->context._gs.selector = 0x0000;
	Registers->context._gs.base = 0;
	Registers->context._gs.limit = 0xFFFF;

	Registers->context._ss.selector = 0x0000;
	Registers->context._ss.base = 0;
	Registers->context._ss.limit = 0xFFFF;
	Registers->context._ss.desc = 1;
	Registers->context._ss.type = 3;
	Registers->context._ss.present = 1;


	Registers->context._gdt.limit = 0xFFFF;
	Registers->context._idt.limit = 0xFFFF;

	Registers->context._ldt.desc = 0;
	Registers->context._ldt.type = 2;
	Registers->context._ldt.limit = 0xFFFF;
	Registers->context._ldt.present = 1;

	Registers->context._tr.desc = 0;
	Registers->context._tr.present = 1;
	Registers->context._tr.type = 0xb;
	Registers->context._tr.limit = 0xFFFF;


	Registers->fx.mxcsr = 0x1f80;
	Registers->fx.mxcsr_mask = 0x0000ffbf | 0x00020000 | 0x00000040;
	Registers->smbase = 0x30000;
	Registers->apicbase = 0xfee00000 | 0x900;
	Registers->xcr0 = 1;
	Registers->fmask = 0x00020200;
	Registers->pat = 0x0007040600070406;
}

NTSTATUS SvmmSetRegisters(struct Registers* Registers)
{
#ifdef HAXM
	haxm_set_registers(Registers);
#elif GVM
	gvm_set_registers(Registers);
#endif
	return 0;

}

NTSTATUS SvmmGetRegisters(struct Registers* Registers)
{
#ifdef HAXM
	haxm_get_registers(Registers);
#elif GVM
	gvm_get_registers(Registers);
#endif
	return 0;
}

/*

#define BX_KEY_PRESSED  0x00000000
#define BX_KEY_RELEASED 0x80000000

#define BX_KEY_UNHANDLED 0x10000000

#define VK_PRIOR    65365
#define VK_NEXT     65366
#define VK_F2       65471
#define VK_F3       65472
#define VK_F4       65473
#define VK_F5       65474
#define VK_F6       65475
#define VK_F7       65476
#define VK_F8       65477
#define VK_F9       65478
#define VK_F11      65480
#define VK_UP       65362
#define VK_DOWN     65364
#define VK_RETURN   65293
#define VK_LEFT     65361
#define VK_RIGHT    65363
#define VK_END      65367
#define VK_HOME     65360
#define VK_DELETE   65535
#define VK_ESCAPE   65307

HHOOK hKeyboardHook;
typedef struct {
	HINSTANCE hInstance;

	CRITICAL_SECTION drawCS;
	CRITICAL_SECTION keyCS;
	CRITICAL_SECTION mouseCS;

	int kill;  // reason for terminateEmul(int)
	BOOL UIinited;
	HWND mainWnd;
	HWND simWnd;
} sharedThreadInfo;

sharedThreadInfo stInfo;
static BITMAPINFO* bitmap_info;
static int ms_savedx = 0, ms_savedy = 0;
static DWORD workerThreadID = 0;
HANDLE workerThread;
HWND hwndTB, hwndSB;
HBITMAP MemoryBitmap;
HDC MemoryDC;
static HWND hotKeyReceiver = NULL;
BOOL fullscreenMode, inFullscreenToggle;
DWORD x_tilesize, win32_max_xres, win32_max_yres, dimension_x, dimension_y, current_bpp, stretched_x, stretched_y, stretch_factor;
DWORD bx_bitmap_entries, win32_toolbar_entries, bx_hb_separator;
RGBQUAD* cmap_index;
HBITMAP vgafont[256];
BOOL mouseCaptureMode, mouseCaptureNew, mouseToggleReq;
int mouse_buttons;


#define BX_MAX_STATUSITEMS			10
#define BX_SB_MAX_TEXT_ELEMENTS		2
#define EXIT_GUI_SHUTDOWN			1
#define SIZE_OF_SB_MOUSE_MESSAGE 170
#define SIZE_OF_SB_ELEMENT        40

DWORD SB_Edges[BX_MAX_STATUSITEMS + BX_SB_MAX_TEXT_ELEMENTS + 1];
unsigned SB_Text_Elements;

*/


int main(int argc, char *argv[])
{
	NTSTATUS Status;
	DWORD Bytes, i, vector;
	BOOL Ret;
	BYTE Buffer[2048];

	RamSize = 256 * MB;
	Ram = VirtualAlloc(NULL, RamSize + ROM_SIZE + PAGE_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!Ram) {
		fprintf(stderr, "VirtualAlloc 0x%x", GetLastError());
		return -1;
	}
	//CreateVm
#ifdef HAXM
	Status = haxm_init(Ram, RamSize);
#elif GVM
	Status = gvm_init(Ram, RamSize);
#endif
	if (Status < 0) {
		fprintf(stderr, "haxm_init 0x%x", Status);
		goto exit;
	}
	//Initialize Registers State
	memset(&Registers, '\0', sizeof(Registers));
	SvmmInitializeRegisterState(&Registers);
	SvmmSetRegisters(&Registers);
	//Initialize Devices DEV_init_devices
	ParallelInitialize();
	SerialInitialize();
	TimerInitialize();
	I440fxInitialize();
	PicInitialize();
	DmaInitialize();
	CmosInitialize();
	Ps2Initialize();
	BiosInitialize();
	PitInitialize();
	VgaCoreInitialize();
	//Load Bios At The End Of Ram
	//Status = SvmmLoadRom("bios.bin", TYPE_SYSTEM_BIOS);
	Status = SvmmLoadRom("BIOS-bochs-latest.bin", TYPE_SYSTEM_BIOS);
	if (Status < 0) {
		fprintf(stderr, "SvmmLoadRom 0x%x", Status);
		goto exit;
	}
	Status = SvmmLoadRom("VGABIOS-lgpl-latest.bin", TYPE_VGA_BIOS);
	if (Status < 0) {
		fprintf(stderr, "SvmmLoadRom 0x%x", Status);
		goto exit;
	}

	//Windbg Stub
	if (argv[1]) {
		Status = WindbgHandshake(argv[1]);
		if (Status)
			goto exit;
		Stepping = 1;
		do {
			do {
				memset(Buffer, '\0', sizeof(Buffer));
				Status = recvfrom(windbg_sock, (char*)Buffer, sizeof(Buffer), 0, NULL, NULL);
			} while (Stepping && (!Status || Status == -1));
			//pkt = (PKD_PACKET)buffer;
			Status = KdParsePacket((PKD_PACKET)Buffer);
		} while (Status != DbgKdContinueApi2);
		KdLoadSymbols("vga.exe", 0x1000, 0x3000, 0x00);
		do {
			do {
				memset(Buffer, '\0', sizeof(Buffer));
				Status = recvfrom(windbg_sock, (char*)Buffer, sizeof(Buffer), 0, NULL, NULL);
			} while (Stepping && (!Status || Status == -1));
			if (!Status || Status == -1)
				break;
			//pkt = (PKD_PACKET)buffer;
			Status = KdParsePacket((PKD_PACKET)Buffer);
		} while (Status != DbgKdContinueApi2);
	}
	SvmmSetRegisters(&Registers);
	
	//Hypervisor loop
	while (1) {
		//SvmmSetRegisters(&Registers);
		/*
		if (Registers.context._rflags & EFLAGS_TF_MASK)
				KdSendSingleStep();
		*/
#ifdef HAXM
		Ret = DeviceIoControl(hHaxCpu, HAX_VCPU_IOCTL_RUN, NULL, 0, NULL, 0, &Bytes, NULL);
#elif GVM
		Ret = DeviceIoControl(hGvmCpu, GVM_RUN, NULL, 0, NULL, 0, &Bytes, NULL);
#endif
		if (!Ret) {
			fprintf(stderr, "VCPU_IOCTL_RUN 0x%x", GetLastError());
			goto exit;
		}
		SvmmGetRegisters(&Registers);
#ifdef __DEBUG__
		SvmmPrintRegisters(&Registers);
#endif
#ifdef HAXM
		switch (tunnel->_exit_status) {
		case HAX_EXIT_HLT:
#ifdef __DEBUG__
			printf("HAX_EXIT_HLT\n");
			scanf_s("%d", &Bytes);
#endif
			break;
		case HAX_EXIT_IO:
#ifdef __DEBUG__
			printf("HAX_EXIT_IO\n");
#endif
			for (i = 0; i < tunnel->io._count; i++) {
#ifdef __DEBUG__
				printf("port: 0x%x\n", (uint16_t)tunnel->io._port);
				printf("size: %d\n",  (uint16_t)tunnel->io._size);
				printf("operation: %s\n", tunnel->io._direction == HAX_EXIT_IO_IN ? "in" : "out");
#endif
				if (tunnel->io._direction == HAX_EXIT_IO_IN) {
					*(DWORD*)(iobuf + i * tunnel->io._size) = ReadPortHandler(tunnel->io._port, tunnel->io._size);
					//*(DWORD *)(iobuf + i * tunnel->io._size) = bx_pc_system.inp(tunnel->io._port, tunnel->io._size);
					//printf("value: 0x%x\n", *(Bit32u *)(iobuf + i * tunnel->io._size));
				}
				else {
					WritePortHandler(tunnel->io._port, *(DWORD*)(iobuf + i * tunnel->io._size), tunnel->io._size);
					//bx_pc_system.outp(tunnel->io._port, *(DWORD*)(iobuf + i * tunnel->io._size), tunnel->io._size);
					//printf("value: 0x%x\n", *(Bit32u *)(iobuf + i * tunnel->io._size));
				}
			}
			//SvmmPrintRegisters();
			//scanf_s("%d", &Bytes);
			break;
		case HAX_EXIT_MMIO:
			printf("HAX_EXIT_MMIO\n");
			scanf_s("%d", &Bytes);
			break;
		case HAX_EXIT_REALMODE:
			printf("HAX_EXIT_REALMODE\n");
			scanf_s("%d", &Bytes);
			break;
		case HAX_EXIT_INTERRUPT:
#ifdef __DEBUG__
			printf("HAX_EXIT_INTERRUPT\n");
			//SvmmPrintRegisters();
			scanf_s("%d", &Bytes);
#endif
			break;
		case HAX_EXIT_UNKNOWN:
			printf("HAX_EXIT_UNKNOWN\n");
			scanf_s("%d", &Bytes);
			break;
		case HAX_EXIT_STATECHANGE:
			printf("HAX_EXIT_STATECHANGE\n");
			SvmmPrintRegisters(&Registers);
			scanf_s("%d", &Bytes);
			break;
		case HAX_EXIT_PAUSED:
			printf("HAX_EXIT_PAUSED\n");
			scanf_s("%d", &Bytes);
			break;
		case HAX_EXIT_FAST_MMIO:
			mmio = (struct hax_fastmmio*)iobuf;
			printf("HAX_EXIT_FAST_MMIO\n");
			printf("rip = 0x%llx\n", Registers.context._rip);
			printf("direction %d, paddr = 0x%llx, size = %d\n", mmio->direction, mmio->gpa, mmio->size);
			/*
			if (!mmio->direction)
				access_read_physical(mmio->gpa, mmio->size, (void*)&mmio->value);
			else
				access_write_physical(mmio->gpa, mmio->size, (void*)&mmio->value);
			*/
			scanf_s("%d", &Bytes);
			break;
		case HAX_EXIT_PAGEFAULT:
			printf("HAX_EXIT_PAGEFAULT\n");
			printf("gpa = 0x%llx\n", tunnel->pagefault.gpa);
			scanf_s("%d", &Bytes);
			break;
		case HAX_EXIT_DEBUG:
#ifdef __DEBUG__
			printf("HAX_EXIT_DEBUG\n");
#endif
			KdSendSingleStep();
			InBreak = 1;
			do {
				do {
					memset(Buffer, '\0', sizeof(Buffer));
					Status = recvfrom(windbg_sock, (char*)Buffer, sizeof(Buffer), 0, NULL, NULL);
				} while (InBreak && (!Status || Status == -1));
				if (!Status || Status == -1)
					break;
				Status = KdParsePacket((PKD_PACKET)Buffer);
			} while (Status != DbgKdContinueApi2);
			InBreak = 0;
			//SvmmPrintRegisters(&Registers);
			//scanf_s("%d", &Bytes);
			/*
			if (windbg_host) {
				memset(buffer, '\0', sizeof(buffer));
				pkt = (PKD_PACKET)buffer;
				pkt->packetLeader = KD_PACKET_DATA;
				pkt->packetType = KD_PACKET_TYPE_STATE_CHANGE64;
				pkt->length = sizeof(DBGKD_ANY_WAIT_STATE_CHANGE);
				pkt->id = packetId;

				waitChange = (PDBGKD_ANY_WAIT_STATE_CHANGE)(pkt + 1);
				waitChange->NewState = DbgKdExceptionStateChange;
				waitChange->ProcessorLevel = 0;
				waitChange->Processor = 0;
				waitChange->NumberProcessors = 1;
				waitChange->Thread = NULL;
				waitChange->u.Exception.FirstChance = 0x1;
				if (BX_CPU_THIS->eflags & EFLAGS_TF_MASK)
					waitChange->u.Exception.ExceptionRecord.ExceptionCode = STATUS_SINGLE_STEP;
				else
					waitChange->u.Exception.ExceptionRecord.ExceptionCode = STATUS_BREAKPOINT;
				waitChange->u.Exception.ExceptionRecord.ExceptionAddress = (BX_CPU_THIS_PTR cr0.get32() & 1) ? RIP : RIP | (BX_CPU_THIS->sregs[BX_SEG_REG_CS].selector.value << 4);
				waitChange->ProgramCounter = RIP; //waitChange->u.Exception.ExceptionRecord.ExceptionAddress;
				waitChange->u.Exception.ExceptionRecord.ExceptionFlags = 0;
				waitChange->u.Exception.ExceptionRecord.ExceptionRecord = 0;
				waitChange->u.Exception.ExceptionRecord.NumberParameters = 0;
				waitChange->ControlReport.Dr6 = BX_CPU_THIS->dr6.get32();
				waitChange->ControlReport.Dr7 = BX_CPU_THIS->dr7.get32();
				waitChange->ControlReport.EFlags = BX_CPU_THIS->eflags;
				waitChange->ControlReport.ReportFlags = 3;
				waitChange->ControlReport.SegCs = BX_CPU_THIS->sregs[BX_SEG_REG_CS].selector.value;
				waitChange->ControlReport.SegDs = BX_CPU_THIS->sregs[BX_SEG_REG_DS].selector.value;
				waitChange->ControlReport.SegEs = BX_CPU_THIS->sregs[BX_SEG_REG_ES].selector.value;
				waitChange->ControlReport.SegFs = BX_CPU_THIS->sregs[BX_SEG_REG_FS].selector.value;

				BX_CPU_THIS_PTR access_read_physical(waitChange->u.Exception.ExceptionRecord.ExceptionAddress, 16, (void*)waitChange->ControlReport.InstructionStream);
				//memcpy((BYTE*)waitChange->ControlReport.InstructionStream, (BYTE*)BX_CPU_THIS->getHostMemAddr(waitChange->u.Exception.ExceptionRecord.ExceptionAddress, 0), 16);
				KdSendPacketAck(pkt);
				inbreak = 1;
				do {
					do {
						memset(buffer, '\0', sizeof(buffer));
						status = recvfrom(sock, (char*)buffer, sizeof(buffer), 0, NULL, NULL);
					} while (inbreak && (!status || status == -1));
					if (!status || status == -1)
						break;
					pkt = (PKD_PACKET)buffer;
					status = KdParsePacket(pkt);
				} while (status != DbgKdContinueApi2);
				inbreak = 0;
			}
			*/
			break;
		case HAX_EXIT_VMX_TIMER:
#ifdef __DEBUG__
			printf("HAX_EXIT_VMX_TIMER\n");	
#endif
			if (argv[1]) {
				do {
					do {
						memset(Buffer, '\0', sizeof(Buffer));
						Status = recvfrom(windbg_sock, (char*)Buffer, sizeof(Buffer), 0, NULL, NULL);
					} while (InBreak && (!Status || Status == -1));
					if (!Status || Status == -1)
						break;
					Status = KdParsePacket((PKD_PACKET)Buffer);
				} while (Status != DbgKdContinueApi2);
				InBreak = 0;
			}
			TimerTick(100);
			break;
		case HAX_EXIT_RSM:
			printf("HAX_EXIT_RSM\n");
			//return_system_management_mode();
			scanf_s("%d", &Bytes);
			break;
		case HAX_EXIT_MTF:
			/*
			fprintf(fout, "HAX_EXIT_MTF\n");
			fprintf(fout, "rip = 0x%llx\n", context._rip);
			fflush(fout);
			*/
			SvmmPrintRegisters(&Registers);
			scanf_s("%d", &Bytes);
			break;
		default:
			printf("default\n");
			scanf_s("%d", &Bytes);
			break;
		}
#elif GVM
		switch (kvm_run->exit_reason) {
			case GVM_EXIT_IO:
#ifdef __DEBUG__
			printf("GVM_EXIT_IO\n");
			//scanf_s("%d", &Bytes);
#endif
			for (i = 0; i < kvm_run->io.count; i++) {
#ifdef __DEBUG__
				printf("port: 0x%x\n", (uint16_t)kvm_run->io.port);
				printf("size: %d\n", (uint16_t)kvm_run->io.size);
				printf("operation: %s\n", kvm_run->io.direction == GVM_EXIT_IO_IN ? "in" : "out");
#endif
				if (kvm_run->io.direction == GVM_EXIT_IO_IN) {
					*(DWORD*)(pio_data + i * kvm_run->io.size) = ReadPortIoHandler(kvm_run->io.port, kvm_run->io.size);
				}
				else {
					WritePortIoHandler(kvm_run->io.port, *(DWORD*)(pio_data + i * kvm_run->io.size), kvm_run->io.size);
				}
			}
			break;

		case GVM_EXIT_INTR:
#ifdef __DEBUG__
			printf("GVM_EXIT_INTR\n");
			//SvmmPrintRegisters();
			if (kvm_events & USER_EVENT_DMA) {
				DmaSetHoldRequest(0, 0);
				printf("dma %d\n", kvm_run->user_event_pending);
			}
			scanf_s("%d", &Bytes);
#endif
			break;
		case GVM_EXIT_UNKNOWN:
			printf("GVM_EXIT_UNKNOWN\n");
			scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_HLT:
			printf("GVM_EXIT_HLT\n");
			SvmmPrintRegisters(&Registers);
			scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_INTERNAL_ERROR:
			printf("GVM_EXIT_INTERNAL_ERROR\n");
			scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_MMIO:
			//mmio = (struct hax_fastmmio*)iobuf;
#ifdef __DEBUG__
			printf("GVM_EXIT_MMIO\n");
			printf("rip = 0x%llx\n", Registers.context._rip);
			printf("direction %d, paddr = 0x%llx, size = %d\n", kvm_run->mmio.is_write, kvm_run->mmio.phys_addr, kvm_run->mmio.len);
			scanf_s("%d", &Bytes);
#endif
			if (!kvm_run->mmio.is_write)
				ReadMMIOHandler(kvm_run->mmio.phys_addr, (BYTE*)kvm_run->mmio.data, kvm_run->mmio.len);
			else
				WriteMMIOHandler(kvm_run->mmio.phys_addr, (BYTE*)kvm_run->mmio.data, kvm_run->mmio.len);
			
			
			
			break;
		case GVM_EXIT_SHUTDOWN:
			printf("GVM_EXIT_SHUTDOWN\n");
			scanf_s("%d", &Bytes);
			break;
		
		case GVM_EXIT_NMI:
			printf("GVM_EXIT_NMI\n");
			//return_system_management_mode();
			scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_WATCHDOG:
#ifdef __DEBUG__
			printf("GVM_EXIT_WATCHDOG\n");
#endif
			
			if (argv[1] && !Stepping) {
				printf("stepping %d\n", Stepping);
				memset(Buffer, '\0', sizeof(Buffer));
				Status = recvfrom(windbg_sock, (char*)Buffer, sizeof(Buffer), 0, NULL, NULL);
				if (Status == 1 && Buffer[0] == KD_PACKET_BREAK) {
					KdSendSingleStep();
					do {
						do {
							memset(Buffer, '\0', sizeof(Buffer));
							Status = recvfrom(windbg_sock, (char*)Buffer, sizeof(Buffer), 0, NULL, NULL);
						} while (Stepping && (!Status || Status == -1));
						Status = KdParsePacket((PKD_PACKET)Buffer);
					} while (Status != DbgKdContinueApi2);
				}
			}
			

			/*
			if (argv[1] && !Stepping) {
				//printf("watchdog %s\n", Stepping ? "stepping" : "running");
				do {
					do {
						memset(Buffer, '\0', sizeof(Buffer));
						Status = recvfrom(windbg_sock, (char*)Buffer, sizeof(Buffer), 0, NULL, NULL);
					} while (!Status || Status == -1);
					if (!Status || Status == -1)
						break;
					Status = KdParsePacket((PKD_PACKET)Buffer);
				} while (Status != DbgKdContinueApi2);
			}
			*/
			//SvmmPrintRegisters(&Registers);
			TimerTick(0x034fb5e3);
			//SvmmSetRegisters(&Registers);
			//scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_SYSTEM_EVENT:
			printf("GVM_EXIT_SYSTEM_EVENT\n");
			scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_IRQ_WINDOW_OPEN:
			printf("GVM_EXIT_IRQ_WINDOW_OPEN\n");
			kvm_run->request_interrupt_window = 0;
			vector = PicIAC();
			PicInterrupt(vector);
			//DmaSetHoldRequest(0, 1);
			//scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_IOAPIC_EOI:
			printf("GVM_EXIT_SYSTEM_EVENT\n");
			scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_EXCEPTION:
			printf("GVM_EXIT_EXCEPTION\n");
			scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_HYPERCALL:
			printf("GVM_EXIT_HYPERCALL\n");
			scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_DEBUG:
			printf("GVM_EXIT_DEBUG\n");
			//scanf_s("%d", &Bytes);
			KdSendSingleStep();
			do {
				do {
					memset(Buffer, '\0', sizeof(Buffer));
					Status = recvfrom(windbg_sock, (char*)Buffer, sizeof(Buffer), 0, NULL, NULL);
				} while (Stepping && (!Status || Status == -1));
				Status = KdParsePacket((PKD_PACKET)Buffer);
			} while (Status != DbgKdContinueApi2);

			SvmmPrintRegisters(&Registers);
			/*
			InBreak = 1;
			do {
				do {
					memset(Buffer, '\0', sizeof(Buffer));
					Status = recvfrom(windbg_sock, (char*)Buffer, sizeof(Buffer), 0, NULL, NULL);
				} while (InBreak && (!Status || Status == -1));
				if (!Status || Status == -1)
					break;
				Status = KdParsePacket((PKD_PACKET)Buffer);
			} while (Status != DbgKdContinueApi2);
			InBreak = 0;
			*/
			//SvmmSetRegisters(&Registers);
			/*
			if (Registers.context._rflags & EFLAGS_TF_MASK)
				KdSendSingleStep();
			else {
				memset(&kvm_debug, '\0', sizeof(kvm_debug));
				kvm_debug.control = GVM_GUESTDBG_ENABLE | GVM_GUESTDBG_USE_SW_BP;
				DeviceIoControl(hGvmCpu, GVM_SET_GUEST_DEBUG, &kvm_debug, sizeof(kvm_debug), (LPVOID)NULL, 0, &bytes, NULL);

			}
			*/
			//scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_FAIL_ENTRY:
			printf("GVM_EXIT_FAIL_ENTRY\n");
			printf("reason 0x%llx\n", kvm_run->fail_entry.hardware_entry_failure_reason);
			scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_SET_TPR:
			printf("GVM_EXIT_SET_TPR\n");
			scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_TPR_ACCESS:
			printf("GVM_EXIT_TPR_ACCESS\n");
			scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_OSI:
			printf("GVM_EXIT_OSI\n");
			scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_PAPR_HCALL:
			printf("GVM_EXIT_PAPR_HCALL\n");
			scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_MTF:
			printf("GVM_EXIT_MTF\n");
			SvmmPrintRegisters(&Registers);
			scanf_s("%d", &Bytes);

			break;
		default:
			printf("default\n");
			scanf_s("%d", &Bytes);
			break;
		}
#endif
	}

exit:
	VirtualFree(Ram, 0, MEM_RELEASE);

	return 0;
}
