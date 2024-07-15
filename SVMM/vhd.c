
#include <stdio.h>
#include <WinSock2.h>
#include "vhd.h"

/* vhd support taken from virtualbox */

static VHDIMAGE vhdImage;

ULONG64 VhdGetHardDiskSize(VOID)
{
	return vhdImage.cbSize;
}

static void VhdLoadDynamicDisk(uint64_t uDynamicDiskHeaderOffset)
{
	DWORD RetSize, i;
	LARGE_INTEGER tmp;
	VHDDynamicDiskHeader vhdDynamicDiskHeader;
	ULONG64 uBlockAllocationTableOffset;

	tmp.QuadPart = uDynamicDiskHeaderOffset;

	if (!SetFilePointerEx(vhdImage.hFile, tmp, &tmp, FILE_BEGIN)) {
		exit(-1);
	}
	if (!ReadFile(vhdImage.hFile, &vhdDynamicDiskHeader, sizeof(VHDDynamicDiskHeader), &RetSize, NULL)) {
		fprintf(stderr, "Error in ReadFile vhdDynamicDiskHeader\n");
		exit(-1);
	}
	if (memcmp(vhdDynamicDiskHeader.Cookie, VHD_DYNAMIC_DISK_HEADER_COOKIE, VHD_DYNAMIC_DISK_HEADER_COOKIE_SIZE)) {
		fprintf(stderr, "Error invalid vhdDynamicDiskHeader.Cookie\n");
		exit(-1);
	}

	vhdImage.cbDataBlock = htonl(vhdDynamicDiskHeader.BlockSize);
	vhdImage.cBlockAllocationTableEntries = htonl(vhdDynamicDiskHeader.MaxTableEntries);

	if (vhdImage.cBlockAllocationTableEntries > (VHD_MAX_SECTORS - 2)) {
		fprintf(stderr, "Error invalid vhdImage.cBlockAllocationTableEntries\n");
		exit(-1);
	}

	vhdImage.cSectorsPerDataBlock = vhdImage.cbDataBlock / VHD_SECTOR_SIZE;

	/*
	 * Every block starts with a bitmap indicating which sectors are valid and which are not.
	 * We store the size of it to be able to calculate the real offset.
	 */
	vhdImage.cbDataBlockBitmap = vhdImage.cSectorsPerDataBlock / 8;
	vhdImage.cDataBlockBitmapSectors = vhdImage.cbDataBlockBitmap / VHD_SECTOR_SIZE;
	/* Round up to full sector size */
	if (vhdImage.cbDataBlockBitmap % VHD_SECTOR_SIZE > 0)
		vhdImage.cDataBlockBitmapSectors++;

	vhdImage.pu8Bitmap = calloc(vhdImage.cbDataBlockBitmap + 8, 1);
	if (!vhdImage.pu8Bitmap) {
		fprintf(stderr, "Error invalid vhdImage.pu8Bitmap\n");
		exit(-1);
	}

	vhdImage.pBlockAllocationTable = (uint32_t*)calloc(vhdImage.cBlockAllocationTableEntries, sizeof(uint32_t));
	if (!vhdImage.pBlockAllocationTable) {
		fprintf(stderr, "Error invalid vhdImage.pBlockAllocationTable\n");
		exit(-1);
	}

	/*
	 * Read the table.
	 */
	vhdImage.uBlockAllocationTableOffset = htonll(vhdDynamicDiskHeader.TableOffset);
	tmp.QuadPart = vhdImage.uBlockAllocationTableOffset;
	if (!SetFilePointerEx(vhdImage.hFile, tmp, &tmp, FILE_BEGIN)) {
		exit(-1);
	}

	if (!ReadFile(vhdImage.hFile, vhdImage.pBlockAllocationTable, vhdImage.cBlockAllocationTableEntries * sizeof(uint32_t), &RetSize, NULL)) {
		fprintf(stderr, "Error in ReadFile vhdImage.pBlockAllocationTable\n");
		exit(-1);
	}


	for (i = 0; i < vhdImage.cBlockAllocationTableEntries; i++)
		vhdImage.pBlockAllocationTable[i] = htonl(vhdImage.pBlockAllocationTable[i]);


	if (vhdImage.uImageFlags & VD_IMAGE_FLAGS_DIFF)
		memcpy(vhdImage.ParentUuid.au8, vhdDynamicDiskHeader.ParentUuid, sizeof(vhdImage.ParentUuid));

}

