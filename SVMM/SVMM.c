#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include "SVMM.h"
#include "i440fx.h"
#include "piix3.h"
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
#include "ide.h"
#include "vgacore.h"
#include "ioapic.h"
#include "apic.h"
#include "pci.h"
#include "acpi.h"
#include "exdi.h"
#include "harddisk.h"
#include "SvmmDbgServer.h"
#include "hpet.h"
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
#endif

extern SOCKET windbg_sock;
extern BYTE Stepping;
extern struct kvm_msrs* msrs;
DWORD svmm_events;

//GLOBALS
BYTE* Ram;
BYTE* MemoryEnd;
ULONG64 RamSize;
struct Registers Registers;
BYTE RequestInterruptWindow;

BYTE *SvmmGetHostPageFromGPA(ULONG64 gpaAddress)
{
	BYTE* hostAddress;

	if (gpaAddress >= 0xFFFE0000 && gpaAddress <= 0xFFFFFFFF) {
		//0xFFFE0000 = Ram + RamSize, 0xFFFF0000 = Ram + RamSize + 0x10000
		gpaAddress &= 0x000FFFFF;
		gpaAddress = ((gpaAddress >> 16) & 1) << 4;
		hostAddress = (BYTE*)Ram + RamSize + gpaAddress;
	}
	else if (gpaAddress >= 0xE0000 && gpaAddress <= 0xFFFFF) {
		hostAddress = (BYTE*)Ram + RamSize + gpaAddress;
	}
	else {
		hostAddress = (BYTE*)Ram + gpaAddress;
	}
	if (hostAddress >= MemoryEnd) {
		fprintf(stderr, "Error hostAddress >= MemoryEnd gpa Address = 0x%llx\n", gpaAddress);
		exit(-1);
	}

	return hostAddress;
}

LONG SvmmGetCpuMode()
{
	if (!(Registers.context._cr0 & 1))
		return CPU_MODE_REAL;
	else if (Registers.context._cr4 & (1 << 5))
		return CPU_MODE_LONG_MODE;
	else
		return CPU_MODE_PROTECTED;
}

LONG64 SvmmGetRip()
{
	LONG Mode;

	Mode = SvmmGetCpuMode();

	if (Mode == CPU_MODE_REAL)
		return Registers.context._rip + ((ULONG64)Registers.context._cs.selector << 4);
	else if (Mode == CPU_MODE_PROTECTED)
		return Registers.context._rip + ((ULONG64)Registers.context._cs.base);
	else
		return Registers.context._rip;
}

//GetGVAFromGPA
/*
ULONG64 Pml4GetGpaFromGva(ULONG64 Gva)
{
	ULONG64 pte, phys;
	int leaf;

	phys = (ULONG64)SvmmGetHostPageFromGPA(Registers.context._cr3 & BX_CR3_PAGING_MASK);
	for (leaf = 3; leaf >= 0; --leaf) {
		pte = phys + ((Gva >> (9 + 9 * leaf)) & ((1 << 9) - 1));
		if (leaf == 2 && *(ULONG64*)pte & PAGE_1GB_ENABLED)
			return (*(ULONG64*)pte & PAGING_MASK) + (Gva & 0x1fffffff);
		if (leaf == 1 && *(ULONG64*)pte & PAGE_2MB_ENABLED)
			return (*(ULONG64*)pte & PAGING_MASK) + (Gva & 0x1fffff);
		phys = (ULONG64)SvmmGetHostPageFromGPA(*(ULONG64*)pte & PAGING_MASK);
	}
	//printf("dword = 0x%x\n", *(DWORD*)(phys + (va & 0xfff)));
	return (*(ULONG64*)pte & PAGING_MASK) + (Gva & 0xfff);

}
*/


static BYTE Pml4Levels[] = { 9, 9, 9, 9 };
static BYTE PdePaeLevels[] = { 9, 9, 2 };
static BYTE PdeNoPaeLevels[] = { 10, 10 };

