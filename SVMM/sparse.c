#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>

struct sparse Sparse;

ULONG64 SparseGetHardDiskSize(VOID)
{
	return Sparse.HdSize;
}

NTSTATUS SparseInitializeHardDisk(CHAR* Pathname)
{
	DWORD RetSize, i;
	HANDLE hFile;
	LARGE_INTEGER Size, tmp;

	memset(&Sparse, '\0', sizeof(Sparse));

	hFile = CreateFileA(Pathname, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Error in CreateFile 0x%x\n", GetLastError());
		exit(-1);
	}
	Sparse.hFile = hFile;
	if (!GetFileSizeEx(hFile, &Size)) {
		fprintf(stderr, "Error in GetFileSizeEx 0x%x\n", GetLastError());
		exit(-1);
	}
	if (Size.QuadPart < sizeof(standard_header_t)) {
		fprintf(stderr, "Error in GetFileSizeEx 0x%x\n", GetLastError());
		exit(-1);
	}
	if (!ReadFile(hFile, &Sparse.Header, sizeof(Sparse.Header), &RetSize, NULL)) {
		fprintf(stderr, "Error in ReadFile Sparse.Header\n");
		exit(-1);
	}
	if (Sparse.Header.version != SPARSE_HEADER_VERSION) {
		fprintf(stderr, "Error in Header.version 0x%x\n", GetLastError());
		exit(-1);
	}
	Sparse.NumberOfPages = Sparse.Header.numpages;
	Sparse.PageSize = Sparse.Header.pagesize;
	if (Sparse.NumberOfPages * sizeof(DWORD) < Sparse.NumberOfPages) {
		fprintf(stderr, "Error in Header.numpages 0x%x\n", GetLastError());
		exit(-1);
	}
	if (Sparse.PageSize % 0x200) {
		fprintf(stderr, "Error in pagesize alignment 0x%x\n", GetLastError());
		exit(-1);
	}

	Sparse.PageTable = (DWORD *)calloc(sizeof(DWORD), Sparse.NumberOfPages);
	tmp.QuadPart = 0;
	if (!SetFilePointerEx(Sparse.hFile, tmp, &tmp, FILE_CURRENT)) {
		exit(-1);
	}
	Sparse.PageTableFileOffet = tmp.QuadPart;
	if (!ReadFile(hFile, Sparse.PageTable, Sparse.NumberOfPages * sizeof(DWORD), &RetSize, NULL)) {
		fprintf(stderr, "Error in ReadFile 0x%x\n", GetLastError());
		exit(-1);
	}

	if (RetSize != Sparse.NumberOfPages * sizeof(DWORD)) {
		fprintf(stderr, "Error in Reading Header.numpages 0x%x\n", GetLastError());
		exit(-1);
	}
	/*
	printf("harddisk page table\n");
	for (i = 0; i < Sparse.NumberOfPages; i++) {
		printf("page_table[%d] = 0x%x\n", i, Sparse.PageTable[i]);
		scanf_s("%d", &RetSize);
	}
	*/
	Sparse.TotalSize = ((ULONG64)Sparse.NumberOfPages * Sparse.PageSize);
	Sparse.HdSize = Sparse.Header.disk;//_byteswap_uint64(Sparse.Header.disk);

	return 0;
}

LONG64 SparseSeek(ULONG64 Offset)
{
	if (Sparse.TotalSize <= Offset) {
		fprintf(stderr, "Error in SparseSeek() Sparse.TotalSize <= Offset \n");
		exit(-1);
	}

	Sparse.PositionVirtualPage = Offset / Sparse.PageSize;
	if (Sparse.NumberOfPages <= Sparse.PositionVirtualPage) {
		fprintf(stderr, "Error in SparseSeek() Sparse.NumberOfPages <= Sparse.PositionVirtualPage \n");
		exit(-1);
	}
	Sparse.PositionPhysicalPage = Sparse.PageTable[Sparse.PositionVirtualPage];
	Sparse.PositionPageOffset = Offset & (Sparse.PageSize - 1);

	return Sparse.PositionPhysicalPage;
}

static LONG64 SparseReadPage(BYTE* Buffer, ULONG64 BufferSize)
{
	LARGE_INTEGER tmp;
	ULONG64 FileOffset;
	DWORD Ret;
	BOOL Status;

	/* If page doesn't exist zero out the buffer */
	if (Sparse.PositionPhysicalPage == SPARSE_PAGE_NOT_ALLOCATED)
		return memset(Buffer, '\0', BufferSize);

	tmp.QuadPart = ((ULONG64)Sparse.PositionPhysicalPage + Sparse.PositionPageOffset);
	Status = SetFilePointerEx(Sparse.hFile, tmp, &tmp, FILE_CURRENT);
	if (!Status) {
		fprintf(stderr, "Error in SetFilePointerEx 0x%x\n", GetLastError());
		exit(-1);
	}
	Status = ReadFile(Sparse.hFile, Buffer, BufferSize, &Ret, NULL);
	if (!Status) {
		fprintf(stderr, "Error in ReadFile 0x%x\n", GetLastError());
		exit(-1);
	}
	if (BufferSize != Ret) {
		fprintf(stderr, "Error in ReadFile BufferSize != Ret 0x%x\n", GetLastError());
		exit(-1);
	}

}