NTSTATUS VhdInitializeHardDisk(CHAR* Pathname)
{
	DWORD RetSize;
	HANDLE hFile;
	VHDFooter vhdFooter;
	LARGE_INTEGER Size, tmp;
	
	memset(&vhdImage, 0, sizeof(vhdImage));

	hFile = CreateFileA(Pathname, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Error in CreateFile 0x%x\n", GetLastError());
		exit(-1);
	}
	vhdImage.hFile = hFile;

	if (!GetFileSizeEx(hFile, &Size)) {
		fprintf(stderr, "Error in GetFileSizeEx 0x%x\n", GetLastError());
		exit(-1);
	}
	vhdImage.cbSize = Size.QuadPart;

	if (Size.QuadPart < sizeof(VHDFooter)) {
		fprintf(stderr, "Error in GetFileSizeEx 0x%x\n", GetLastError());
		exit(-1);
	}
	tmp.QuadPart = Size.QuadPart - sizeof(VHDFooter);
	if (!SetFilePointerEx(vhdImage.hFile, tmp, &tmp, FILE_BEGIN)) {
		exit(-1);
	}
	if (!ReadFile(hFile, &vhdFooter, sizeof(VHDFooter), &RetSize, NULL)) {
		fprintf(stderr, "Error in ReadFile Sparse.Header\n");
		exit(-1);
	}
	if (memcmp(vhdFooter.Cookie, VHD_FOOTER_COOKIE, VHD_FOOTER_COOKIE_SIZE)) {
		fprintf(stderr, "Error invalid vhdFooter.Cookie\n");
		exit(-1);
	}
	switch (htonl(vhdFooter.DiskType)) {
		case VHD_FOOTER_DISK_TYPE_FIXED:
			vhdImage.uImageFlags |= VD_IMAGE_FLAGS_FIXED;
			break;
		case VHD_FOOTER_DISK_TYPE_DYNAMIC:
			vhdImage.uImageFlags &= ~VD_IMAGE_FLAGS_FIXED;
			VhdLoadDynamicDisk(htonll(vhdFooter.DataOffset));
			//vhdLoadDynamicDisk(vhdFooter.DataOffset);
			break;
		case VHD_FOOTER_DISK_TYPE_DIFFERENCING:
			vhdImage.uImageFlags |= VD_IMAGE_FLAGS_DIFF;
			vhdImage.uImageFlags &= ~VD_IMAGE_FLAGS_FIXED;
			VhdLoadDynamicDisk(htonll(vhdFooter.DataOffset));
			//vhdLoadDynamicDisk(vhdFooter.DataOffset);
			break;
		default:
			fprintf(stderr, "Error invalid DiskType\n");
			exit(-1);
	}

	vhdImage.cbSize = htonll(vhdFooter.CurSize);
	vhdImage.LCHSGeometry.cCylinders = 0;
	vhdImage.LCHSGeometry.cHeads = 0;
	vhdImage.LCHSGeometry.cSectors = 0;
	vhdImage.PCHSGeometry.cCylinders = htons(vhdFooter.DiskGeometryCylinder);
	vhdImage.PCHSGeometry.cHeads = vhdFooter.DiskGeometryHeads;
	vhdImage.PCHSGeometry.cSectors = vhdFooter.DiskGeometrySectors;


	memcpy(&vhdImage.vhdFooterCopy, &vhdFooter, sizeof(VHDFooter));
	memcpy(&vhdImage.ImageUuid, &vhdFooter.UniqueID, sizeof(vhdFooter.UniqueID));
	vhdImage.u64DataOffset = htonll(vhdFooter.DataOffset);
	
	PVDREGIONDESC pRegion = &vhdImage.RegionList.aRegions[0];
	vhdImage.RegionList.fFlags = 0;
	vhdImage.RegionList.cRegions = 1;

	pRegion->offRegion = 0; /* Disk start. */
	pRegion->cbBlock = 512;
	pRegion->enmDataForm = VDREGIONDATAFORM_RAW;
	pRegion->enmMetadataForm = VDREGIONMETADATAFORM_NONE;
	pRegion->cbData = 512;
	pRegion->cbMetadata = 0;
	pRegion->cRegionBlocksOrBytes = vhdImage.cbSize;
}

LONG64 VhdSeek(ULONG64 Offset)
{
	vhdImage.u64DataOffset = Offset;
}

