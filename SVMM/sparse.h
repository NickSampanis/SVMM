#pragma once
#include <Windows.h>

// hdimage capabilities
#define HDIMAGE_READONLY      1
#define HDIMAGE_HAS_GEOMETRY  2
#define HDIMAGE_AUTO_GEOMETRY 4

// hdimage format check return values
#define HDIMAGE_FORMAT_OK      0
#define HDIMAGE_SIZE_ERROR    -1
#define HDIMAGE_READ_ERROR    -2
#define HDIMAGE_NO_SIGNATURE  -3
#define HDIMAGE_TYPE_ERROR    -4
#define HDIMAGE_VERSION_ERROR -5

// SPARSE IMAGES HEADER
#define SPARSE_HEADER_MAGIC  (0x02468ace)
#define SPARSE_HEADER_VERSION  2
#define SPARSE_HEADER_V1       1
#define SPARSE_HEADER_SIZE        (256) // Plenty of room for later
#define SPARSE_PAGE_NOT_ALLOCATED (0xffffffff)

typedef struct
{
	DWORD	magic;
	DWORD	version;
	DWORD	pagesize;
	DWORD	numpages;
	ULONG64 disk;
	DWORD	padding[58];
} sparse_header_t;

typedef struct
{
	BYTE   magic[32];
	BYTE   type[16];
	BYTE   subtype[16];
	DWORD  version;
	DWORD  header;
} standard_header_t;

typedef struct sparse {
	sparse_header_t Header;
	ULONG PageSize;
	ULONG NumberOfPages;
	ULONG64 PageTableFileOffet;
	DWORD* PageTable;
	ULONG64 TotalSize;
	ULONG64 HdSize;
	HANDLE hFile;
	DWORD PositionVirtualPage;
	DWORD PositionPhysicalPage;
	DWORD PositionPageOffset;
} sparse;

NTSTATUS SparseInitializeHardDisk(CHAR* Pathname);
ULONG64 SpareGetHardDiskSize(VOID);
LONG64 SparseSeek(ULONG64 Offset);
LONG64 SparseRead(BYTE* Buffer, ULONG64 BufferSize);
LONG64 SparseWrite(BYTE* Buffer, ULONG64 BufferSize);

