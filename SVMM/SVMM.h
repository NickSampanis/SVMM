#ifndef SVMM_H
#define SVMM_H
//#define __DEBUG__

#include <Windows.h>
#include <stdint.h>
#include <intrin.h>
//#define __DEBUG__ 1
#define GVM		1

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


typedef __m128i int128_t;



BYTE* SvmmGetHostAddress(ULONG64 GuestVa);
NTSTATUS SvmmSetRegisters(struct Registers* Registers);
NTSTATUS SvmmGetRegisters(struct Registers* Registers);
void SvmmPrintRegisters(struct Registers* Registers);

#endif