LONG64 SparseRead(BYTE* Buffer, ULONG64 BufferSize)
{
	ULONG64 i, restOfPage;

	restOfPage = 0;
	for (i = 0; i < BufferSize; i += restOfPage) {
		restOfPage = Sparse.PageSize - Sparse.PositionPageOffset;
		if (BufferSize <= restOfPage)
			restOfPage = BufferSize;
		SparseReadPage(Buffer, restOfPage);
		Sparse.PositionPageOffset += restOfPage;
		if (Sparse.PositionPageOffset == Sparse.PageSize) {
			//next page
			Sparse.PositionVirtualPage++;
			Sparse.PositionPhysicalPage = Sparse.PageTable[Sparse.PositionVirtualPage];
			Sparse.PositionPageOffset = 0;
		}
	}
	return i;
}

static LONG64 SparseWritePage(BYTE* Buffer, ULONG64 BufferSize)
{
	LARGE_INTEGER tmp;
	DWORD Ret;
	BOOL Status;
	BYTE *zeroBuffer;

	if (Sparse.PositionPhysicalPage == SPARSE_PAGE_NOT_ALLOCATED) {
		/* Create new file page at the end of file */
		SetFilePointer(Sparse.hFile, 0, NULL, FILE_END);
		tmp.QuadPart = 0;
		/* Get eof offset */
		Status = SetFilePointerEx(Sparse.hFile, tmp, &tmp, FILE_CURRENT);
		if (!Status) {
			exit(-1);
		}
		zeroBuffer = calloc(sizeof(BYTE), Sparse.PageSize);
		Status = WriteFile(Sparse.hFile, zeroBuffer, Sparse.PageSize, &Ret, NULL);
		if (!Status) {
			fprintf(stderr, "Error in WriteFile  0x%x\n", GetLastError());
			exit(-1);
		}
		if (Sparse.PageSize != Ret) {
			fprintf(stderr, "Error in WriteFile Sparse.PageSize != Ret 0x%x\n", GetLastError());
			exit(-1);
		}
		free(zeroBuffer);

		/* Update Page Table */
		Sparse.PositionPhysicalPage = tmp.QuadPart / Sparse.PageSize;
		Sparse.PageTable[Sparse.PositionVirtualPage] = Sparse.PositionPhysicalPage;
		tmp.QuadPart = Sparse.PageTableFileOffet + sizeof(DWORD) * Sparse.PositionVirtualPage;
		Status = SetFilePointerEx(Sparse.hFile, tmp, &tmp, FILE_CURRENT);
		if (!Status) {
			fprintf(stderr, "Error in SetFilePointerEx  0x%x\n", GetLastError());
			exit(-1);
		}
		Status = WriteFile(Sparse.hFile, &Sparse.PositionPhysicalPage, sizeof(DWORD), &Ret, NULL);
		if (!Status) {
			fprintf(stderr, "Error in WriteFile  0x%x\n", GetLastError());
			exit(-1);
		}
		if (sizeof(DWORD) != Ret) {
			fprintf(stderr, "Error in WriteFile sizeof(DWORD) != Ret 0x%x\n", GetLastError());
			exit(-1);
		}
	}
	tmp.QuadPart = ((ULONG64)Sparse.PositionPhysicalPage + Sparse.PositionPageOffset);
	Status = SetFilePointerEx(Sparse.hFile, tmp, &tmp, FILE_CURRENT);
	if (!Status) {
		fprintf(stderr, "Error in SetFilePointerEx  0x%x\n", GetLastError());
		exit(-1);
	}
	Status = WriteFile(Sparse.hFile, Buffer, BufferSize, &Ret, NULL);
	if (!Status) {
		fprintf(stderr, "Error in WriteFile  0x%x\n", GetLastError());
		exit(-1);
	}
	if (BufferSize != Ret) {
		fprintf(stderr, "Error in WriteFile BufferSize != Ret 0x%x\n", GetLastError());
		exit(-1);
	}
}

LONG64 SparseWrite(BYTE* Buffer, ULONG64 BufferSize)
{
	ULONG64 i, restOfPage;
	restOfPage = 0;
	for (i = 0; i < BufferSize; i += restOfPage) {
		restOfPage = Sparse.PageSize - Sparse.PositionPageOffset;
		if (BufferSize <= restOfPage)
			restOfPage = BufferSize;
		SparseWritePage(Buffer, restOfPage);
		Sparse.PositionPageOffset += restOfPage;
		if (Sparse.PositionPageOffset == Sparse.PageSize) {
			//next page
			Sparse.PositionVirtualPage++;
			Sparse.PositionPhysicalPage = Sparse.PageTable[Sparse.PositionVirtualPage];
			Sparse.PositionPageOffset = 0;
		}
	}
	return i;
}