LONG64 VhdRead(BYTE* Buffer, ULONG64 BufferSize)
{
	ULONG64 i, restOfPage, uVhdOffset;
	DWORD cBlockAllocationTableEntry, cBATEntryIndex, RetSize, cSectors;
	LARGE_INTEGER tmp;

	if (vhdImage.pBlockAllocationTable)
	{
		/*
		 * Get the data block first.
		 */
		cBlockAllocationTableEntry = (vhdImage.u64DataOffset / VHD_SECTOR_SIZE) / vhdImage.cSectorsPerDataBlock;
		cBATEntryIndex = (vhdImage.u64DataOffset / VHD_SECTOR_SIZE) % vhdImage.cSectorsPerDataBlock;
		
		/*
		 * Clip read range to remain in this data block.
		 */
		//cbToRead = RT_MIN(cbToRead, (vhdImage.cbDataBlock - (cBATEntryIndex * VHD_SECTOR_SIZE)));

		/*
		 * If the block is not allocated the content of the entry is ~0
		 */
		if (vhdImage.pBlockAllocationTable[cBlockAllocationTableEntry] == 0xffffffff) {
			//rc = VERR_VD_BLOCK_FREE;
			exit(-1);
		}
		
		/*
		tmp.QuadPart = vhdImage.pBlockAllocationTable[cBlockAllocationTableEntry] * VHD_SECTOR_SIZE;
		if (!SetFilePointerEx(vhdImage.hFile, tmp, &tmp, FILE_BEGIN)) {
			exit(-1);
		}
		if (!ReadFile(vhdImage.hFile, vhdImage.pu8Bitmap, vhdImage.cbDataBlockBitmap, &RetSize, NULL)) {
			fprintf(stderr, "Error in ReadFile Sparse.Header\n");
			exit(-1);
		}
		*/
		uVhdOffset = ((ULONG64)vhdImage.pBlockAllocationTable[cBlockAllocationTableEntry] + vhdImage.cDataBlockBitmapSectors + cBATEntryIndex) * VHD_SECTOR_SIZE;

		tmp.QuadPart = uVhdOffset;
		if (!SetFilePointerEx(vhdImage.hFile, tmp, &tmp, FILE_BEGIN)) {
			exit(-1);
		}
		if (!ReadFile(vhdImage.hFile, Buffer, BufferSize, &RetSize, NULL)) {
			fprintf(stderr, "Error in ReadFile Sparse.Header\n");
			exit(-1);
		}
		/*
		if (vhdImage.pu8Bitmap[cBATEntryIndex / 8] & (1 << (7 - (cBATEntryIndex % 8)))) {
			cBATEntryIndex++;
			cSectors = 1;

			while ((cSectors < (BufferSize / VHD_SECTOR_SIZE))
				&& vhdImage.pu8Bitmap[cBATEntryIndex / 8] & (1 << (7 - (cBATEntryIndex % 8))))
			{
				cBATEntryIndex++;
				cSectors++;
			}

			//BufferSize = cSectors * VHD_SECTOR_SIZE;

			tmp.QuadPart = uVhdOffset;
			if (!SetFilePointerEx(vhdImage.hFile, tmp, &tmp, FILE_BEGIN)) {
				exit(-1);
			}
			if (!ReadFile(vhdImage.hFile, Buffer, BufferSize, &RetSize, NULL)) {
				fprintf(stderr, "Error in ReadFile Sparse.Header\n");
				exit(-1);
			}
		}
		else {
			cBATEntryIndex++;
			cSectors = 1;

			while ((cSectors < (BufferSize / VHD_SECTOR_SIZE))
				&& !vhdImage.pu8Bitmap[cBATEntryIndex / 8] & (1 << (7 - (cBATEntryIndex % 8))))
			{
				cBATEntryIndex++;
				cSectors++;
			}

			BufferSize = cSectors * VHD_SECTOR_SIZE;
		}
		*/
	}
	else {
		tmp.QuadPart = vhdImage.u64DataOffset;
		if (!SetFilePointerEx(vhdImage.hFile, tmp, &tmp, FILE_BEGIN)) {
			exit(-1);
		}
		if (!ReadFile(vhdImage.hFile, Buffer, BufferSize, &RetSize, NULL)) {
			fprintf(stderr, "Error in ReadFile Sparse.Header\n");
			exit(-1);
		}

	}
	//if (pcbActuallyRead)
	//	*pcbActuallyRead = cbToRead;
}

LONG64 VhdWrite(BYTE* Buffer, ULONG64 BufferSize)
{

}
