#ifndef SVMM_H
#define SVMM_H
//#define __DEBUG__

#include <Windows.h>
#include <stdint.h>
#include <intrin.h>
//#define __DEBUG__ 1
#define GVM		1

#define CPU_MODE_REAL			0
#define CPU_MODE_PROTECTED		1
#define CPU_MODE_LONG_MODE		2

#define GB (1024 * 1024 * 1024)
#define MB (1024 * 1024)
#define BIOSROMSZ		(1 << 21)    /* 2M BIOS ROM @0xffe00000, must be a power of 2 */
#define EXROMSIZE		(0x20000)    /* ROMs 0xc0000-0xdffff (area 0xe0000-0xfffff=bios mapped) */
#define PAGE_SIZE		0x1000
#define ROM_SIZE		(BIOSROMSZ + EXROMSIZE)
#define BIOS_MASK		(BIOSROMSZ - 1)
#define EXROM_MASK		(EXROMSIZE - 1)
#define TYPE_SYSTEM_BIOS  0
#define TYPE_VGA_BIOS	  1	

#define ROM_ADDR_BIOS	  0x00000
#define ROM_ADDR_VGA	  0xc0000

#define EVENT_NMI                          (1 <<  0)
#define EVENT_SMI                          (1 <<  1)
#define EVENT_INIT                         (1 <<  2)
#define EVENT_CODE_BREAKPOINT_ASSIST       (1 <<  3)
#define EVENT_VMX_MONITOR_TRAP_FLAG        (1 <<  4)
#define EVENT_VMX_PREEMPTION_TIMER_EXPIRED (1 <<  5)
#define EVENT_VMX_INTERRUPT_WINDOW_EXITING (1 <<  6)
#define EVENT_VMX_VIRTUAL_NMI              (1 <<  7)
#define EVENT_SVM_VIRQ_PENDING             (1 <<  8)
#define EVENT_PENDING_VMX_VIRTUAL_INTR     (1 <<  9)
#define EVENT_PENDING_INTR                 (1 << 10)
#define EVENT_PENDING_LAPIC_INTR           (1 << 11)
#define EVENT_VMX_VTPR_UPDATE              (1 << 12)
#define EVENT_VMX_VEOI_UPDATE              (1 << 13)
#define EVENT_VMX_VIRTUAL_APIC_WRITE       (1 << 14)
#define EVENT_DMA						   (1 << 15)


#define BX_CR3_PAGING_MASK			0x000ffffffffff000ULL
#define PAGING_MASK					0x000ffffffffff000ULL
#define PAGE_1GB_ENABLED			(1 << 7)
#define PAGE_2MB_ENABLED			(1 << 7)

#define CR0_PG						(1ULL << 31)
#define EFER_LME					(1 << 8)
#define CR4_PAE						(1 << 5)

#define EFLAGS_IF					(1 << 9)
typedef __m128i int128_t;


BYTE* SvmmGetHostPageFromGPA(ULONG64 gpaAddress);
BYTE* SvmmGetHostAddress(ULONG64 GuestAddress);
ULONG64 SvmmGetGpaFromGva(ULONG64 GuestVirtualAddress);

NTSTATUS SvmmSetRegisters(struct Registers* Registers);
NTSTATUS SvmmGetRegisters(struct Registers* Registers);
void SvmmPrintRegisters(void);
VOID SvmmInitializeRegisterState(struct Registers* Registers);
VOID SvmmInterrupt(DWORD Vector);

#endif