ULONG64 Pml4GetGpaFromGva(ULONG64 Gva, BYTE* Levels, ULONG LevelSize, ULONG EntrySize)
{
	ULONG64 pte, phys, entry;
	LONG i, bitsSum;
	CHAR leaf;

	phys = (ULONG64)SvmmGetHostPageFromGPA(Registers.context._cr3 & BX_CR3_PAGING_MASK);
	entry = 0;
	for (leaf = LevelSize - 1; leaf >= 0; --leaf) {
		bitsSum = 12;
		for (i = 0; i < leaf; i++)
			bitsSum += Levels[i];

		pte = phys + ((Gva >> bitsSum) & ((1ULL << Levels[leaf]) - 1)) * EntrySize;
		if (EntrySize == sizeof(ULONG))
			entry = *(ULONG *)pte;
		else
			entry = *(ULONG64*)pte;

		if (leaf == 2 && entry & PAGE_1GB_ENABLED)
			return (entry & PAGING_MASK) + (Gva & 0x1fffffff);
		if (leaf == 1 && entry & PAGE_2MB_ENABLED)
			return (entry & PAGING_MASK) + (Gva & 0x1fffff);
		phys = (ULONG64)SvmmGetHostPageFromGPA(entry & PAGING_MASK);
		if (!phys)
			return 0;
	}
	//printf("dword = 0x%x\n", *(DWORD*)(phys + (va & 0xfff)));
	return (entry & PAGING_MASK) + (Gva & 0xfff);

}

ULONG64 SvmmGetGpaFromGva(ULONG64 GuestVirtualAddress)
{
	ULONG64 gpa;

	gpa = 0;
	if (Registers.context._efer & EFER_LME)
		gpa = Pml4GetGpaFromGva(GuestVirtualAddress, Pml4Levels, sizeof(Pml4Levels), 8);
	else if (Registers.context._cr4 & CR4_PAE)
		gpa = Pml4GetGpaFromGva(GuestVirtualAddress, PdePaeLevels, sizeof(PdePaeLevels), 8);
	else 
		gpa = Pml4GetGpaFromGva(GuestVirtualAddress, PdeNoPaeLevels, sizeof(PdeNoPaeLevels), 4);

	return gpa;
}

/* Returns Host Virtual address for any given gpa or gva */
BYTE* SvmmGetHostAddress(ULONG64 GuestAddress)
{
	ULONG64 gpaAddress;

	gpaAddress = GuestAddress;
	/* paging enabled */
	
	if (Registers.context._cr0 & CR0_PG)
		gpaAddress = SvmmGetGpaFromGva(GuestAddress);
	

	return SvmmGetHostPageFromGPA(gpaAddress);
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


void SvmmPrintRegisters()
{
	int i;
	ULONG64 gvaAddress;

	printf("RAX = 0x%llx\n", Registers.context._rax);
	printf("RCX = 0x%llx\n", Registers.context._rcx);
	printf("RDX = 0x%llx\n", Registers.context._rdx);
	printf("RBX = 0x%llx\n", Registers.context._rbx);
	printf("RSP = 0x%llx\n", Registers.context._rsp);
	printf("RBP = 0x%llx\n", Registers.context._rbp);
	printf("RSI = 0x%llx\n", Registers.context._rsi);
	printf("RDI = 0x%llx\n", Registers.context._rdi);
	printf("R8 = 0x%llx\n",  Registers.context._r8);
	printf("R9 = 0x%llx\n",  Registers.context._r9);
	printf("R10 = 0x%llx\n", Registers.context._r10);
	printf("R11 = 0x%llx\n", Registers.context._r11);
	printf("R12 = 0x%llx\n", Registers.context._r12);
	printf("R13 = 0x%llx\n", Registers.context._r13);
	printf("R14 = 0x%llx\n", Registers.context._r14);
	printf("R15 = 0x%llx\n", Registers.context._r15);
	printf("RIP = 0x%llx\n", SvmmGetRip());
	printf("eflgs = 0x%llx\n", Registers.context._rflags);

	printf("idtr.base = 0x%llx\n", Registers.context._idt.base);
	printf("idtr.limit = 0x%lx\n", Registers.context._idt.limit);
	printf("gdtr.base = 0x%llx\n", Registers.context._gdt.base);
	printf("gdtr.limit = 0x%lx\n", Registers.context._gdt.limit);

	printf("cr0 = 0x%llx\n", Registers.context._cr0);
	printf("cr2 = 0x%llx\n", Registers.context._cr2);
	printf("cr3 = 0x%llx\n", Registers.context._cr3);
	printf("cr4 = 0x%llx\n", Registers.context._cr4);

	//try with type = 1
	//try with avail = 1
	printf("\ncs selector = 0x%x\n", Registers.context._cs.selector);
	printf("cs base = 0x%llx\n", Registers.context._cs.base);
	printf("cs limit = 0x%lx\n", Registers.context._cs.limit);
	printf("cs type = 0x%x\n", Registers.context._cs.type);
	printf("cs desc = 0x%x\n", Registers.context._cs.desc);
	printf("cs dpl = 0x%x\n", Registers.context._cs.dpl);
	printf("cs present = 0x%x\n", Registers.context._cs.present);
	printf("cs available = 0x%x\n", Registers.context._cs.available);
	printf("cs long_mode = 0x%x\n", Registers.context._cs.long_mode);
	printf("cs granularity = 0x%x\n\n", Registers.context._cs.granularity);

	printf("\nds selector = 0x%x\n", Registers.context._ds.selector);
	printf("ds base = 0x%llx\n", Registers.context._ds.base);
	printf("ds limit = 0x%lx\n", Registers.context._ds.limit);
	printf("ds type = 0x%x\n", Registers.context._ds.type);
	printf("ds desc = 0x%x\n", Registers.context._ds.desc);
	printf("ds dpl = 0x%x\n", Registers.context._ds.dpl);
	printf("ds present = 0x%x\n", Registers.context._ds.present);
	printf("ds available = 0x%x\n", Registers.context._ds.available);
	printf("ds long_mode = 0x%x\n", Registers.context._ds.long_mode);
	printf("ds granularity = 0x%x\n\n", Registers.context._ds.granularity);

	printf("\ngs selector = 0x%x\n", Registers.context._gs.selector);
	printf("gs base = 0x%llx\n", Registers.context._gs.base);
	printf("gs limit = 0x%lx\n", Registers.context._gs.limit);
	printf("gs type = 0x%x\n", Registers.context._gs.type);
	printf("gs desc = 0x%x\n", Registers.context._gs.desc);
	printf("gs dpl = 0x%x\n", Registers.context._gs.dpl);
	printf("gs present = 0x%x\n", Registers.context._gs.present);
	printf("gs available = 0x%x\n", Registers.context._gs.available);
	printf("gs long_mode = 0x%x\n", Registers.context._gs.long_mode);
	printf("gs granularity = 0x%x\n\n", Registers.context._gs.granularity);


	printf("fs selector = 0x%x\n", Registers.context._fs.selector);
	printf("fs base = 0x%llx\n", Registers.context._fs.base);
	printf("fs limit = 0x%lx\n", Registers.context._fs.limit);
	printf("fs type = 0x%x\n", Registers.context._fs.type);
	printf("fs desc = 0x%x\n", Registers.context._fs.desc);
	printf("fs dpl = 0x%x\n", Registers.context._fs.dpl);
	printf("fs present = 0x%x\n", Registers.context._fs.present);
	printf("fs available = 0x%x\n", Registers.context._fs.available);
	printf("fs long_mode = 0x%x\n", Registers.context._fs.long_mode);
	printf("fs granularity = 0x%x\n\n", Registers.context._fs.granularity);
	
	gvaAddress = SvmmGetHostAddress(SvmmGetRip());
	printf("GUEST RIP = 0x%llx\n", SvmmGetRip());
	printf("HOST  RIP = 0x%llx\n", (ULONG64)gvaAddress);
	for (i = 0; i < 8; i++)
		printf("0x%02x ", ((BYTE *)gvaAddress)[i]);
	printf("\n");

}
double PcFreq;

double StartCounter()
{
	double res;
	LARGE_INTEGER freq, counter;

	QueryPerformanceFrequency(&freq);
	PcFreq = ((double)freq.QuadPart) / 1000.0;

	QueryPerformanceCounter(&counter);
	
	return counter.QuadPart;
}

double GetCounter(ULONG64 StartCounter)
{
	LARGE_INTEGER counter;

	QueryPerformanceCounter(&counter);
	
	return ((double)counter.QuadPart - (double)StartCounter) / PcFreq;
}

//Table 14-1. Initial Processor State BX_CPU_C::reset
VOID SvmmInitializeRegisterState(struct Registers* Registers)
{
	memset(Registers, '\0', sizeof(struct Registers));
	Registers->context._rip = 0x0000FFF0;
	Registers->context._dr6 = DR6_RESERVED;
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

VOID SvmmInterrupt(DWORD Vector)
{
	DWORD Bytes;
	BOOL Ret;


#ifdef HAXM
	Ret = DeviceIoControl(hHaxCpu, HAX_VCPU_IOCTL_INTERRUPT, &Vector, sizeof(Vector), NULL, 0, &Bytes, NULL);
	if (!Ret) {
		fprintf(stderr, "HAX_VCPU_IOCTL_INTERRUPT 0x%x", GetLastError());
		return;
	}
#elif GVM
	struct kvm_interrupt interrupt;

	interrupt.irq = Vector;
	Ret = DeviceIoControl(hGvmCpu, GVM_INTERRUPT, &interrupt, sizeof(interrupt), NULL, 0, &Bytes, NULL);
	if (!Ret) {
		fprintf(stderr, "GVM_INTERRUPT 0x%x", GetLastError());
		return;
	}
#endif

}

int main(int argc, char *argv[])
{
	NTSTATUS Status;
	ULONG64 tsc, counter;
	DWORD Bytes, i, vector, bytes;
	BOOL Ret;
	BYTE Buffer[2048], dbgState;

	
	counter = 0;
	tsc = 0;
	RamSize = 1024 * MB;
	Ram = VirtualAlloc(NULL, RamSize + ROM_SIZE + PAGE_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!Ram) {
		fprintf(stderr, "VirtualAlloc 0x%x", GetLastError());
		return -1;
	}
	MemoryEnd = Ram + RamSize + ROM_SIZE + PAGE_SIZE;
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
	TimerInitialize();
	CmosInitialize();
	PciInitialize();
	ParallelInitialize();
	SerialInitialize();
	I440fxInitialize();
	Piix3Initialize();
	PicInitialize();
	DmaInitialize();
	Ps2Initialize();
	BiosInitialize();
	PitInitialize();
	VgaCoreInitialize();
	IdeInitialize();
	HardDiskInitialize();
	IoApicInitialize();
	ApicInitialize();
	AcpiInitialize();
	HpetInitialize();
	CmosSetupMemory(RamSize);
	//CmosSetRegister(CMOS_BOOT_REG, CMOS_BOOT_CD | (CMOS_BOOT_HD << 4));
	CmosSetRegister(CMOS_BOOT_REG, CMOS_BOOT_HD);
	CmosSetRegister(CMOS_FAST_BOOT, 1);

	//Load Bios At The End Of Ram
	Status = SvmmLoadRom("BIOS-bochs-latest.bin", TYPE_SYSTEM_BIOS);
	/*
	//UEFI future stuff
	Status = SvmmLoadRom("OVMF.fd", TYPE_SYSTEM_BIOS);
	if (Status < 0) {
		fprintf(stderr, "SvmmLoadRom 0x%x", Status);
		goto exit;
	}
	*/
	//UEFI to delete
	//SvmmLoadUefi();
	/*
	HANDLE hFile = CreateFileA("OVMF.fd", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Error in CreateFileA 0x%lx\n", GetLastError());
		return -1;
	}
	DWORD fileSize = GetFileSize(hFile, NULL);
	if (fileSize == INVALID_FILE_SIZE) {
		fprintf(stderr, "Error in GetFileSize fileSize > ROM_SIZE \n");
		return -1;
	}
	BYTE* buf;
	DWORD retSize;
	NTSTATUS status;

	buf = malloc(fileSize);
	memset(buf, 0xff, fileSize);
	SetFilePointer(hFile, fileSize - 128 * 1024, NULL, FILE_BEGIN);
	status = ReadFile(hFile, Ram + RamSize + 0xe0000, 128 * 1024, &retSize, NULL);
	if (!status) {
		fprintf(stderr, "Error in ReadFile 0x%lx\n", GetLastError());
		return -1;
	}
	*/
	//VGA BIOS
	
	Status = SvmmLoadRom("VGABIOS-lgpl-latest.bin", TYPE_VGA_BIOS);
	if (Status < 0) {
		fprintf(stderr, "SvmmLoadRom 0x%x", Status);
		goto exit;
	}
	
	
	SvmmSetRegisters(&Registers);
	if (argv[1]) {
		SvmmDbgInit(argv[1]);
		dbgState = SvmmDbgLoop();
	}
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
		//SvmmPrintRegisters(&Registers);
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
			SvmmPrintRegisters(&Registers);
			/*
			if (kvm_events & USER_EVENT_DMA) {
				DmaSetHoldRequest(0, 0);
				printf("dma %d\n", kvm_run->user_event_pending);
			}
			*/
			scanf_s("%d", &Bytes);
#endif
			
			printf("GVM_EXIT_INTR\n");
			printf("rip = 0x%llx\n", Registers.context._rip);
	
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
				MmioReadHandler(kvm_run->mmio.phys_addr, (BYTE*)kvm_run->mmio.data, kvm_run->mmio.len);
			else
				MmioWriteHandler(kvm_run->mmio.phys_addr, (BYTE*)kvm_run->mmio.data, kvm_run->mmio.len);
			break;
		case GVM_EXIT_SHUTDOWN:
			printf("GVM_EXIT_SHUTDOWN\n");
			scanf_s("%d", &Bytes);
			break;
		
		case GVM_EXIT_NMI:
			printf("GVM_EXIT_NMI\n");
			msrs->nmsrs = 1;
			msrs->entries[0].index = MSR_CORE_PERF_FIXED_CTR0;
			msrs->entries[0].data = (1ULL << 48) - TICK_PERIOD; // | (1 << 3);
			Ret = DeviceIoControl(hGvmCpu, GVM_SET_MSRS,
				msrs, sizeof(struct kvm_msrs) + sizeof(struct kvm_msr_entry) * msrs->nmsrs,
				msrs, sizeof(struct kvm_msrs) + sizeof(struct kvm_msr_entry) * msrs->nmsrs, &bytes,
				(LPOVERLAPPED)NULL);
			TimerTick(TICK_PERIOD);
			//return_system_management_mode();
			scanf_s("%d", &Bytes);
			
			//printf("rip = 0x%llx\n", SvmmGetRip());
			break;
		case GVM_EXIT_WATCHDOG:
#ifdef __DEBUG__
			printf("GVM_EXIT_WATCHDOG\n");
#endif
			
			
			//SvmmSetRegisters(&Registers);
			//scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_SYSTEM_EVENT:
			printf("GVM_EXIT_SYSTEM_EVENT\n");
			scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_IRQ_WINDOW_OPEN:
			//printf("GVM_EXIT_IRQ_WINDOW_OPEN\n");
			//scanf_s("%d", &Bytes);
			kvm_run->request_interrupt_window = 0;
			//printf("irq Open\n");
			if (svmm_events & EVENT_PENDING_INTR)
				vector = PicIAC();
			else if (svmm_events & EVENT_PENDING_LAPIC_INTR)
				vector = ApicAcknowledgeInterrupt();
			else
				continue;
			printf("current rip = 0x%llx\n", Registers.context._rip);
			printf("vector = 0x%x\n", vector);
			if (SvmmGetCpuMode() == CPU_MODE_LONG_MODE) {
				ULONG64* tmp, tmp2;
				tmp = SvmmGetHostAddress(Registers.context._idt.base + vector * 16);
				tmp2 = (*tmp & 0xffff) | ((*tmp >> 32) & 0xffff0000);
				tmp++;
				tmp2 |= (*tmp << 32);
				printf("irq handler 0x%llx\n", tmp2);
			}
			SvmmInterrupt(vector);
			//scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_IOAPIC_EOI:
			printf("GVM_EXIT_SYSTEM_EVENT\n");
			scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_EXCEPTION:
			printf("GVM_EXIT_EXCEPTION\n");
			printf("rip = 0x%llx\n", Registers.context._rip);

			//scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_HYPERCALL:
			printf("GVM_EXIT_HYPERCALL\n");
			scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_DEBUG:
			//printf("GVM_EXIT_DEBUG\n");

			
			if (argv[1]) {
				if (*SvmmGetHostAddress(SvmmGetRip()) == 0xcc || 
					Registers.context._rip == Registers.context._dr0 || 
					Registers.context._rip == Registers.context._dr1 || 
					Registers.context._rip == Registers.context._dr2 || 
					Registers.context._rip == Registers.context._dr3 ||
					dbgState == DBG_TYPE_RUN || dbgState == DBG_TYPE_STEP_OVER) {
					//printf("you should break!?\n");
					//SvmmDbgSend(DBG_TYPE_ON_BREAK, (BYTE*)NULL, 0);
					dbgState = SvmmDbgLoop();
				}
				else if (dbgState == DBG_TYPE_STEP_INTO) {
					dbgState = SvmmDbgLoop();
				}
				
			}
		
			//SvmmPrintRegisters(&Registers);
			
			break;
		case GVM_EXIT_FAIL_ENTRY:
			printf("GVM_EXIT_FAIL_ENTRY\n");
			printf("reason 0x%llx\n", kvm_run->fail_entry.hardware_entry_failure_reason);
			scanf_s("%d", &Bytes);
			break;
		case GVM_EXIT_SET_TPR:
			printf("GVM_EXIT_SET_TPR\n");
			ApicMMIOWriteHandler(APIC_DEFAULT_MMIO + LAPIC_TPR, &kvm_run->cr8, 4);
			printf("rip = 0x%llx\n", Registers.context._rip);

			//scanf_s("%d", &Bytes);
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
			//printf("GVM_EXIT_MTF\n");
			//SvmmPrintRegisters(&Registers);
			//scanf_s("%d", &Bytes);
			printf("rip = 0x%llx\n", Registers.context._rip);
			//printf("rax = 0x%llx\n", Registers.context._rax);
			//printf("rcx = 0x%llx\n", Registers.context._rcx);
			break;
		case GVM_EXIT_VMX_TIMER:
			/*
			printf("GVM_EXIT_VMX_TIMER\n");
			
			if (!counter)
				counter = StartCounter();
			
			printf("counter = %f\n", GetCounter(counter));
			counter = StartCounter();
			*/
			TimerTick(TICK_PERIOD);
			break;
		case GVM_EXIT_MSR:
			printf("GVM_EXIT_MSR\n");
			printf("%s msr[0x%llx] = 0x%llx\n", kvm_run->msr.is_write ? "write" : "read", 
				kvm_run->msr.msr_index, kvm_run->msr.data);
			ApicSetMsr(kvm_run->msr.data);			
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
