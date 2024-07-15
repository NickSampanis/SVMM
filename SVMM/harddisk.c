#include "harddisk.h"
#include "pci.h"
#include "pic.h"
#include "ide.h"
#include "cmos.h"
#include "timer.h"
#include "cdrom.h"
//#include "sparse.h"
#include "vhd.h"
#include <stdio.h>

ATA_CHANNEL AtaChannel[4];

#define Read8(x)  (*(BYTE *)(x))
#define Read16(x) (htons(*(WORD *)(x)))
#define Read32(x) (htonl(*(DWORD *)(x)))

VOID HardDiskRaiseInterrupt(BYTE Channel)
{
	BYTE Dnum;

	Dnum = AtaChannel[Channel].DriveSelect ? 1 : 0;

	if (!AtaChannel[Channel].Drive[Dnum].Controller.DisableIrq) {
		IdeSetIrq(Channel);
		PicRaiseIrq(AtaChannel[Channel].Irq);
	}
}

VOID HardDiskLba48Transform(BYTE Channel, BYTE Lba48)
{
	BYTE Dnum;

	Dnum = AtaChannel[Channel].DriveSelect ? 1 : 0;

	AtaChannel[Channel].Drive[Dnum].Controller.Lba48 = Lba48;
	if (!AtaChannel[Channel].Drive[Dnum].Controller.Lba48) {
		if (!AtaChannel[Channel].Drive[Dnum].Controller.SectorCount)
			AtaChannel[Channel].Drive[Dnum].Controller.NumberOfSectors = 256;
		else
			AtaChannel[Channel].Drive[Dnum].Controller.NumberOfSectors = AtaChannel[Channel].Drive[Dnum].Controller.SectorCount;
	}
	else {
		if (!AtaChannel[Channel].Drive[Dnum].Controller.SectorCount && !AtaChannel[Channel].Drive[Dnum].Controller.Hob.Nsector)
			AtaChannel[Channel].Drive[Dnum].Controller.NumberOfSectors = 65536;
		else
			AtaChannel[Channel].Drive[Dnum].Controller.NumberOfSectors = (AtaChannel[Channel].Drive[Dnum].Controller.Hob.Nsector << 8) 
			| AtaChannel[Channel].Drive[Dnum].Controller.SectorCount;
	}
}



VOID HardDiskCommandNop(BYTE Channel)
{
	BYTE Dnum;

	Dnum = AtaChannel[Channel].DriveSelect ? 1 : 0;

	AtaChannel[Channel].Drive[Dnum].Controller.interrupt_reason.i_o = 1;
	AtaChannel[Channel].Drive[Dnum].Controller.interrupt_reason.c_d = 1;
	AtaChannel[Channel].Drive[Dnum].Controller.interrupt_reason.rel = 0;

	AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
	AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
	AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_REQ;
	AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_ERROR;
}

VOID HardDiskCommandAborted(BYTE Channel)
{
	BYTE Dnum;

	Dnum = AtaChannel[Channel].DriveSelect ? 1 : 0;

	AtaChannel[Channel].Drive[Dnum].Controller.CurrentCommand = 0;
	AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
	AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_ERROR;
	AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
	AtaChannel[Channel].Drive[Dnum].Controller.ErrorRegister = 4;
	AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_REQ;
	AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_CORRECTED_DATA;
	AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex = 0;

	HardDiskRaiseInterrupt(Channel);
}

VOID HardDiskInitModeSense(BYTE Channel, const void* Src, int Size)
{
	BYTE Dnum;

	Dnum = AtaChannel[Channel].DriveSelect ? 1 : 0;

	memset(AtaChannel[Channel].Drive[Dnum].Controller.Buffer, '\0', 7 + Size);
	AtaChannel[Channel].Drive[Dnum].Controller.Buffer[0] = (Size + 6) >> 8;
	AtaChannel[Channel].Drive[Dnum].Controller.Buffer[1] = (Size + 6) & 0xff;
	AtaChannel[Channel].Drive[Dnum].Controller.Buffer[2] = 0x12; // media present 120mm CD-ROM (CD-R) data/audio  door close
	
	if (!Src) {
		AtaChannel[Channel].Drive[Dnum].Controller.Buffer[8] = 0x01;
		AtaChannel[Channel].Drive[Dnum].Controller.Buffer[9] = 0x06;
		AtaChannel[Channel].Drive[Dnum].Controller.Buffer[10] = 0x00;
		AtaChannel[Channel].Drive[Dnum].Controller.Buffer[11] = 0x05; // Try to recover 5 times
		AtaChannel[Channel].Drive[Dnum].Controller.Buffer[12] = 0x00;
		AtaChannel[Channel].Drive[Dnum].Controller.Buffer[13] = 0x00;
		AtaChannel[Channel].Drive[Dnum].Controller.Buffer[14] = 0x00;
		AtaChannel[Channel].Drive[Dnum].Controller.Buffer[15] = 0x00;
	}
	else {
		memmove(AtaChannel[Channel].Drive[Dnum].Controller.Buffer + 8, Src, Size);
	}
}

VOID HardDiskAtapiError(BYTE Channel, BYTE SenseKey, BYTE Asc, SIZE_T Size)
{
	BYTE Dnum;

	Dnum = AtaChannel[Channel].DriveSelect ? 1 : 0;

	AtaChannel[Channel].Drive[Dnum].Controller.ErrorRegister = SenseKey << 4;
	AtaChannel[Channel].Drive[Dnum].Controller.interrupt_reason.i_o = 1;
	AtaChannel[Channel].Drive[Dnum].Controller.interrupt_reason.c_d = 1;
	AtaChannel[Channel].Drive[Dnum].Controller.interrupt_reason.rel = 0;

	AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
	AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_ERROR;
	AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
	AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_REQ;
	AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_WRITE_FAULT;
	
	AtaChannel[Channel].Drive[Dnum].Controller.Sense.sense_key = SenseKey;
	AtaChannel[Channel].Drive[Dnum].Controller.Sense.asc = Asc;
	AtaChannel[Channel].Drive[Dnum].Controller.Sense.ascq = 0;
	
}
VOID HardDiskReadySendAtapi(BYTE Channel)
{
	BYTE Dnum;

	Dnum = AtaChannel[Channel].DriveSelect ? 1 : 0;
	AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
	AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_REQ;
	AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_ERROR;
	AtaChannel[Channel].Drive[Dnum].Controller.interrupt_reason.i_o = 1;
	AtaChannel[Channel].Drive[Dnum].Controller.interrupt_reason.c_d = 0;
	if (AtaChannel[Channel].Drive[Dnum].Controller.PacketDma) {
		IdeStartTransfer(Channel);
	}
	else {
		HardDiskRaiseInterrupt(Channel);
	}
}

//increments the logical sector and calculates chs
ULONG64 HardDiskCalculateCHS(BYTE Channel, ULONG64 LogicalSector)
{
	BYTE Dnum;

	Dnum = AtaChannel[Channel].DriveSelect ? 1 : 0;

	AtaChannel[Channel].Drive[Dnum].Controller.SectorCount--;
	AtaChannel[Channel].Drive[Dnum].Controller.NumberOfSectors--;

	if (AtaChannel[Channel].Drive[Dnum].Controller.AddressMode) {
		LogicalSector++;
		if (AtaChannel[Channel].Drive[Dnum].Controller.Lba48) {
			/*
				 40                32              24          16                8                0
			[CylinderNumber][CylinderNumber][sectorNumber][cylinderNumber][cylinderNumber][sectorNumber]
			*/
			//lba48
			AtaChannel[Channel].Drive[Dnum].Controller.Hob.HighCylinder = (LogicalSector >> 40) & 0xff;
			AtaChannel[Channel].Drive[Dnum].Controller.Hob.LowCylinder = (LogicalSector >> 32) & 0xff;
			AtaChannel[Channel].Drive[Dnum].Controller.Hob.Sector = (LogicalSector >> 24) & 0xff;
			AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber = (LogicalSector >> 8) & 0xffff;
			AtaChannel[Channel].Drive[Dnum].Controller.SectorNumber = LogicalSector & 0xff;
		}
		else {
			//lba28
			AtaChannel[Channel].Drive[Dnum].Controller.HeadNumber = (LogicalSector >> 24) & 0xff;
			AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber = (WORD)(LogicalSector >> 8) & 0xffff;
			AtaChannel[Channel].Drive[Dnum].Controller.SectorNumber = LogicalSector & 0xff;
		}
	}
	else {
		//CHS
		AtaChannel[Channel].Drive[Dnum].Controller.SectorNumber++;
		if (AtaChannel[Channel].Drive[Dnum].Controller.SectorNumber > AtaChannel[Channel].Drive[Dnum].Spt) {
			AtaChannel[Channel].Drive[Dnum].Controller.SectorNumber = 1;
			AtaChannel[Channel].Drive[Dnum].Controller.HeadNumber++;

			if (AtaChannel[Channel].Drive[Dnum].Controller.SectorNumber >= AtaChannel[Channel].Drive[Dnum].Heads) {
				AtaChannel[Channel].Drive[Dnum].Controller.SectorNumber = 0;
				AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber++;
				if (AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber >= AtaChannel[Channel].Drive[Dnum].Cylinders) {
					AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber = AtaChannel[Channel].Drive[Dnum].Cylinders - 1;
				}
			}
		}
	}
	
	return LogicalSector;
}

ULONG64 HardDiskCalculateLogicalAddress(BYTE Channel)
{
	BYTE Dnum;
	ULONG64 LogicalSector;

	Dnum = AtaChannel[Channel].DriveSelect ? 1 : 0;
	LogicalSector = 0;

	if (AtaChannel[Channel].Drive[Dnum].Controller.AddressMode) {
		if (AtaChannel[Channel].Drive[Dnum].Controller.Lba48) {
			/*
			 40                32              24          16                8                0
			[CylinderNumber][CylinderNumber][sectorNumber][cylinderNumber][cylinderNumber][sectorNumber]
			*/
			LogicalSector = (ULONG64)AtaChannel[Channel].Drive[Dnum].Controller.Hob.HighCylinder << 40 |
				(ULONG64)AtaChannel[Channel].Drive[Dnum].Controller.Hob.LowCylinder << 32 |
				(ULONG64)AtaChannel[Channel].Drive[Dnum].Controller.Hob.Sector << 24 |
				(ULONG64)AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber << 8 |
				(ULONG64)AtaChannel[Channel].Drive[Dnum].Controller.SectorNumber;
		}
		else {
			//lba28
			LogicalSector = ((ULONG64)AtaChannel[Channel].Drive[Dnum].Controller.HeadNumber & 0xff) << 24 |
				((ULONG64)AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber & 0xffff) << 8 |
				(ULONG64)AtaChannel[Channel].Drive[Dnum].Controller.SectorNumber & 0xff;
		}
	}
	else {
		//CHS
		LogicalSector = (ULONG64)AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber * HARDDISK_HEADS * HARDDISK_SECTORS
			+ (ULONG64)AtaChannel[Channel].Drive[Dnum].Controller.HeadNumber * HARDDISK_SECTORS
			+ ((ULONG64)AtaChannel[Channel].Drive[Dnum].Controller.SectorNumber - 1);
	}

	if (VhdGetHardDiskSize() / 512 <= LogicalSector)
		return -1;
	return LogicalSector;
}

VOID HardDiskSetSignature(BYTE Channel, BYTE Dnum)
{
	AtaChannel[Channel].Drive[Dnum].Controller.HeadNumber = 0;
	AtaChannel[Channel].Drive[Dnum].Controller.SectorCount = 1;
	AtaChannel[Channel].Drive[Dnum].Controller.SectorNumber = 1;

	if (AtaChannel[Channel].Drive[Dnum].DriveType == DRIVE_TYPE_DISK) {
		AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber = 0;
		AtaChannel[Channel].DriveSelect = 0;
	}
	else if (AtaChannel[Channel].Drive[Dnum].DriveType == DRIVE_TYPE_CDROM) {
		AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber = 0xeb14;
	}
	else {
		AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber = 0xffff;
	}

}
VOID HardDiskInitAtapiCommand(BYTE Channel, BYTE Command, DWORD ReqLength, DWORD AllocLength)
{
	BYTE Dnum;

	Dnum = AtaChannel[Channel].DriveSelect ? 1 : 0;

	if (AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber == -1)
		AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber = 0xfffe;

	if ((AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber & 1)
		&& AllocLength > AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber)
		AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber--;

	if (!AtaChannel[Channel].Drive[Dnum].Controller.PacketDma) {
		if (!AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber)
			return;
	}
	if (!AllocLength)
		AllocLength = AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber;

	AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_BUSY;
	AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
	AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_REQ;
	AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_ERROR;
	AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex = 0;
	AtaChannel[Channel].Drive[Dnum].Controller.DrqIndex = 0;

	if (AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber > ReqLength)
		AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber = ReqLength;

	if (AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber > AllocLength)
		AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber = AllocLength;

	AtaChannel[Channel].Drive[Dnum].Controller.Atapi.AtapiCommand = Command;
	AtaChannel[Channel].Drive[Dnum].Controller.Atapi.DrqBytes = AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber;
	AtaChannel[Channel].Drive[Dnum].Controller.Atapi.TotalBytesRemaining = (ReqLength < AllocLength) ? ReqLength : AllocLength;
}

VOID HardDiskBdmaComplete(BYTE Channel)
{
	BYTE Dnum;
	LONG64 ret;

	Dnum = AtaChannel[Channel].DriveSelect ? 1 : 0;

	AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
	AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
	AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_REQ;
	AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_ERROR;
	if (AtaChannel[Channel].Drive[Dnum].DriveType == DRIVE_TYPE_CDROM) {
		AtaChannel[Channel].Drive[Dnum].Controller.interrupt_reason.i_o = 1;
		AtaChannel[Channel].Drive[Dnum].Controller.interrupt_reason.c_d = 1;
		AtaChannel[Channel].Drive[Dnum].Controller.interrupt_reason.rel = 0;
	}
	else {
		AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_WRITE_FAULT;
		AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_SEEK_COMP;
		AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_CORRECTED_DATA;
		AtaChannel[Channel].Drive[Dnum].CurrentLSector = AtaChannel[Channel].Drive[Dnum].NextLSector;
	}
	HardDiskRaiseInterrupt(Channel);
}

ULONG64 HardDiskWriteSector(BYTE Channel, BYTE* Buffer, DWORD BufferSize)
{
	BYTE Dnum;
	ULONG64 LogicalSector, i;
	LONG64 ret;

	Dnum = AtaChannel[Channel].DriveSelect ? 1 : 0;

	for (i = 0; i < AtaChannel[Channel].Drive[Dnum].Controller.BufferSize; i += HARDDISK_SECTOR_SIZE) {
		LogicalSector = HardDiskCalculateLogicalAddress(Channel);
		if (LogicalSector == -1) {
			HardDiskCommandAborted(Channel);
			return 0;
		}
		//ret = SparseSeek(LogicalSector * HARDDISK_SECTOR_SIZE);
		ret = VhdSeek(LogicalSector * HARDDISK_SECTOR_SIZE);

		/*
		if (!ret) {
			HardDiskCommandAborted(Channel);
			return 0;
		}
		*/
		ret = VhdWrite(AtaChannel[Channel].Drive[Dnum].Controller.Buffer + i, HARDDISK_SECTOR_SIZE);
		//ret = SparseWrite(AtaChannel[Channel].Drive[Dnum].Controller.Buffer, HARDDISK_SECTOR_SIZE);
		if (!ret) {
			HardDiskCommandAborted(Channel);
			return 0;
		}

		LogicalSector = HardDiskCalculateCHS(Channel, LogicalSector);
		AtaChannel[Channel].Drive[Dnum].NextLSector = LogicalSector;
	}
	return i;

}

ULONG64 HardDiskBmdmaWriteSector(BYTE Channel, BYTE* Buffer, DWORD BufferSize)
{
	BYTE Dnum;
	ULONG64 LogicalSector, i;
	LONG64 ret;

	Dnum = AtaChannel[Channel].DriveSelect ? 1 : 0;

	if (AtaChannel[Channel].Drive[Dnum].Controller.CurrentCommand == IDE_COMMAND_WRITE_DMA
		|| AtaChannel[Channel].Drive[Dnum].Controller.CurrentCommand == IDE_COMMAND_WRITE_DMA_EXT) {
		HardDiskCommandAborted(Channel);
		return 0;
	}
	if (!AtaChannel[Channel].Drive[Dnum].Controller.NumberOfSectors)
		return 0;

	ret = HardDiskWriteSector(Channel, Buffer, BufferSize);

	return ret;
}

ULONG64 HardDiskReadSector(BYTE Channel, BYTE* Buffer, DWORD BufferSize)
{
	BYTE Dnum;
	ULONG64 LogicalSector, i;
	LONG64 ret;

	Dnum = AtaChannel[Channel].DriveSelect ? 1 : 0;

	for (i = 0; i < AtaChannel[Channel].Drive[Dnum].Controller.BufferSize; i += HARDDISK_SECTOR_SIZE) {
		LogicalSector = HardDiskCalculateLogicalAddress(Channel);
		/*
		if (LogicalSector == -1) {
			HardDiskCommandAborted(Channel);
			return 0;
		}
		*/
		//ret = SparseSeek(LogicalSector * HARDDISK_SECTOR_SIZE);
		ret = VhdSeek(LogicalSector * HARDDISK_SECTOR_SIZE);
		/*
		if (!ret) {
			HardDiskCommandAborted(Channel);
			return 0;
		}
		*/
		//ret = SparseRead(AtaChannel[Channel].Drive[Dnum].Controller.Buffer, HARDDISK_SECTOR_SIZE);
		ret = VhdRead(AtaChannel[Channel].Drive[Dnum].Controller.Buffer + i, HARDDISK_SECTOR_SIZE);
		/*
		if (!ret) {
			HardDiskCommandAborted(Channel);
			return 0;
		}
		*/
		LogicalSector = HardDiskCalculateCHS(Channel, LogicalSector);
		AtaChannel[Channel].Drive[Dnum].NextLSector = LogicalSector;
	}
	return i;
}

ULONG64 HardDiskBmdmaReadSector(BYTE Channel, BYTE* Buffer, DWORD BufferSize)
{
	BYTE Dnum;
	ULONG64 LogicalSector, i;
	LONG64 ret;

	Dnum = AtaChannel[Channel].DriveSelect ? 1 : 0;

	if (AtaChannel[Channel].Drive[Dnum].Controller.CurrentCommand == IDE_COMMAND_READ_DMA
		|| AtaChannel[Channel].Drive[Dnum].Controller.CurrentCommand == IDE_COMMAND_READ_DMA_EXT) {
		if (!AtaChannel[Channel].Drive[Dnum].Controller.NumberOfSectors)
			return 0;
		ret = HardDiskReadSector(Channel, Buffer, HARDDISK_SECTOR_SIZE);
		if (!ret)
			return 0;

	}
	else if (AtaChannel[Channel].Drive[Dnum].Controller.CurrentCommand == IDE_COMMAND_ATAPI_PACKET) {
		if (AtaChannel[Channel].Drive[Dnum].Controller.PacketDma) {
			switch (AtaChannel[Channel].Drive[Dnum].Controller.Atapi.AtapiCommand) {
				case 0x28: // read (10)
				case 0xa8: // read (12)
				case 0xbe: // read cd
					ret = CdRomReadBlock(AtaChannel[Channel].Drive[Dnum].Controller.LbAddress, 
						Buffer,
						AtaChannel[Channel].Drive[Dnum].Controller.BufferSize);
					if (!ret)
						return 0;
					AtaChannel[Channel].Drive[Dnum].Controller.LbAddress++;
					AtaChannel[Channel].Drive[Dnum].Controller.RemainingBlocks--;
					/*
					if (!BX_SELECTED_DRIVE(channel).cdrom.remaining_blocks) {
						BX_SELECTED_DRIVE(channel).cdrom.curr_lba = BX_SELECTED_DRIVE(channel).cdrom.next_lba;
					}
					*/
					break;
				default:
					if (AtaChannel[Channel].Drive[Dnum].Controller.BufferSize > AtaChannel[Channel].Drive[Dnum].Controller.RemainingBlocks) {
						memcpy(Buffer, AtaChannel[Channel].Drive[Dnum].Controller.Buffer, AtaChannel[Channel].Drive[Dnum].Controller.RemainingBlocks);
					}
					else {
						memcpy(Buffer, AtaChannel[Channel].Drive[Dnum].Controller.Buffer, AtaChannel[Channel].Drive[Dnum].Controller.BufferSize);
					}
					break;
				}
		}
		else {
			HardDiskCommandAborted(Channel);
			return 0;
		}
	}
	else {
		HardDiskCommandAborted(Channel);
		return 0;
	}
	return 1;
}

VOID HardDiskPortIoWriteHandler(ULONG Address, ULONG Value, ULONG Length)
{
	CHAR serial_number[21];
	BYTE Channel, Dnum, PrevReset, atapiCommand;
	DWORD Port, i, transferLength;
	ULONG64 temp;

	for (Channel = 0; Channel < MAX_ATA_CHANNEL; Channel++) {
		if ((Address & 0xfff8) == AtaChannel[Channel].IoAddr1) {
			Port = (DWORD)Address - AtaChannel[Channel].IoAddr1;
			break;
		}
		if ((Address & 0xfff8) == AtaChannel[Channel].IoAddr2) {
			Port = (DWORD)Address - AtaChannel[Channel].IoAddr2 + 0x10;
			break;
		}
	}
	if (Channel == MAX_ATA_CHANNEL) {
		Port = (DWORD)Address - 0x3e0;
		Channel = 0;
	}
	Dnum = AtaChannel[Channel].DriveSelect ? 1 : 0;
	temp = 0;
	transferLength = 0;
	switch (Port) {
		case 0:
			switch (AtaChannel[Channel].Drive[Dnum].Controller.CurrentCommand) {
				case IDE_COMMAND_WRITE: // WRITE SECTORS
				case IDE_COMMAND_WRITE_MULTIPLE: // WRITE MULTIPLE SECTORS
				case IDE_COMMAND_WRITE_EXT: // WRITE SECTORS EXT
				case IDE_COMMAND_WRITE_MULTIPLE_EXT: // WRITE MULTIPLE EXT
					if (AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex >= AtaChannel[Channel].Drive[Dnum].Controller.BufferSize) {
						fprintf(stderr, "Error Controller.BufferIndex > Controller.BufferSize\n");
						exit(-1);
					}
					switch (Length) {
						case 4:
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex + 3] = Value >> 24;
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex + 2] = Value >> 16;
						case 2:
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex + 1] = Value >> 8;
						case 1:
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex] = Value;
					}
					AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex += Length;
					if (AtaChannel[Channel].Drive[Dnum].Controller.BufferSize > AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex)
						break;
					temp = HardDiskWriteSector(Channel, AtaChannel[Channel].Drive[Dnum].Controller.Buffer, AtaChannel[Channel].Drive[Dnum].Controller.BufferSize);
					if (!temp)
						break;
					if (AtaChannel[Channel].Drive[Dnum].Controller.CurrentCommand == IDE_COMMAND_WRITE_MULTIPLE
						|| AtaChannel[Channel].Drive[Dnum].Controller.CurrentCommand == IDE_COMMAND_WRITE_MULTIPLE_EXT) {
						if (AtaChannel[Channel].Drive[Dnum].Controller.NumberOfSectors > AtaChannel[Channel].Drive[Dnum].Controller.MultipleSectors)
							AtaChannel[Channel].Drive[Dnum].Controller.BufferSize = AtaChannel[Channel].Drive[Dnum].Controller.MultipleSectors * HARDDISK_SECTOR_SIZE;
						else
							AtaChannel[Channel].Drive[Dnum].Controller.BufferSize = AtaChannel[Channel].Drive[Dnum].Controller.NumberOfSectors * HARDDISK_SECTOR_SIZE;
					}
					AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex = 0;
					if (AtaChannel[Channel].Drive[Dnum].Controller.NumberOfSectors) {
						AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
						AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
						AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_REQ;
						AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_CORRECTED_DATA;
						AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_ERROR;
					}
					else {
						AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
						AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
						AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_REQ;
						AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_ERROR;
						AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_CORRECTED_DATA;
						AtaChannel[Channel].Drive[Dnum].CurrentLSector = AtaChannel[Channel].Drive[Dnum].NextLSector;
					}
					HardDiskRaiseInterrupt(Channel);
					break;
				case IDE_COMMAND_ATAPI_PACKET:
					if (AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex > PACKET_SIZE)
						return;
					/* Fetch the packet from portIO */
					switch (Length) {
						case 4:
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex + 3] = (BYTE)(Value >> 24);
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex + 2] = (BYTE)(Value >> 16);
						case 2:
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex + 1] = (BYTE)(Value >> 8);
						case 1:
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex] = (BYTE)(Value);
					}
					AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex += Length;
					if (AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex < PACKET_SIZE)
						return;
					/* Packet is ready, handle it */
					AtaChannel[Channel].Drive[Dnum].Controller.BufferSize = 2048;
					atapiCommand = AtaChannel[Channel].Drive[Dnum].Controller.Buffer[0];
					switch (atapiCommand) {
						case IDE_COMMAND_NOP:
							HardDiskCommandNop(Channel);
							HardDiskRaiseInterrupt(Channel);
							break;
						case IDE_COMMAND_START_STOP_UNIT:
							HardDiskCommandNop(Channel);
							HardDiskRaiseInterrupt(Channel);
							break;

						case SCSIOP_MECHANISM_STATUS:
							transferLength = Read16(AtaChannel[Channel].Drive[Dnum].Controller.Buffer + 8);
							if (!transferLength)
								return;
							HardDiskInitAtapiCommand(Channel, SCSIOP_MECHANISM_STATUS, 8, transferLength);
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[0] = 0; // reserved for non changers
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[1] = 0; // reserved for non changers
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[2] = 0; // Current LBA (TODO!)
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[3] = 0; // Current LBA (TODO!)
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[4] = 0; // Current LBA (TODO!)
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[5] = 1; // one slot
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[6] = 0; // slot table length
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[7] = 0; // slot table length
							HardDiskReadySendAtapi(Channel);
							break;

						case IDE_COMMAND_MODE_SENSE: // mode sense (6)
						case 0x5a: // mode sense (10)
							if (atapiCommand == 0x5a) 
								transferLength = Read16(AtaChannel[Channel].Drive[Dnum].Controller.Buffer + 7);
							
							else 
								transferLength = AtaChannel[Channel].Drive[Dnum].Controller.Buffer[4];
						
							temp = AtaChannel[Channel].Drive[Dnum].Controller.Buffer[2] >> 6;

							switch (temp) {
								case 0x0: // current values
									temp = AtaChannel[Channel].Drive[Dnum].Controller.Buffer[2] & 0x3f;
									switch (temp) {
										case 0x01: // error recovery
											HardDiskInitAtapiCommand(Channel, atapiCommand, 16, transferLength);
											HardDiskInitModeSense(Channel, NULL, 8);
											HardDiskReadySendAtapi(Channel);
											break;

										case 0x2a: // CD-ROM capabilities & mech. status
											HardDiskInitAtapiCommand(Channel, atapiCommand, 28, transferLength);
											HardDiskInitModeSense(Channel, &AtaChannel[Channel].Drive[Dnum].Controller.Buffer[8], 28);
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[8] = 0x2a;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[9] = 0x12;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[10] = 0x03;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[11] = 0x00;
											// Multisession, Mode 2 Form 2, Mode 2 Form 1, Audio
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[12] = 0x71;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[13] = (3 << 5);
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[14] = (1 | (AtaChannel[Channel].Drive[Dnum].Controller.Locked ? (1 << 1) : 0) | (1 << 3) | (1 << 5));
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[15] = 0x00;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[16] = ((16 * 176) >> 8) & 0xff;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[17] = (16 * 176) & 0xff;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[18] = 0;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[19] = 2;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[20] = (512 >> 8) & 0xff;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[21] = 512 & 0xff;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[22] = ((16 * 176) >> 8) & 0xff;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[23] = (16 * 176) & 0xff;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[24] = 0;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[25] = 0;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[26] = 0;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[27] = 0;
											HardDiskReadySendAtapi(Channel);
											break;

										case 0x0d: // CD-ROM
										case 0x0e: // CD-ROM audio control
										case 0x3f: // all
											HardDiskAtapiError(Channel, IDE_SENSE_ILLEGAL_REQUEST, IDE_ASC_INV_FIELD_IN_CMD_PACKET, 1);
											HardDiskRaiseInterrupt(Channel);
											break;

										default:											
											HardDiskAtapiError(Channel, IDE_SENSE_ILLEGAL_REQUEST, IDE_ASC_INV_FIELD_IN_CMD_PACKET, 1);
											HardDiskRaiseInterrupt(Channel);
											break;
									}
									break;
								case 0x1: // changeable values
									temp = AtaChannel[Channel].Drive[Dnum].Controller.Buffer[2] & 0x3f;
									switch (temp) {
										case 0x01: // error recovery
										case 0x0d: // CD-ROM
										case 0x0e: // CD-ROM audio control
										case 0x2a: // CD-ROM capabilities & mech. status
										case 0x3f: // all
											HardDiskAtapiError(Channel, IDE_SENSE_ILLEGAL_REQUEST, IDE_ASC_INV_FIELD_IN_CMD_PACKET, 1);
											HardDiskRaiseInterrupt(Channel);
											break;

										default:
											// not implemeted by this device
											HardDiskAtapiError(Channel, IDE_SENSE_ILLEGAL_REQUEST, IDE_ASC_INV_FIELD_IN_CMD_PACKET, 1);
											HardDiskRaiseInterrupt(Channel);
											break;
									}
									break;
								case 0x2: // default values
									temp = AtaChannel[Channel].Drive[Dnum].Controller.Buffer[2] & 0x3f;
									switch (temp) {
										case 0x2a: // CD-ROM capabilities & mech. status, copied from current values
											HardDiskInitAtapiCommand(Channel, atapiCommand, 28, transferLength);
											HardDiskInitModeSense(Channel, &AtaChannel[Channel].Drive[Dnum].Controller.Buffer[8], 28);
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[8] = 0x2a;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[9] = 0x12;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[10] = 0x03;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[11] = 0x00;
											// Multisession, Mode 2 Form 2, Mode 2 Form 1, Audio
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[12] = 0x71;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[13] = (3 << 5);
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[14] = (1 | (AtaChannel[Channel].Drive[Dnum].Controller.Locked ? (1 << 1) : 0) | (1 << 3) | (1 << 5));
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[15] = 0x00;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[16] = ((16 * 176) >> 8) & 0xff;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[17] = (16 * 176) & 0xff;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[18] = 0;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[19] = 2;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[20] = (512 >> 8) & 0xff;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[21] = 512 & 0xff;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[22] = ((16 * 176) >> 8) & 0xff;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[23] = (16 * 176) & 0xff;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[24] = 0;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[25] = 0;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[26] = 0;
											AtaChannel[Channel].Drive[Dnum].Controller.Buffer[27] = 0;
											HardDiskReadySendAtapi(Channel);
											break;

										case 0x01: // error recovery
										case 0x0d: // CD-ROM
										case 0x0e: // CD-ROM audio control
										case 0x3f: // all
											HardDiskAtapiError(Channel, IDE_SENSE_ILLEGAL_REQUEST, IDE_ASC_INV_FIELD_IN_CMD_PACKET, 1);
											HardDiskRaiseInterrupt(Channel);
											break;

										default:
											// not implemeted by this device
											HardDiskAtapiError(Channel, IDE_SENSE_ILLEGAL_REQUEST, IDE_ASC_INV_FIELD_IN_CMD_PACKET, 1);
											HardDiskRaiseInterrupt(Channel);
											break;
									}
									break;
								case 0x3: // saved values not implemented
								default:
									HardDiskAtapiError(Channel, IDE_SENSE_ILLEGAL_REQUEST, IDE_ASC_SAVING_PARAMETERS_NOT_SUPPORTED, 1);
									HardDiskRaiseInterrupt(Channel);
									break;
								
							}
							break;

						case 0x12: // inquiry
						
							transferLength = AtaChannel[Channel].Drive[Dnum].Controller.Buffer[4];
							HardDiskInitAtapiCommand(Channel, atapiCommand, 36, transferLength);

							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[0] = 0x05; // CD-ROM
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[1] = 0x80; // Removable
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[2] = 0x00; // ISO, ECMA, ANSI version
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[3] = 0x21; // ATAPI-2, as specified
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[4] = 31; // additional length (total 36)
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[5] = 0x00; // reserved
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[6] = 0x00; // reserved
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[7] = 0x00; // reserved

							// Vendor ID
							const char* vendor_id = "BOCHS   ";
							for (i = 0; i < 8; i++)
								AtaChannel[Channel].Drive[Dnum].Controller.Buffer[8 + i] = vendor_id[i];

							// Product ID
							const char* product_id = "Generic CD-ROM  ";
							for (i = 0; i < 16; i++)
								AtaChannel[Channel].Drive[Dnum].Controller.Buffer[16 + i] = product_id[i];
						

							// Product Revision level
							const char* rev_level = "1.0 ";
							for (i = 0; i < 4; i++)
								AtaChannel[Channel].Drive[Dnum].Controller.Buffer[32 + i] = rev_level[i];

							HardDiskReadySendAtapi(Channel);
							break;

						case IDE_COMMAND_REQUEST_SENSE:
							HardDiskInitAtapiCommand(Channel, IDE_COMMAND_REQUEST_SENSE, 18, AtaChannel[Channel].Drive[Dnum].Controller.Buffer[4]);
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[0] = 0x70 | (1 << 7);
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[1] = 0;
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[2] = AtaChannel[Channel].Drive[Dnum].Controller.Sense.sense_key;
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[3] = AtaChannel[Channel].Drive[Dnum].Controller.Sense.information.arr[0];
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[4] = AtaChannel[Channel].Drive[Dnum].Controller.Sense.information.arr[1];
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[5] = AtaChannel[Channel].Drive[Dnum].Controller.Sense.information.arr[2];
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[6] = AtaChannel[Channel].Drive[Dnum].Controller.Sense.information.arr[3];
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[7] = 17 - 7;
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[8] = AtaChannel[Channel].Drive[Dnum].Controller.Sense.specific_inf.arr[0];
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[9] = AtaChannel[Channel].Drive[Dnum].Controller.Sense.specific_inf.arr[1];
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[10] = AtaChannel[Channel].Drive[Dnum].Controller.Sense.specific_inf.arr[2];
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[11] = AtaChannel[Channel].Drive[Dnum].Controller.Sense.specific_inf.arr[3];
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[12] = AtaChannel[Channel].Drive[Dnum].Controller.Sense.asc;
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[13] = AtaChannel[Channel].Drive[Dnum].Controller.Sense.ascq;
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[14] = AtaChannel[Channel].Drive[Dnum].Controller.Sense.fruc;
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[15] = AtaChannel[Channel].Drive[Dnum].Controller.Sense.key_spec.arr[0];
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[16] = AtaChannel[Channel].Drive[Dnum].Controller.Sense.key_spec.arr[1];
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[17] = AtaChannel[Channel].Drive[Dnum].Controller.Sense.key_spec.arr[2];

							if (AtaChannel[Channel].Drive[Dnum].Controller.Sense.sense_key == IDE_SENSE_UNIT_ATTENTION) {
								AtaChannel[Channel].Drive[Dnum].Controller.Sense.sense_key = IDE_SENSE_NONE;
							}
							HardDiskReadySendAtapi(Channel);
							break;
						case SCSIOP_READ_CAPACITY:
							HardDiskInitAtapiCommand(Channel, SCSIOP_READ_CAPACITY, 8, 8);
							temp = CdRomGetCapacity() - 1;
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[0] = (temp >> 24) & 0xff;
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[1] = (temp >> 16) & 0xff;
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[2] = (temp >> 8) & 0xff;
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[3] = temp & 0xff;
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[4] = (2048 >> 24) & 0xff;
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[5] = (2048 >> 16) & 0xff;
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[6] = (2048 >> 8) & 0xff;
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[7] = 2048 & 0xff;
							HardDiskReadySendAtapi(Channel);
							break;
						case SCSIOP_READ:
							transferLength = Read16(AtaChannel[Channel].Drive[Dnum].Controller.Buffer + 7);
						case SCSIOP_READ12:
							if (!transferLength)
								transferLength = Read32(AtaChannel[Channel].Drive[Dnum].Controller.Buffer + 6);
							/* Get LB Address*/
							temp = Read32(AtaChannel[Channel].Drive[Dnum].Controller.Buffer + 2);

							if (transferLength * 2048 < transferLength
								|| transferLength * 2048 > CdRomGetCapacity() - 1
								|| temp > CdRomGetCapacity() - 1
								|| (DWORD)temp + transferLength < (DWORD)temp
								|| (DWORD)temp + transferLength - 1 < (DWORD)temp
								|| transferLength * 2048 + (DWORD)temp < (DWORD)temp) {
								HardDiskAtapiError(Channel, IDE_SENSE_ILLEGAL_REQUEST, IDE_ASC_LOGICAL_BLOCK_OOR, 1);
								HardDiskRaiseInterrupt(Channel);
							}
							if (temp + transferLength - 1 > CdRomGetCapacity() - 1)
								transferLength = (CdRomGetCapacity() - 1) - temp + 1;
							if (!transferLength || transferLength > CdRomGetCapacity() - 1) {
								HardDiskCommandNop(Channel);
								HardDiskRaiseInterrupt(Channel);
							}
							HardDiskInitAtapiCommand(Channel, atapiCommand, transferLength * 2048, transferLength * 2048);
							AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex = AtaChannel[Channel].Drive[Dnum].Controller.BufferSize;
							AtaChannel[Channel].Drive[Dnum].Controller.LbAddress = (DWORD)temp;
							AtaChannel[Channel].Drive[Dnum].Controller.RemainingBlocks = transferLength;
							break;
						case SCSIOP_READ_CD:
							temp = Read32(AtaChannel[Channel].Drive[Dnum].Controller.Buffer + 2);
							transferLength = AtaChannel[Channel].Drive[Dnum].Controller.Buffer[8] |
								(AtaChannel[Channel].Drive[Dnum].Controller.Buffer[7] << 8) |
								(AtaChannel[Channel].Drive[Dnum].Controller.Buffer[6] << 16);
							if (transferLength == 0) {
								HardDiskCommandNop(Channel);
								HardDiskRaiseInterrupt(Channel);
								break;
							}
							switch (AtaChannel[Channel].Drive[Dnum].Controller.Buffer[9] & 0xf8) {
								case 0x00:
									HardDiskCommandNop(Channel);
									HardDiskRaiseInterrupt(Channel);
									break;
								case 0xf8:
									AtaChannel[Channel].Drive[Dnum].Controller.BufferSize = 2352;
								case 0x10:
									HardDiskInitAtapiCommand(Channel, atapiCommand,
										transferLength * AtaChannel[Channel].Drive[Dnum].Controller.BufferSize,
										transferLength * AtaChannel[Channel].Drive[Dnum].Controller.BufferSize);
									AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex = AtaChannel[Channel].Drive[Dnum].Controller.BufferSize;
									AtaChannel[Channel].Drive[Dnum].Controller.RemainingBlocks = transferLength;
									AtaChannel[Channel].Drive[Dnum].Controller.LbAddress = (DWORD)temp;
									//start_seek(channel);
									break;
								default:
									HardDiskAtapiError(Channel, IDE_SENSE_ILLEGAL_REQUEST, IDE_ASC_INV_FIELD_IN_CMD_PACKET, 1);
									HardDiskRaiseInterrupt(Channel);
							}
							break;
						case SCSIOP_READ_TOC:
							transferLength = Read16(AtaChannel[Channel].Drive[Dnum].Controller.Buffer + 7);
							switch (AtaChannel[Channel].Drive[Dnum].Controller.Buffer[9] >> 6) {
								case 2:
								case 3:
								case 4:
									if (!((AtaChannel[Channel].Drive[Dnum].Controller.Buffer[1] >> 1) & 1))
										exit(-1);
								case 0:
								case 1:
								case 5:
									temp = CdRomReadToc(AtaChannel[Channel].Drive[Dnum].Controller.Buffer, transferLength, 
										(AtaChannel[Channel].Drive[Dnum].Controller.Buffer[1] >> 1) & 1, 
										AtaChannel[Channel].Drive[Dnum].Controller.Buffer[6],
										AtaChannel[Channel].Drive[Dnum].Controller.Buffer[9] >> 6);

									if (!temp) {
										HardDiskAtapiError(Channel, IDE_SENSE_ILLEGAL_REQUEST, IDE_ASC_INV_FIELD_IN_CMD_PACKET, 1);
										HardDiskRaiseInterrupt(Channel);
										break;
									}
									HardDiskInitAtapiCommand(Channel, atapiCommand, (DWORD)temp, transferLength);
									HardDiskReadySendAtapi(Channel);
									break;

								default:
									HardDiskAtapiError(Channel, IDE_SENSE_ILLEGAL_REQUEST, IDE_ASC_INV_FIELD_IN_CMD_PACKET, 1);
									HardDiskRaiseInterrupt(Channel);
							}
							break;
						case SCSIOP_SEEK:
							temp = Read32(AtaChannel[Channel].Drive[Dnum].Controller.Buffer + 2);

							if (temp > CdRomGetCapacity() - 1) {
								HardDiskAtapiError(Channel, IDE_SENSE_ILLEGAL_REQUEST, IDE_ASC_LOGICAL_BLOCK_OOR, 1);
								HardDiskRaiseInterrupt(Channel);
								break;
							}
							CdRomSeek((DWORD)temp);
							AtaChannel[Channel].Drive[Dnum].Controller.LbAddress = (DWORD)temp;
							HardDiskCommandNop(Channel);
							HardDiskRaiseInterrupt(Channel);
							break;
						case 0x1e: // prevent/allow medium removal
							AtaChannel[Channel].Drive[Dnum].Controller.Locked = AtaChannel[Channel].Drive[Dnum].Controller.Buffer[4] & 1;
							HardDiskCommandNop(Channel);
							HardDiskRaiseInterrupt(Channel);
							break;

						case SCSIOP_READ_SUB_CHANNEL: // read sub-channel
							temp = AtaChannel[Channel].Drive[Dnum].Controller.Buffer[3];
							transferLength = Read16(AtaChannel[Channel].Drive[Dnum].Controller.Buffer + 7);
							int ret_len = 4; // header size
							
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[0] = 0;
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[1] = 0; // audio not supported
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[2] = 0;
							AtaChannel[Channel].Drive[Dnum].Controller.Buffer[3] = 0;

							if ((AtaChannel[Channel].Drive[Dnum].Controller.Buffer[2] >> 6) & 1) { // !sub_q == header only
								if ((temp == 2) || (temp == 3)) { // UPC or ISRC
									ret_len = 24;
									AtaChannel[Channel].Drive[Dnum].Controller.Buffer[4] = (BYTE)temp;
									if (temp == 3) {
										AtaChannel[Channel].Drive[Dnum].Controller.Buffer[5] = 0x14;
										AtaChannel[Channel].Drive[Dnum].Controller.Buffer[6] = 1;
									}
									AtaChannel[Channel].Drive[Dnum].Controller.Buffer[8] = 0; // no UPC, no ISRC
								}
								else {
									HardDiskAtapiError(Channel, IDE_SENSE_ILLEGAL_REQUEST, IDE_ASC_INV_FIELD_IN_CMD_PACKET, 1);
									HardDiskRaiseInterrupt(Channel);
									break;
								}
							}
							HardDiskInitAtapiCommand(Channel, atapiCommand, ret_len, transferLength);
							HardDiskReadySendAtapi(Channel);
							break;

						case SCSIOP_READ_DISK_INFORMATION: // read disc info
						  // no-op to keep the Linux CD-ROM driver happy
							HardDiskAtapiError(Channel, IDE_SENSE_ILLEGAL_REQUEST, IDE_ASC_INV_FIELD_IN_CMD_PACKET, 1);
							HardDiskRaiseInterrupt(Channel);
							break;

						case SCSIOP_GET_EVENT_STATUS: // get event status notification						
							transferLength = Read16(AtaChannel[Channel].Drive[Dnum].Controller.Buffer + 7);
							if (AtaChannel[Channel].Drive[Dnum].Controller.Buffer[1] & 1) {
								// we currently only support the MEDIA event (bit 4)
								if (AtaChannel[Channel].Drive[Dnum].Controller.Buffer[4] == (1 << 4)) {
									AtaChannel[Channel].Drive[Dnum].Controller.Buffer[0] = 0;
									AtaChannel[Channel].Drive[Dnum].Controller.Buffer[1] = 4;  // MEDIA event is 4 bytes long
									AtaChannel[Channel].Drive[Dnum].Controller.Buffer[2] = (0 << 7) | 4;  // 4 = MEDIA event
									AtaChannel[Channel].Drive[Dnum].Controller.Buffer[3] = (1 << 4);  // we only support the MEDIA event (bit 4)
									AtaChannel[Channel].Drive[Dnum].Controller.Buffer[4] = 0;      // Event code: 4 = media changed, 3 = removed
									AtaChannel[Channel].Drive[Dnum].Controller.Buffer[5] = (1 << 1); // Media Status (bit 1 = Media Present)
									AtaChannel[Channel].Drive[Dnum].Controller.Buffer[6] = 0;
									AtaChannel[Channel].Drive[Dnum].Controller.Buffer[7] = 0;
									temp = (transferLength <= 4) ? 4 : 8;
								}
								else {
									AtaChannel[Channel].Drive[Dnum].Controller.Buffer[0] = 0;
									AtaChannel[Channel].Drive[Dnum].Controller.Buffer[1] = 0;
									AtaChannel[Channel].Drive[Dnum].Controller.Buffer[2] = (1 << 7) | AtaChannel[Channel].Drive[Dnum].Controller.Buffer[4];
									AtaChannel[Channel].Drive[Dnum].Controller.Buffer[3] = (1 << 4);  // we only support the MEDIA event (bit 4)
									temp = 4;
								}
								HardDiskInitAtapiCommand(Channel, atapiCommand, (DWORD)temp, (DWORD)temp);
								HardDiskReadySendAtapi(Channel);
							}
							else {
								HardDiskAtapiError(Channel, IDE_SENSE_ILLEGAL_REQUEST, IDE_ASC_INV_FIELD_IN_CMD_PACKET, 1);
								HardDiskRaiseInterrupt(Channel);
							}
							break;
						case 0x55: // mode select
						case 0xa6: // load/unload cd
						case 0x4b: // pause/resume
						case 0x45: // play audio
						case 0x47: // play audio msf
						case 0xbc: // play cd
						case 0xb9: // read cd msf
						case 0x44: // read header
						case 0xba: // scan
						case 0xbb: // set cd speed
						case 0x4e: // stop play/scan
						case 0x46: // get configuration
							HardDiskAtapiError(Channel, IDE_SENSE_ILLEGAL_REQUEST, IDE_ASC_ILLEGAL_OPCODE, 0);
							HardDiskRaiseInterrupt(Channel);
							break;
						default:
							HardDiskAtapiError(Channel, IDE_SENSE_ILLEGAL_REQUEST, IDE_ASC_ILLEGAL_OPCODE, 1);
							HardDiskRaiseInterrupt(Channel);
							break;
					}
					break;
				default:
					break;
			}
			break;
		case 1:	//IDE_WRITE_FEATURES
			AtaChannel[Channel].Drive[0].Controller.Features = Value;
			AtaChannel[Channel].Drive[1].Controller.Features = Value;
			break;
		case 2:
			AtaChannel[Channel].Drive[0].Controller.SectorCount = Value;
			AtaChannel[Channel].Drive[1].Controller.SectorCount = Value;
			break;
		case 3:
			AtaChannel[Channel].Drive[0].Controller.SectorNumber = Value;
			AtaChannel[Channel].Drive[1].Controller.SectorNumber = Value;
			break;
		case 4:
			AtaChannel[Channel].Drive[0].Controller.CylinderNumber = (AtaChannel[Channel].Drive[0].Controller.CylinderNumber & 0xff00) | (Value & 0xff);
			AtaChannel[Channel].Drive[1].Controller.CylinderNumber = (AtaChannel[Channel].Drive[1].Controller.CylinderNumber & 0xff00) | (Value & 0xff);
			break;
		case 5:
			AtaChannel[Channel].Drive[0].Controller.CylinderNumber = ((Value & 0xff) << 8) | (AtaChannel[Channel].Drive[0].Controller.CylinderNumber & 0xff);
			AtaChannel[Channel].Drive[1].Controller.CylinderNumber = ((Value & 0xff) << 8) | (AtaChannel[Channel].Drive[1].Controller.CylinderNumber & 0xff);
			break;
		case 6:
			AtaChannel[Channel].DriveSelect = Value & IDE_DRIVE_SELECT;
			AtaChannel[Channel].Drive[0].Controller.AddressMode = Value & IDE_LBA_MODE;
			AtaChannel[Channel].Drive[1].Controller.AddressMode = Value & IDE_LBA_MODE;
			AtaChannel[Channel].Drive[0].Controller.HeadNumber = Value & 0xf;
			AtaChannel[Channel].Drive[1].Controller.HeadNumber = Value & 0xf;
			break;
		case 7: //0x1f7
			if (Dnum)
				break;
			PicLowerIrq(AtaChannel[Channel].Irq);
			if (AtaChannel[Channel].Drive[Dnum].Controller.Status & IDE_STATUS_BUSY)
				return;
			if ((Value & 0xf0) == 0x10)
				Value = 0x10;
			AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_ERROR;
			switch (Value) {
				case 0x10:
					if (AtaChannel[Channel].Drive[Dnum].DriveType != DRIVE_TYPE_DISK) {
						HardDiskCommandAborted(Channel);
						break;
					}

					if (!AtaChannel[Channel].Drive[Dnum].DriveType) {
						AtaChannel[Channel].Drive[Dnum].Controller.ErrorRegister = 0x02; // Track 0 not found
						AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
						AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
						AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_SEEK_COMP;
						AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_REQ;
						AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_ERROR;
						HardDiskRaiseInterrupt(Channel);
						break;
					}

					/* move head to cylinder 0, issue IRQ */
					AtaChannel[Channel].Drive[Dnum].Controller.ErrorRegister = 0;
					AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber = 0;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_SEEK_COMP;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_REQ;
					HardDiskRaiseInterrupt(Channel);
					break;
				case IDE_COMMAND_READ_EXT:
				case IDE_COMMAND_READ_MULTIPLE_EXT:
					temp = 1;
				case IDE_COMMAND_READ:
				case 0x21:
				case IDE_COMMAND_READ_MULTIPLE:
					if (AtaChannel[Channel].Drive[Dnum].DriveType != DRIVE_TYPE_DISK) {
						HardDiskCommandAborted(Channel);
						break;
					}
					if (!AtaChannel[Channel].Drive[Dnum].Controller.AddressMode &&
						!AtaChannel[Channel].Drive[Dnum].Controller.HeadNumber &&
						!AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber &&
						!AtaChannel[Channel].Drive[Dnum].Controller.SectorNumber) {
						HardDiskCommandAborted(Channel);
						break;
					}
					HardDiskLba48Transform(Channel, temp);
					if (Value == 0xC4 || Value == 0x29) {
						if (!AtaChannel[Channel].Drive[Dnum].Controller.MultipleSectors) {
							HardDiskCommandAborted(Channel);
							break;
						}
						if (AtaChannel[Channel].Drive[Dnum].Controller.NumberOfSectors > AtaChannel[Channel].Drive[Dnum].Controller.MultipleSectors)
							AtaChannel[Channel].Drive[Dnum].Controller.BufferSize = AtaChannel[Channel].Drive[Dnum].Controller.MultipleSectors * 512;
						
						else 
							AtaChannel[Channel].Drive[Dnum].Controller.BufferSize = AtaChannel[Channel].Drive[Dnum].Controller.NumberOfSectors * 512;
					}
					else {
						AtaChannel[Channel].Drive[Dnum].Controller.BufferSize = 512;
					}
					temp = HardDiskCalculateLogicalAddress(Channel);
					if (temp == -1) {
						HardDiskCommandAborted(Channel);
						break;
					}
					AtaChannel[Channel].Drive[Dnum].NextLSector = temp;
					AtaChannel[Channel].Drive[Dnum].Controller.CurrentCommand = Value;
					AtaChannel[Channel].Drive[Dnum].Controller.ErrorRegister = 0;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_BUSY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_SEEK_COMP;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_REQ;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_CORRECTED_DATA;
					AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex = 0;

					temp = HardDiskReadSector(Channel, AtaChannel[Channel].Drive[Dnum].Controller.Buffer, AtaChannel[Channel].Drive[Dnum].Controller.BufferSize);
					if (!temp)
						HardDiskCommandAborted(Channel);
					break;
				case IDE_COMMAND_VERIFY_EXT:
					temp = 1;
				case IDE_COMMAND_VERIFY:
				case IDE_COMMAND_VERIFY_NO_RETRY:
					if (AtaChannel[Channel].Drive[Dnum].DriveType != DRIVE_TYPE_DISK) {
						HardDiskCommandAborted(Channel);
						break;
					}
					HardDiskLba48Transform(Channel, temp);
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_REQ;
					HardDiskRaiseInterrupt(Channel);
					break;

				case IDE_COMMAND_WRITE_EXT:
				case IDE_COMMAND_WRITE_MULTIPLE_EXT:
					temp = 1;
				case IDE_COMMAND_WRITE:
				case IDE_COMMAND_WRITE_MULTIPLE:
					if (AtaChannel[Channel].Drive[Dnum].DriveType != DRIVE_TYPE_DISK) {
						HardDiskCommandAborted(Channel);
						break;
					}
					HardDiskLba48Transform(Channel, temp);
					if (Value == 0xC5 || Value == 0x39) {
						if (!AtaChannel[Channel].Drive[Dnum].Controller.MultipleSectors) {
							HardDiskCommandAborted(Channel);
							break;
						}
						if (AtaChannel[Channel].Drive[Dnum].Controller.NumberOfSectors > AtaChannel[Channel].Drive[Dnum].Controller.MultipleSectors)
							AtaChannel[Channel].Drive[Dnum].Controller.BufferSize = AtaChannel[Channel].Drive[Dnum].Controller.MultipleSectors * 512;

						else
							AtaChannel[Channel].Drive[Dnum].Controller.BufferSize = AtaChannel[Channel].Drive[Dnum].Controller.NumberOfSectors * 512;

					}
					else {
						AtaChannel[Channel].Drive[Dnum].Controller.BufferSize = 512;
					}
					temp = HardDiskCalculateLogicalAddress(Channel);
					if (temp == -1) {
						HardDiskCommandAborted(Channel);
						break;
					}
					AtaChannel[Channel].Drive[Dnum].NextLSector = temp;
					AtaChannel[Channel].Drive[Dnum].Controller.CurrentCommand = Value;
					AtaChannel[Channel].Drive[Dnum].Controller.ErrorRegister = 0;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_SEEK_COMP;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_REQ;
					AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex = 0;

					temp = HardDiskWriteSector(Channel, AtaChannel[Channel].Drive[Dnum].Controller.Buffer, AtaChannel[Channel].Drive[Dnum].Controller.BufferSize);
					if (!temp)
						HardDiskCommandAborted(Channel);
					break;
				case IDE_COMMAND_EXECUTE_DEVICE_DIAGNOSTIC:
					HardDiskSetSignature(Channel, Dnum);
					AtaChannel[Channel].Drive[Dnum].Controller.ErrorRegister = 0x01;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_REQ;
					HardDiskRaiseInterrupt(Channel);
					break;
				case IDE_COMMAND_SET_DRIVE_PARAMETERS:
					if (AtaChannel[Channel].Drive[Dnum].DriveType != DRIVE_TYPE_DISK) {
						HardDiskCommandAborted(Channel);
						break;
					}

					if (AtaChannel[Channel].Drive[Dnum].Controller.SectorCount != HARDDISK_SECTORS) {
						HardDiskCommandAborted(Channel);
						break;
					}
					if (!AtaChannel[Channel].Drive[Dnum].Controller.HeadNumber) {
						fprintf(stderr, "IDE_COMMAND_SET_DRIVE_PARAMETERS AtaChannel[Channel].Drive[Dnum].Controller.HeadNumber == 0");
						exit(-1);
					}
					else if (AtaChannel[Channel].Drive[Dnum].Controller.HeadNumber != HARDDISK_HEADS - 1) {
						//fprintf(stderr, "IDE_COMMAND_SET_DRIVE_PARAMETERS AtaChannel[Channel].Drive[Dnum].Controller.HeadNumber != HARDDISK_HEADS");
						HardDiskCommandAborted(Channel);
						break;
					}

					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_REQ;
					HardDiskRaiseInterrupt(Channel);
					break;
				case IDE_COMMAND_IDENTIFY:
					if (!AtaChannel[Channel].Drive[Dnum].DriveType) {
						HardDiskCommandAborted(Channel);
						break;
					}
					else if (AtaChannel[Channel].Drive[Dnum].DriveType == DRIVE_TYPE_CDROM) {
						HardDiskCommandAborted(Channel);
						HardDiskSetSignature(Channel, Dnum);
						break;
					}
					AtaChannel[Channel].Drive[Dnum].Controller.CurrentCommand = Value;
					AtaChannel[Channel].Drive[Dnum].Controller.ErrorRegister = 0;

					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_CORRECTED_DATA;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_REQ;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_SEEK_COMP;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_WRITE_FAULT;

					if (!AtaChannel[Channel].Drive[Dnum].IdentifySet) {
						memset(AtaChannel[Channel].Drive[Dnum].DriveId, 0, sizeof(AtaChannel[Channel].Drive[Dnum].DriveId));
						AtaChannel[Channel].Drive[Dnum].DriveId[0] = 0x0040;
						if (AtaChannel[Channel].Drive[Dnum].Cylinders > 16383)
							AtaChannel[Channel].Drive[Dnum].DriveId[1] = 16383;

						else
							AtaChannel[Channel].Drive[Dnum].DriveId[1] = AtaChannel[Channel].Drive[Dnum].Cylinders;

						AtaChannel[Channel].Drive[Dnum].DriveId[3] = AtaChannel[Channel].Drive[Dnum].Heads;
						AtaChannel[Channel].Drive[Dnum].DriveId[4] = HARDDISK_SECTOR_SIZE * AtaChannel[Channel].Drive[Dnum].Spt;
						AtaChannel[Channel].Drive[Dnum].DriveId[5] = HARDDISK_SECTOR_SIZE;
						AtaChannel[Channel].Drive[Dnum].DriveId[6] = AtaChannel[Channel].Drive[Dnum].Spt;

						memset(serial_number, '\0', sizeof(serial_number));
						// Word 7-9: Vendor specific

						// Word 10-19: Serial number (20 ASCII characters, 0000h=not specified)
						// This field is right justified and padded with spaces (20h).
						memcpy(serial_number, "BXHD00000           ", 21);
						serial_number[7] = Channel + 49;
						serial_number[8] = AtaChannel[Channel].DriveSelect + 49;
						for (i = 0; i < 10; i++) {
							AtaChannel[Channel].Drive[Dnum].DriveId[10 + i] = (serial_number[i * 2] << 8) | serial_number[i * 2 + 1];
						}

						// Word 20: buffer type
						//          0000h = not specified
						//          0001h = single ported single sector buffer which is
						//                  not capable of simulataneous data xfers to/from
						//                  the host and the disk.
						//          0002h = dual ported multi-sector buffer capable of
						//                  simulatenous data xfers to/from the host and disk.
						//          0003h = dual ported mutli-sector buffer capable of
						//                  simulatenous data xfers with a read caching
						//                  capability.
						//          0004h-ffffh = reserved
						AtaChannel[Channel].Drive[Dnum].DriveId[20] = 3;

						// Word 21: buffer size in 512 byte increments, 0000h = not specified
						AtaChannel[Channel].Drive[Dnum].DriveId[21] = 512; // 512 Sectors = 256kB cache

						// Word 22: # of ECC bytes available on read/write long cmds
						//          0000h = not specified
						AtaChannel[Channel].Drive[Dnum].DriveId[22] = 4;

						// Word 23..26: Firmware revision (8 ascii chars, 0000h=not specified)
						// This field is left justified and padded with spaces (20h)
						for (i = 23; i <= 26; i++)
							AtaChannel[Channel].Drive[Dnum].DriveId[i] = 0;

						// Word 27..46: Model number (40 ascii chars, 0000h=not specified)
						// This field is left justified and padded with spaces (20h)
						for (i = 0; i < 20; i++) {
							AtaChannel[Channel].Drive[Dnum].DriveId[27 + i] = (AtaChannel[Channel].Drive[Dnum].Model[i * 2] << 8) | AtaChannel[Channel].Drive[Dnum].Model[i * 2 + 1];
						}

						// Word 47: 15-8 Vendor unique
						//           7-0 00h= read/write multiple commands not implemented
						//               xxh= maximum # of sectors that can be transferred
						//                    per interrupt on read and write multiple commands
						AtaChannel[Channel].Drive[Dnum].DriveId[47] = MAX_MULTIPLE_SECTORS;

						// Word 48: 0000h = cannot perform dword IO
						//          0001h = can    perform dword IO
						AtaChannel[Channel].Drive[Dnum].DriveId[48] = 1;

						// Word 49: Capabilities
						//   15-10: 0 = reserved
						//       9: 1 = LBA supported
						//       8: 1 = DMA supported
						//     7-0: Vendor unique
						AtaChannel[Channel].Drive[Dnum].DriveId[49] = (1 << 9) | (1 << 8);

						// Word 50: Reserved

						// Word 51: 15-8 PIO data transfer cycle timing mode
						//           7-0 Vendor unique
						AtaChannel[Channel].Drive[Dnum].DriveId[51] = 0x200;

						// Word 52: 15-8 DMA data transfer cycle timing mode
						//           7-0 Vendor unique
						AtaChannel[Channel].Drive[Dnum].DriveId[52] = 0x200;

						// Word 53: 15-1 Reserved
						//             2 1=the fields reported in word 88 are valid
						//             1 1=the fields reported in words 64-70 are valid
						//             0 1=the fields reported in words 54-58 are valid
						AtaChannel[Channel].Drive[Dnum].DriveId[53] = 0x07;

						// Word 54: # of user-addressable cylinders in curr xlate mode
						// Word 55: # of user-addressable heads in curr xlate mode
						// Word 56: # of user-addressable sectors/track in curr xlate mode
						if (AtaChannel[Channel].Drive[Dnum].Cylinders > 16383) {
							AtaChannel[Channel].Drive[Dnum].DriveId[54] = 16383;
						}
						else {
							AtaChannel[Channel].Drive[Dnum].DriveId[54] = AtaChannel[Channel].Drive[Dnum].Cylinders;
						}
						AtaChannel[Channel].Drive[Dnum].DriveId[55] = AtaChannel[Channel].Drive[Dnum].Heads;
						AtaChannel[Channel].Drive[Dnum].DriveId[56] = AtaChannel[Channel].Drive[Dnum].Spt;

						// Word 57-58: Current capacity in sectors
						// Excludes all sectors used for device specific purposes.

						temp = AtaChannel[Channel].Drive[Dnum].Cylinders * AtaChannel[Channel].Drive[Dnum].Heads * AtaChannel[Channel].Drive[Dnum].Spt;
						AtaChannel[Channel].Drive[Dnum].DriveId[57] = (temp & 0xffff); // LSW
						AtaChannel[Channel].Drive[Dnum].DriveId[58] = (temp >> 16);    // MSW

						// Word 59: 15-9 Reserved
						//             8 1=multiple sector setting is valid
						//           7-0 current setting for number of sectors that can be
						//               transferred per interrupt on R/W multiple commands
						if (HARDDISK_SECTOR_SIZE > 0)
							AtaChannel[Channel].Drive[Dnum].DriveId[59] = 0x0100 | MAX_MULTIPLE_SECTORS;
						else
							AtaChannel[Channel].Drive[Dnum].DriveId[59] = 0x0000;

						// Word 60-61:
						// If drive supports LBA Mode, these words reflect total # of user
						// addressable sectors.  This value does not depend on the current
						// drive geometry.  If the drive does not support LBA mode, these
						// words shall be set to 0.
						if (VhdGetHardDiskSize() > 0)
							temp = (VhdGetHardDiskSize() / HARDDISK_SECTOR_SIZE);
						else
							temp = AtaChannel[Channel].Drive[Dnum].Cylinders * AtaChannel[Channel].Drive[Dnum].Heads * AtaChannel[Channel].Drive[Dnum].Spt;
						AtaChannel[Channel].Drive[Dnum].DriveId[60] = (temp & 0xffff); // LSW
						AtaChannel[Channel].Drive[Dnum].DriveId[61] = (temp >> 16) & 0xffff; // MSW

						// Word 62: 15-8 single word DMA transfer mode active
						//           7-0 single word DMA transfer modes supported
						// The low order byte identifies by bit, all the Modes which are
						// supported e.g., if Mode 0 is supported bit 0 is set.
						// The high order byte contains a single bit set to indiciate
						// which mode is active.
						AtaChannel[Channel].Drive[Dnum].DriveId[62] = 0x0;

						// Word 63: 15-8 multiword DMA transfer mode active
						//           7-0 multiword DMA transfer modes supported
						// The low order byte identifies by bit, all the Modes which are
						// supported e.g., if Mode 0 is supported bit 0 is set.
						// The high order byte contains a single bit set to indiciate
						// which mode is active.

						AtaChannel[Channel].Drive[Dnum].DriveId[63] = 0x07 | (AtaChannel[Channel].Drive[Dnum].Controller.MdmaMode << 8);

						// Word 64 PIO modes supported
						AtaChannel[Channel].Drive[Dnum].DriveId[64] = 0x00;

						// Word 65-68 PIO/DMA cycle time (nanoseconds)
						for (i = 65; i <= 68; i++)
							AtaChannel[Channel].Drive[Dnum].DriveId[i] = 120;

						// Word 69-79 Reserved

						// Word 80: 15-9 reserved
						//             8 supports ATA/ATAPI-8
						//             7 supports ATA/ATAPI-7
						//             6 supports ATA/ATAPI-6
						//             5 supports ATA/ATAPI-5
						//             4 supports ATA/ATAPI-4
						//             3 supports ATA-3
						//             2 supports ATA-2
						//             1 supports ATA-1
						//             0 reserved
						AtaChannel[Channel].Drive[Dnum].DriveId[80] = 0x7E;

						// Word 81: Minor version number
						AtaChannel[Channel].Drive[Dnum].DriveId[81] = 0x00;

						// Word 82: 15 obsolete
						//          14 NOP command supported
						//          13 READ BUFFER command supported
						//          12 WRITE BUFFER command supported
						//          11 obsolete
						//          10 Host protected area feature set supported
						//           9 DEVICE RESET command supported
						//           8 SERVICE interrupt supported
						//           7 release interrupt supported
						//           6 look-ahead supported
						//           5 write cache supported
						//           4 supports PACKET command feature set
						//           3 supports power management feature set
						//           2 supports removable media feature set
						//           1 supports securite mode feature set
						//           0 support SMART feature set
						AtaChannel[Channel].Drive[Dnum].DriveId[82] = 1 << 14;

						// Word 83: 15 shall be ZERO
						//          14 shall be ONE
						//          13 FLUSH CACHE EXT command supported
						//          12 FLUSH CACHE command supported
						//          11 Device configuration overlay supported
						//          10 48-bit Address feature set supported
						//           9 Automatic acoustic management supported
						//           8 SET MAX security supported
						//           7 reserved for 1407DT PARTIES
						//           6 SetF sub-command Power-Up supported
						//           5 Power-Up in standby feature set supported
						//           4 Removable media notification supported
						//           3 APM feature set supported
						//           2 CFA feature set supported
						//           1 READ/WRITE DMA QUEUED commands supported
						//           0 Download MicroCode supported
						AtaChannel[Channel].Drive[Dnum].DriveId[83] = (1 << 14) | (1 << 13) | (1 << 12) | (1 << 10);
						AtaChannel[Channel].Drive[Dnum].DriveId[84] = 1 << 14;
						AtaChannel[Channel].Drive[Dnum].DriveId[85] = 1 << 14;

						// Word 86: 15 shall be ZERO
						//          14 shall be ONE
						//          13 FLUSH CACHE EXT command enabled
						//          12 FLUSH CACHE command enabled
						//          11 Device configuration overlay enabled
						//          10 48-bit Address feature set enabled
						//           9 Automatic acoustic management enabled
						//           8 SET MAX security enabled
						//           7 reserved for 1407DT PARTIES
						//           6 SetF sub-command Power-Up enabled
						//           5 Power-Up in standby feature set enabled
						//           4 Removable media notification enabled
						//           3 APM feature set enabled
						//           2 CFA feature set enabled
						//           1 READ/WRITE DMA QUEUED commands enabled
						//           0 Download MicroCode enabled
						AtaChannel[Channel].Drive[Dnum].DriveId[86] = (1 << 14) | (1 << 13) | (1 << 12) | (1 << 10);
						AtaChannel[Channel].Drive[Dnum].DriveId[87] = 1 << 14;
						AtaChannel[Channel].Drive[Dnum].DriveId[88] = 0x3f | (AtaChannel[Channel].Drive[Dnum].Controller.UdmaMode << 8);
						AtaChannel[Channel].Drive[Dnum].DriveId[93] = 1 | (1 << 14) | 0x2000;

						// Word 100-103: 48-bit total number of sectors
						AtaChannel[Channel].Drive[Dnum].DriveId[100] = (temp & 0xffff);
						AtaChannel[Channel].Drive[Dnum].DriveId[101] = (temp >> 16);
						AtaChannel[Channel].Drive[Dnum].DriveId[102] = (temp >> 32);
						AtaChannel[Channel].Drive[Dnum].DriveId[103] = (temp >> 48);

						// Word 106: Physical/Logical Sector Size (ATAPI 7+) (Optional)
						//           15 shall be ZERO
						//           14 shall be ONE
						//           13 1 = Device has multiple logical sectors per physical sector
						//           12 1 = Device Logical sector greater than 256 words
						//       11 - 4  reserved
						//        3 - 0  x where 2^x = logical sectors per physical sector
						// Words 117-118: Words per Logical Sector
						//
						// We do not emulate 512-byte logical sectors on 1k and 4k drives.  Why would we?
						// Therefore, we tell the guest that we are physical sectors with one logical sector per physical sector.
						AtaChannel[Channel].Drive[Dnum].DriveId[106] = 0;
						AtaChannel[Channel].Drive[Dnum].DriveId[117] = 0;
						AtaChannel[Channel].Drive[Dnum].DriveId[118] = 0;

						// Word 128-159 Vendor unique
						// Word 160-255 Reserved
						AtaChannel[Channel].Drive[Dnum].IdentifySet = 1;
					}
					for (i = 0; i < 256; i++) {
						temp = AtaChannel[Channel].Drive[Dnum].DriveId[i];
						AtaChannel[Channel].Drive[Dnum].Controller.Buffer[i * 2] = temp & 0xff;
						AtaChannel[Channel].Drive[Dnum].Controller.Buffer[i * 2 + 1] = temp >> 8;
					}
					HardDiskRaiseInterrupt(Channel);
					
					break;
				case IDE_COMMAND_SET_FEATURE:
					switch (AtaChannel[Channel].Drive[Dnum].Controller.Features) {
						case 0x03: // Set Transfer Mode

							switch (AtaChannel[Channel].Drive[Dnum].Controller.SectorCount >> 3) {
								case 0x00: // PIO default
								case 0x01: // PIO mode
									AtaChannel[Channel].Drive[Dnum].Controller.MdmaMode = 0x00;
									AtaChannel[Channel].Drive[Dnum].Controller.UdmaMode = 0x00;
									AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
									AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_SEEK_COMP;
									HardDiskRaiseInterrupt(Channel);
									break;
								case 0x04: // MDMA mode
									
									AtaChannel[Channel].Drive[Dnum].Controller.MdmaMode = (1 << (AtaChannel[Channel].Drive[Dnum].Controller.SectorCount & 7));
									AtaChannel[Channel].Drive[Dnum].Controller.UdmaMode = 0x00;
									AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
									AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_SEEK_COMP;
									HardDiskRaiseInterrupt(Channel);
									break;
								case 0x08: // UDMA mode
									AtaChannel[Channel].Drive[Dnum].Controller.MdmaMode = 0x00;
									AtaChannel[Channel].Drive[Dnum].Controller.UdmaMode = (1 << (AtaChannel[Channel].Drive[Dnum].Controller.SectorCount & 7));
									AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
									AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_SEEK_COMP;
									HardDiskRaiseInterrupt(Channel);
									break;
								default:
									
									HardDiskCommandAborted(Channel);
								}
							AtaChannel[Channel].Drive[Dnum].IdentifySet = 0;
							break;
						case 0x02: // Enable and
						case 0x82: //  Disable write cache.
						case 0xAA: // Enable and
						case 0x55: //  Disable look-ahead cache.
						case 0xCC: // Enable and
						case 0x66: //  Disable reverting to power-on default
							AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
							AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_SEEK_COMP;
							HardDiskRaiseInterrupt(Channel);
							break;
					}
					break;
				case IDE_COMMAND_SET_MULTIPLE:
					if (AtaChannel[Channel].Drive[Dnum].DriveType != DRIVE_TYPE_DISK) {
						HardDiskCommandAborted(Channel);
						break;
					}
					if (AtaChannel[Channel].Drive[Dnum].Controller.SectorCount > HARDDISK_SECTORS
						|| !AtaChannel[Channel].Drive[Dnum].Controller.SectorCount) {
						HardDiskCommandAborted(Channel);
						break;
					}
					AtaChannel[Channel].Drive[Dnum].Controller.MultipleSectors = AtaChannel[Channel].Drive[Dnum].Controller.SectorCount;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_WRITE_FAULT;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_REQ;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
					HardDiskRaiseInterrupt(Channel);
					break;

				//set state to handle atapi commands
				case IDE_COMMAND_ATAPI_PACKET:
					if (AtaChannel[Channel].Drive[Dnum].DriveType != DRIVE_TYPE_CDROM) {
						HardDiskCommandAborted(Channel);
						return;
					}
					AtaChannel[Channel].Drive[Dnum].Controller.PacketDma = AtaChannel[Channel].Drive[Dnum].Controller.Features & IDE_SATA_FEATURE_NON_ZERO_DMA_BUFFER_OFFSET;
					if (AtaChannel[Channel].Drive[Dnum].Controller.PacketDma & IDE_SATA_FEATURE_DMA_SETUP_FIS_AUTO_ACTIVATE) {
						HardDiskCommandAborted(Channel);
						return;
					}
					AtaChannel[Channel].Drive[Dnum].Controller.SectorCount = 1;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_WRITE_FAULT;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_REQ;
					AtaChannel[Channel].Drive[Dnum].Controller.CurrentCommand = Value;
					AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex = 0;
					break;
				case IDE_COMMAND_ATAPI_IDENTIFY:
					if (AtaChannel[Channel].Drive[Dnum].DriveType != DRIVE_TYPE_CDROM) {
						HardDiskCommandAborted(Channel);
						return;
					}
					AtaChannel[Channel].Drive[Dnum].Controller.CurrentCommand = Value;
					AtaChannel[Channel].Drive[Dnum].Controller.ErrorRegister = 0;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_CORRECTED_DATA;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_REQ;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_SEEK_COMP;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_WRITE_FAULT;

					AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex = 0;
					if (!AtaChannel[Channel].Drive[Dnum].IdentifySet) {
						/* write identity */
						memset(AtaChannel[Channel].Drive[Dnum].DriveId, 0, sizeof(AtaChannel[Channel].Drive[Dnum].DriveId));
						AtaChannel[Channel].Drive[Dnum].DriveId[0] = (2 << 14) | (5 << 8) | (1 << 7) | (2 << 5) | (0 << 0); // Removable CDROM, 50us response, 12 byte packets
						
						for (i = 1; i <= 9; i++)
							AtaChannel[Channel].Drive[Dnum].DriveId[i] = 0;

						memcpy(serial_number, "BXHD00000           ", 21);
						serial_number[8] = AtaChannel[Channel].DriveSelect;
						for (i = 0; i < 10; i++) {
							AtaChannel[Channel].Drive[Dnum].DriveId[10 + i] = (serial_number[i * 2] << 8) | serial_number[i * 2 + 1];
						}
						for (i = 20; i <= 22; i++)
							AtaChannel[Channel].Drive[Dnum].DriveId[i] = 0;

						const char* firmware = "ALPHA1  ";
						for (i = 0; i < strlen(firmware) / 2; i++) {
							AtaChannel[Channel].Drive[Dnum].DriveId[23 + i] = (firmware[i * 2] << 8) | firmware[i * 2 + 1];
						}

						for (i = 0; i < strlen((char*)AtaChannel[Channel].Drive[Dnum].Model) / 2; i++) {
							AtaChannel[Channel].Drive[Dnum].DriveId[27 + i] = (AtaChannel[Channel].Drive[Dnum].Model[i * 2] << 8) | AtaChannel[Channel].Drive[Dnum].Model[i * 2 + 1];
						}

						AtaChannel[Channel].Drive[Dnum].DriveId[47] = 0;
						AtaChannel[Channel].Drive[Dnum].DriveId[48] = 1; // 32 bits access
						AtaChannel[Channel].Drive[Dnum].DriveId[49] = (1 << 9) | (1 << 8); // LBA and DMA
						
						AtaChannel[Channel].Drive[Dnum].DriveId[50] = 0;
						AtaChannel[Channel].Drive[Dnum].DriveId[51] = 0;
						AtaChannel[Channel].Drive[Dnum].DriveId[52] = 0;
						AtaChannel[Channel].Drive[Dnum].DriveId[53] = 3; // words 64-70, 54-58 valid

						for (i = 54; i <= 62; i++)
							AtaChannel[Channel].Drive[Dnum].DriveId[i] = 0;

						AtaChannel[Channel].Drive[Dnum].DriveId[63] = 0x07 | (AtaChannel[Channel].Drive[Dnum].Controller.MdmaMode << 8);
						
						AtaChannel[Channel].Drive[Dnum].DriveId[64] = 0x0001; // PIO
						AtaChannel[Channel].Drive[Dnum].DriveId[65] = 0x00b4;
						AtaChannel[Channel].Drive[Dnum].DriveId[66] = 0x00b4;
						AtaChannel[Channel].Drive[Dnum].DriveId[67] = 0x012c;
						AtaChannel[Channel].Drive[Dnum].DriveId[68] = 0x00b4;

						AtaChannel[Channel].Drive[Dnum].DriveId[69] = 0;
						AtaChannel[Channel].Drive[Dnum].DriveId[70] = 0;
						AtaChannel[Channel].Drive[Dnum].DriveId[71] = 30; // faked
						AtaChannel[Channel].Drive[Dnum].DriveId[72] = 30; // faked
						AtaChannel[Channel].Drive[Dnum].DriveId[73] = 0;
						AtaChannel[Channel].Drive[Dnum].DriveId[74] = 0;
						AtaChannel[Channel].Drive[Dnum].DriveId[75] = 0;

						for (i = 76; i <= 79; i++)
							AtaChannel[Channel].Drive[Dnum].DriveId[i] = 0;

						AtaChannel[Channel].Drive[Dnum].DriveId[80] = 0x1e; // supports up to ATA/ATAPI-4
						AtaChannel[Channel].Drive[Dnum].DriveId[81] = 0;
						AtaChannel[Channel].Drive[Dnum].DriveId[82] = 0;
						AtaChannel[Channel].Drive[Dnum].DriveId[83] = 0;
						AtaChannel[Channel].Drive[Dnum].DriveId[84] = 0;
						AtaChannel[Channel].Drive[Dnum].DriveId[85] = 0;
						AtaChannel[Channel].Drive[Dnum].DriveId[86] = 0;
						AtaChannel[Channel].Drive[Dnum].DriveId[87] = 0;
						AtaChannel[Channel].Drive[Dnum].DriveId[88] = 0;

						AtaChannel[Channel].Drive[Dnum].IdentifySet = 1;
					}
					for (i = 0; i < 256; i++) {
						temp = AtaChannel[Channel].Drive[Dnum].DriveId[i];
						AtaChannel[Channel].Drive[Dnum].Controller.Buffer[i * 2] = temp & 0xff;
						AtaChannel[Channel].Drive[Dnum].Controller.Buffer[i * 2 + 1] = temp >> 8;
					}
					HardDiskRaiseInterrupt(Channel);
					break;
				case IDE_COMMAND_ATAPI_RESET:
					if (AtaChannel[Channel].Drive[Dnum].DriveType != DRIVE_TYPE_CDROM) {
						HardDiskCommandAborted(Channel);
						return;
					}
					HardDiskSetSignature(Channel, Dnum);
					AtaChannel[Channel].Drive[Dnum].Controller.ErrorRegister &= ~(1 << 7);
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_WRITE_FAULT;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_CORRECTED_DATA;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_REQ;
					break;
				case IDE_COMMAND_ATAPI_SERVICE:
					HardDiskCommandAborted(Channel);
					break;

				//power mgmt & flush
				case IDE_COMMAND_CHECK_POWER:
					AtaChannel[Channel].Drive[Dnum].Controller.SectorCount = 0xff;
				case IDE_COMMAND_STANDBY_IMMEDIATE:
				case IDE_COMMAND_IDLE_IMMEDIATE:
				case IDE_COMMAND_FLUSH_CACHE:
				case IDE_COMMAND_FLUSH_CACHE_EXT:
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_WRITE_FAULT;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_REQ;
					HardDiskRaiseInterrupt(Channel);
					break;
				case IDE_COMMAND_SEEK:
					if (AtaChannel[Channel].Drive[Dnum].DriveType != DRIVE_TYPE_DISK) {
						HardDiskCommandAborted(Channel);
						break;
					}
					temp = HardDiskCalculateLogicalAddress(Channel);
					if (temp == -1) {
						HardDiskCommandAborted(Channel);
						break;
					}
					AtaChannel[Channel].Drive[Dnum].NextLSector = temp;
					AtaChannel[Channel].Drive[Dnum].Controller.CurrentCommand = Value;
					AtaChannel[Channel].Drive[Dnum].Controller.ErrorRegister = 0;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_BUSY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_CORRECTED_DATA;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_REQ;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_SEEK_COMP;
					
					break;
				//DMA
				case IDE_COMMAND_READ_DMA_EXT:
					temp = 1;
				case IDE_COMMAND_READ_DMA:
					if (AtaChannel[Channel].Drive[Dnum].DriveType != DRIVE_TYPE_DISK || !IdeBdmaPresent()) {
						HardDiskCommandAborted(Channel);
						break;
					}
					HardDiskLba48Transform(Channel, temp);
					temp = HardDiskCalculateLogicalAddress(Channel);
					if (temp == -1) {
						HardDiskCommandAborted(Channel);
						break;
					}
					AtaChannel[Channel].Drive[Dnum].NextLSector = temp;
					AtaChannel[Channel].Drive[Dnum].Controller.CurrentCommand = Value;
					AtaChannel[Channel].Drive[Dnum].Controller.ErrorRegister = 0;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_BUSY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_CORRECTED_DATA;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_REQ;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_SEEK_COMP;
					break;
				case IDE_COMMAND_WRITE_DMA_EXT:
					temp = 1;
				case IDE_COMMAND_WRITE_DMA:
					if (AtaChannel[Channel].Drive[Dnum].DriveType != DRIVE_TYPE_DISK || !IdeBdmaPresent()) {
						HardDiskCommandAborted(Channel);
						break;
					}
					HardDiskLba48Transform(Channel, temp);
					temp = HardDiskCalculateLogicalAddress(Channel);
					if (temp == -1) {
						HardDiskCommandAborted(Channel);
						break;
					}
					AtaChannel[Channel].Drive[Dnum].NextLSector = temp;
					AtaChannel[Channel].Drive[Dnum].Controller.CurrentCommand = Value;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_SEEK_COMP;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_REQ;
					break;
				case IDE_COMMAND_READ_NATIVE_ADDRESS_EXT:
					temp = 1;
				case IDE_COMMAND_READ_NATIVE_ADDRESS:
					if (AtaChannel[Channel].Drive[Dnum].DriveType != DRIVE_TYPE_DISK || !AtaChannel[Channel].Drive[Dnum].Controller.AddressMode) {
						HardDiskCommandAborted(Channel);
						break;
					}
					HardDiskLba48Transform(Channel, temp);
					temp = VhdGetHardDiskSize() / 512 - 1;
					if (AtaChannel[Channel].Drive[Dnum].Controller.Lba48) {
						AtaChannel[Channel].Drive[Dnum].Controller.Hob.HighCylinder = (temp >> 40) & 0xff;
						AtaChannel[Channel].Drive[Dnum].Controller.Hob.LowCylinder = (temp >> 32) & 0xff;
						AtaChannel[Channel].Drive[Dnum].Controller.Hob.Sector = (temp >> 24) & 0xff;
						AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber = (temp >> 8) & 0xffff;
						AtaChannel[Channel].Drive[Dnum].Controller.SectorNumber = temp & 0xff;
					}
					else {
						AtaChannel[Channel].Drive[Dnum].Controller.HeadNumber = (temp >> 24) & 0xf;
						AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber = (temp >> 8) & 0xffff;
						AtaChannel[Channel].Drive[Dnum].Controller.SectorNumber = temp & 0xff;
					}
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_SEEK_COMP;
					HardDiskRaiseInterrupt(Channel);
					break;
				case 0x22:
				case 0x23:
				case 0x26:
				case 0x2A:
				case 0x2B:
				case 0x2F:
				case 0x31:
				case 0x32:
				case 0x33:
				case 0x36:
				case 0x37:
				case 0x38:
				case 0x3A:
				case 0x3B:
				case 0x3F:
				case 0x50:
				case 0x51:
				case 0x87:
				case 0x92:
				case 0x94:
				case 0x95:
				case 0x96:
				case 0x97:
				case 0x98:
				case 0x99:
				case 0xB0:
				case 0xB1:
				case 0xC0:
				case 0xC7:
				case 0xC9:
				case 0xCC:
				case 0xCD:
				case 0xD1:
				case 0xDA:
				case 0xDE:
				case 0xDF:
				case 0xE2:
				case 0xE3:
				case 0xE4:
				case 0xE6:
				case 0xE8:
				case 0xED:
				case 0xF1:
				case 0xF2:
				case 0xF3:
				case 0xF4:
				case 0xF5:
				case 0xF6:
				case 0xF9:
				default:
					//command_aborted(Channel, Value & 0xff);
					HardDiskCommandAborted(Channel);
					break;
			}
			break;
		case 0x16:
			
			PrevReset = AtaChannel[Channel].Drive[Dnum].Controller.Reset;
			AtaChannel[Channel].Drive[0].Controller.Reset = Value & 0x04;
			AtaChannel[Channel].Drive[1].Controller.Reset = Value & 0x04;
			AtaChannel[Channel].Drive[0].Controller.DisableIrq = Value & 0x02;
			AtaChannel[Channel].Drive[1].Controller.DisableIrq = Value & 0x02;
			
			if (!PrevReset && AtaChannel[Channel].Drive[Dnum].Controller.Reset) {
				for (i = 0; i < 2; i++) {
					AtaChannel[Channel].Drive[i].Controller.Status |= IDE_STATUS_BUSY;
					AtaChannel[Channel].Drive[i].Controller.Status &= ~IDE_STATUS_DRIVE_RDY;
					AtaChannel[Channel].Drive[i].Controller.ResetInProgress = 1;

					AtaChannel[Channel].Drive[i].Controller.Status &= ~IDE_STATUS_WRITE_FAULT;
					AtaChannel[Channel].Drive[i].Controller.Status |= IDE_STATUS_DEV_SEEK_COMP;
					AtaChannel[Channel].Drive[i].Controller.Status &= ~IDE_STATUS_DEV_REQ;
					AtaChannel[Channel].Drive[i].Controller.Status &= ~IDE_STATUS_CORRECTED_DATA;
					AtaChannel[Channel].Drive[i].Controller.Status &= ~IDE_STATUS_ERROR;
					

					AtaChannel[Channel].Drive[i].Controller.ErrorRegister = 1;
					AtaChannel[Channel].Drive[i].Controller.CurrentCommand = 0;
					AtaChannel[Channel].Drive[i].Controller.BufferIndex = 0;

					AtaChannel[Channel].Drive[i].Controller.MultipleSectors = 0;
					AtaChannel[Channel].Drive[i].Controller.AddressMode = 0;
					AtaChannel[Channel].Drive[i].Controller.DisableIrq = 0;

					PicLowerIrq(AtaChannel[Channel].Irq);
				}
			}
			else if (AtaChannel[Channel].Drive[Dnum].Controller.ResetInProgress &&
				!AtaChannel[Channel].Drive[Dnum].Controller.Reset) {
				for (i = 0; i < 2; i++) {
					AtaChannel[Channel].Drive[i].Controller.Status &= ~IDE_STATUS_BUSY;
					AtaChannel[Channel].Drive[i].Controller.Status |= IDE_STATUS_DRIVE_RDY;
					AtaChannel[Channel].Drive[i].Controller.ResetInProgress = 0;
					HardDiskSetSignature(Channel, Dnum);
				}
			}
			break;
	}
}

ULONG HardDiskPortIoReadHandler(ULONG Address, ULONG Length)
{
	BYTE Channel, Dnum;
	DWORD Port, temp;

	for (Channel = 0; Channel < MAX_ATA_CHANNEL; Channel++) {
		if ((Address & 0xfff8) == AtaChannel[Channel].IoAddr1) {
			Port = Address - AtaChannel[Channel].IoAddr1;
			break;
		}
		if ((Address & 0xfff8) == AtaChannel[Channel].IoAddr2) {
			Port = Address - AtaChannel[Channel].IoAddr2 + 0x10;
			break;
		}
	}
	if (Channel == MAX_ATA_CHANNEL) {
		Port = Address - 0x3e0;
		Channel = 0;
	}
	Dnum = AtaChannel[Channel].DriveSelect ? 1 : 0;
	temp = 0;
	switch (Port) {
		case 0: // hard disk data (16bit) 0x1f0
			switch (AtaChannel[Channel].Drive[Dnum].Controller.CurrentCommand) {
				case IDE_COMMAND_READ:
				case IDE_COMMAND_READ_WITHOUT_RETRIES:
				case IDE_COMMAND_READ_MULTIPLE:
				case IDE_COMMAND_READ_EXT:
				case IDE_COMMAND_READ_MULTIPLE_EXT:
					if (AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex >= AtaChannel[Channel].Drive[Dnum].Controller.BufferSize) {
						fprintf(stderr, "Error Controller.BufferIndex > Controller.BufferSize\n");
						exit(-1);
					}
					if (AtaChannel[Channel].Drive[Dnum].Controller.BufferSize < AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex + Length)
						break;
					switch (Length) {
						case 4:
							temp = *(DWORD *)&AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex];
							break;
						case 2:
							temp = *(WORD*)&AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex];
							break;
						case 1:
							temp = *(BYTE *)&AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex];
					}
					AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex += Length;
					

					if (AtaChannel[Channel].Drive[Dnum].Controller.CurrentCommand == IDE_COMMAND_READ_MULTIPLE
						|| AtaChannel[Channel].Drive[Dnum].Controller.CurrentCommand == IDE_COMMAND_READ_MULTIPLE_EXT) {
						if (AtaChannel[Channel].Drive[Dnum].Controller.NumberOfSectors > AtaChannel[Channel].Drive[Dnum].Controller.MultipleSectors)
							AtaChannel[Channel].Drive[Dnum].Controller.BufferSize = AtaChannel[Channel].Drive[Dnum].Controller.MultipleSectors * HARDDISK_SECTOR_SIZE;
						else
							AtaChannel[Channel].Drive[Dnum].Controller.BufferSize = AtaChannel[Channel].Drive[Dnum].Controller.NumberOfSectors * HARDDISK_SECTOR_SIZE;
					}
					
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_SEEK_COMP;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_WRITE_FAULT;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_ERROR;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_CORRECTED_DATA;
					AtaChannel[Channel].Drive[Dnum].CurrentLSector = AtaChannel[Channel].Drive[Dnum].NextLSector;

					if (AtaChannel[Channel].Drive[Dnum].Controller.NumberOfSectors) {
						AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_REQ;
						//AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_SEEK_COMP;
						AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_SEEK_COMP;
						HardDiskReadSector(Channel, AtaChannel[Channel].Drive[Dnum].Controller.Buffer, AtaChannel[Channel].Drive[Dnum].Controller.BufferSize);
						AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex = 0;
						HardDiskRaiseInterrupt(Channel);
					}
					else {
						AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_REQ;
						AtaChannel[Channel].Drive[Dnum].CurrentLSector = AtaChannel[Channel].Drive[Dnum].NextLSector;
					}
					return temp;
					break;
				case IDE_COMMAND_IDENTIFY:
				case IDE_COMMAND_ATAPI_IDENTIFY:
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_WRITE_FAULT;
					AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_SEEK_COMP;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_CORRECTED_DATA;
					AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_ERROR;

					if (AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex + Length >= 512)
						AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_REQ;
					
					switch (Length) {
						case 1:
							temp = AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex];
							AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex++;
							return temp;
						case 2:
							temp = AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex];
							AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex++;
							temp |= AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex] << 8;
							AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex++;
							return temp;
						case 4:
							temp = AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex];
							AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex++;
							temp |= AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex] << 8;
							AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex++;
							temp |= AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex] << 16;
							AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex++;
							temp |= AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex] << 24;
							AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex++;
							return temp;
					}
					break;
				case IDE_COMMAND_ATAPI_PACKET:
					if (AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex == AtaChannel[Channel].Drive[Dnum].Controller.BufferSize) {
						switch (AtaChannel[Channel].Drive[Dnum].Controller.Atapi.AtapiCommand) {
							case SCSIOP_READ: // read (10)
							case SCSIOP_READ12: // read (12)
							case SCSIOP_READ_CD: // read cd
								temp = CdRomReadBlock(AtaChannel[Channel].Drive[Dnum].Controller.LbAddress,
									AtaChannel[Channel].Drive[Dnum].Controller.Buffer, 
									AtaChannel[Channel].Drive[Dnum].Controller.BufferSize);
								if (temp != AtaChannel[Channel].Drive[Dnum].Controller.BufferSize) {
									exit(-1);
								}
								AtaChannel[Channel].Drive[Dnum].Controller.LbAddress++;
								AtaChannel[Channel].Drive[Dnum].Controller.RemainingBlocks--;
								AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex = 0;
								break;
							default: // no need to load a new block
								break;
						}
					}
					if (AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex > AtaChannel[Channel].Drive[Dnum].Controller.BufferSize) {
						return 0;
					}
					temp = 0;
					switch (Length) {
						case 4:
							temp |= AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex + 3] << 24;
							temp |= AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex + 2] << 16;
						case 2:
							temp |= AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex + 1] << 8;
						case 1:
							temp |= AtaChannel[Channel].Drive[Dnum].Controller.Buffer[AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex];
					}
					AtaChannel[Channel].Drive[Dnum].Controller.BufferIndex += Length;
					AtaChannel[Channel].Drive[Dnum].Controller.DrqIndex += Length;

					if (AtaChannel[Channel].Drive[Dnum].Controller.DrqIndex >= AtaChannel[Channel].Drive[Dnum].Controller.Atapi.DrqBytes) {
						AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_REQ;
						AtaChannel[Channel].Drive[Dnum].Controller.DrqIndex = 0;

						AtaChannel[Channel].Drive[Dnum].Controller.Atapi.TotalBytesRemaining -= AtaChannel[Channel].Drive[Dnum].Controller.Atapi.DrqBytes;

						if (AtaChannel[Channel].Drive[Dnum].Controller.Atapi.TotalBytesRemaining > 0) {							
							AtaChannel[Channel].Drive[Dnum].Controller.interrupt_reason.i_o = 1;
							AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
							AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_REQ;
							AtaChannel[Channel].Drive[Dnum].Controller.interrupt_reason.c_d = 0;

							// set new byte count if last block
							if (AtaChannel[Channel].Drive[Dnum].Controller.Atapi.TotalBytesRemaining < AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber) {
								AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber = AtaChannel[Channel].Drive[Dnum].Controller.Atapi.TotalBytesRemaining;
							}
							AtaChannel[Channel].Drive[Dnum].Controller.Atapi.DrqBytes = AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber;
							HardDiskRaiseInterrupt(Channel);
						}
						else {
							// all bytes read
							AtaChannel[Channel].Drive[Dnum].Controller.interrupt_reason.i_o = 1;
							AtaChannel[Channel].Drive[Dnum].Controller.interrupt_reason.c_d = 1;
							AtaChannel[Channel].Drive[Dnum].Controller.interrupt_reason.rel = 0;
							AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
							AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
							AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_DEV_REQ;
							AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_ERROR;
							HardDiskRaiseInterrupt(Channel);
						}
					}

					return temp;
				default:
					break;
			}
			break;
		case 1:
			if (AtaChannel[Channel].Drive[Dnum].DriveType)
				return AtaChannel[Channel].Drive[Dnum].Controller.ErrorRegister;
			break;
		case 2:
			if (AtaChannel[Channel].Drive[Dnum].DriveType)
				return AtaChannel[Channel].Drive[Dnum].Controller.SectorCount;
			break;
		case 3:
			if (AtaChannel[Channel].Drive[Dnum].DriveType)
				return AtaChannel[Channel].Drive[Dnum].Controller.SectorNumber;
			break;
		case 4:
			if (AtaChannel[Channel].Drive[Dnum].DriveType)
				return AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber & 0xff;
			break;
		case 5:
			if (AtaChannel[Channel].Drive[Dnum].DriveType)
				return (AtaChannel[Channel].Drive[Dnum].Controller.CylinderNumber >> 8) & 0xff;
			break;
		case 6:
			return (1 << 7) |
				AtaChannel[Channel].Drive[Dnum].Controller.AddressMode |
				IDE_512_SECTOR_SIZE |
				AtaChannel[Channel].DriveSelect |
				(AtaChannel[Channel].Drive[Dnum].Controller.HeadNumber & IDE_HEAD_NUMBER);
			break;
		case 7: //0x1f7 Status-Interrupt disable Register
			if (!AtaChannel[Channel].Drive[Dnum].DriveType)
				return 0;
			PicLowerIrq(AtaChannel[Channel].Irq);
		case 0x16: //0x3f6 Status Register
			if (!AtaChannel[Channel].Drive[Dnum].DriveType)
				return 0;

			AtaChannel[Channel].Drive[Dnum].Controller.IndexPulse++;
			AtaChannel[Channel].Drive[Dnum].Controller.Status &= ~IDE_STATUS_INDEX_PULSE;
			if (AtaChannel[Channel].Drive[Dnum].Controller.IndexPulse > 10) {
				AtaChannel[Channel].Drive[Dnum].Controller.IndexPulse = 0;
				AtaChannel[Channel].Drive[Dnum].Controller.Status |= IDE_STATUS_INDEX_PULSE;
			}
			return AtaChannel[Channel].Drive[Dnum].Controller.Status;
		case 0x17: //0x3f7
			return 0xff;
	}
	return 0;
}



VOID HardDiskSeekTimer()
{
	ULONG i;
	BYTE Dnum;
	
	for (i = 0; i < 2; i++) {
		Dnum = AtaChannel[i].DriveSelect ? 1 : 0;
		if (AtaChannel[i].Drive[Dnum].DriveType == DRIVE_TYPE_DISK) {
			switch (AtaChannel[i].Drive[Dnum].Controller.CurrentCommand) {
				case IDE_COMMAND_READ_EXT:
				case IDE_COMMAND_READ_MULTIPLE_EXT:
				case IDE_COMMAND_READ:
				case IDE_COMMAND_READ_WITHOUT_RETRIES:
				case IDE_COMMAND_READ_MULTIPLE:
					AtaChannel[i].Drive[Dnum].Controller.ErrorRegister = 0;
					AtaChannel[i].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
					AtaChannel[i].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
					AtaChannel[i].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_SEEK_COMP;
					AtaChannel[i].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_REQ;
					AtaChannel[i].Drive[Dnum].Controller.Status &= ~IDE_STATUS_CORRECTED_DATA;
					AtaChannel[i].Drive[Dnum].Controller.BufferIndex = 0;
					HardDiskRaiseInterrupt(i);
					break;
				case IDE_COMMAND_READ_DMA_EXT:
				case IDE_COMMAND_READ_DMA:
					AtaChannel[i].Drive[Dnum].Controller.ErrorRegister = 0;
					AtaChannel[i].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
					AtaChannel[i].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
					AtaChannel[i].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_SEEK_COMP;
					AtaChannel[i].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_REQ;
					AtaChannel[i].Drive[Dnum].Controller.Status &= ~IDE_STATUS_CORRECTED_DATA;
					IdeStartTransfer(i);
					break;
				case IDE_COMMAND_SEEK:
					AtaChannel[i].Drive[Dnum].Controller.ErrorRegister = 0;
					AtaChannel[i].Drive[Dnum].Controller.Status &= ~IDE_STATUS_BUSY;
					AtaChannel[i].Drive[Dnum].Controller.Status |= IDE_STATUS_DRIVE_RDY;
					AtaChannel[i].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_SEEK_COMP;
					AtaChannel[i].Drive[Dnum].Controller.Status |= IDE_STATUS_DEV_REQ;
					AtaChannel[i].Drive[Dnum].Controller.Status &= ~IDE_STATUS_CORRECTED_DATA;
					AtaChannel[i].Drive[Dnum].Controller.BufferIndex = 0;
					HardDiskRaiseInterrupt(i);
					break;
			}
		}
		else if (AtaChannel[i].Drive[Dnum].DriveType == DRIVE_TYPE_CDROM) {
			switch (AtaChannel[i].Drive[Dnum].Controller.Atapi.AtapiCommand) {
				case SCSIOP_READ:
				case SCSIOP_READ12:
				case SCSIOP_READ_CD:
					HardDiskReadySendAtapi(i);
					break;
			}
		}
	}
	
}

VOID HardDiskInitialize()
{
	ULONG i, PciAddress;
	DWORD translation;
	USHORT devFunc;

	memset(&AtaChannel, '\0', sizeof(AtaChannel));

	devFunc = BX_PCI_DEVICE(1, 1);
	PciAddress = PCI_DEVFUNC_OFFSET_TO_ADDRESS(0, devFunc, 0);
	/*
	PciSetBarIo(PciAddress, 0, 0, 8, HardDiskPortIoReadHandler, HardDiskPortIoWriteHandler, IDE_PORT_MASK);
	PciSetBarIo(PciAddress, 1, 0, 8, HardDiskPortIoReadHandler, HardDiskPortIoWriteHandler, IDE_PORT_MASK);
	PciSetBarIo(PciAddress, 2, 0, 8, HardDiskPortIoReadHandler, HardDiskPortIoWriteHandler, IDE_PORT_MASK);
	PciSetBarIo(PciAddress, 3, 0, 8, HardDiskPortIoReadHandler, HardDiskPortIoWriteHandler, IDE_PORT_MASK);
	*/
	/* CHANNEL 0 HARD Drive */
	AtaChannel[0].IoAddr1 = 0x1f0;
	AtaChannel[0].IoAddr2 = 0x3f0;
	AtaChannel[0].Irq = 14;
	AtaChannel[0].Drive[0].DriveType = DRIVE_TYPE_DISK;
	AtaChannel[0].Drive[0].Controller.BufferTotalSize = MAX_MULTIPLE_SECTORS * HARDDISK_SECTOR_SIZE;
	AtaChannel[0].Drive[0].Controller.Buffer = (BYTE*)calloc(sizeof(BYTE), MAX_MULTIPLE_SECTORS * HARDDISK_SECTOR_SIZE + 6);
	
	for (i = 0; i < 8; i++)
		RegisterPortIoHandler(AtaChannel[0].IoAddr1 + i, HardDiskPortIoWriteHandler, HardDiskPortIoReadHandler);
	for (i = 0; i < 8; i++)
		RegisterPortIoHandler(AtaChannel[0].IoAddr2 + i, HardDiskPortIoWriteHandler, HardDiskPortIoReadHandler);
	
	//SparseInitializeHardDisk("c.img");
	VhdInitializeHardDisk("c.vhd");
	AtaChannel[0].Drive[0].Cylinders = VhdGetHardDiskSize() / (HARDDISK_HEADS * HARDDISK_SECTORS * HARDDISK_SECTOR_SIZE);
	AtaChannel[0].Drive[0].Heads = HARDDISK_HEADS;
	AtaChannel[0].Drive[0].Spt = HARDDISK_SPT;
	AtaChannel[0].Drive[0].CurrentLSector = VhdGetHardDiskSize() / HARDDISK_SECTOR_SIZE;
	
	//controller initialize
	AtaChannel[0].Drive[0].Controller.Status = IDE_STATUS_DRIVE_RDY;
	AtaChannel[0].Drive[0].Controller.Status |= IDE_STATUS_DEV_SEEK_COMP;
	AtaChannel[0].Drive[0].Controller.ErrorRegister = 1;
	AtaChannel[0].Drive[0].Controller.SectorCount = 1;
	AtaChannel[0].Drive[0].Controller.SectorNumber = 1;

	/* CMOS Hard Disk */
	CmosSetRegister(0x12, 0);
	CmosSetRegister(0x12, (CmosGetRegister(0x12) & 0x0f) | 0xf0);
	CmosSetRegister(0x19, 47); // user definable type
	// AMI BIOS: 1st hard disk #cyl low byte
	CmosSetRegister(0x1b, (AtaChannel[0].Drive[0].Cylinders & 0x00ff));
	// AMI BIOS: 1st hard disk #cyl high byte
	CmosSetRegister(0x1c, (AtaChannel[0].Drive[0].Cylinders & 0xff00) >> 8);
	// AMI BIOS: 1st hard disk #heads
	CmosSetRegister(0x1d, HARDDISK_HEADS);
	// AMI BIOS: 1st hard disk write precompensation cylinder, low byte
	CmosSetRegister(0x1e, 0xff); // -1
	// AMI BIOS: 1st hard disk write precompensation cylinder, high byte
	CmosSetRegister(0x1f, 0xff); // -1
	// AMI BIOS: 1st hard disk control byte
	CmosSetRegister(0x20, (0xc0 | ((HARDDISK_HEADS > 8) << 3)));
	// AMI BIOS: 1st hard disk landing zone, low byte
	CmosSetRegister(0x21, CmosGetRegister(0x1b));
	// AMI BIOS: 1st hard disk landing zone, high byte
	CmosSetRegister(0x22, CmosGetRegister(0x1c));
	// AMI BIOS: 1st hard disk sectors/track
	CmosSetRegister(0x23, HARDDISK_SECTORS);


	// Find the right translation if autodetect
	if ((AtaChannel[0].Drive[0].Cylinders <= 1024) && (HARDDISK_HEADS <= 16) && (HARDDISK_SECTORS <= 63)) {
		translation = BX_ATA_TRANSLATION_NONE;
	
	}
	else if (((DWORD)AtaChannel[0].Drive[0].Cylinders * (DWORD)HARDDISK_HEADS) <= 131072) {
		translation = BX_ATA_TRANSLATION_LARGE;
	}
	else 
		translation = BX_ATA_TRANSLATION_LBA;

	switch (translation) {
		case BX_ATA_TRANSLATION_NONE:
			CmosSetRegister(0x39, CmosGetRegister(0x39) | (0 << 0));
			break;
		case BX_ATA_TRANSLATION_LBA:
			CmosSetRegister(0x39, CmosGetRegister(0x39) | (1 << 0));
			break;
		case BX_ATA_TRANSLATION_LARGE:
			CmosSetRegister(0x39, CmosGetRegister(0x39) | (2 << 0));
			break;
		case BX_ATA_TRANSLATION_RECHS:
			CmosSetRegister(0x39, CmosGetRegister(0x39) | (3 << 0));
			break;
	}
	//CmosSetRegister(0x3b, CmosGetRegister(0x3b));


	/* CHANNEL 1 CD Drive */
	AtaChannel[1].IoAddr1 = 0x170;
	AtaChannel[1].IoAddr2 = 0x370;
	AtaChannel[1].Irq = 15;
	AtaChannel[1].Drive[0].DriveType = DRIVE_TYPE_CDROM;
	AtaChannel[1].Drive[0].Controller.BufferTotalSize = 2352;
	AtaChannel[1].Drive[0].Controller.Buffer = (BYTE*)calloc(sizeof(BYTE), 2356);
	
	for (i = 0; i < 8; i++)
		RegisterPortIoHandler(AtaChannel[1].IoAddr1 + i, HardDiskPortIoWriteHandler, HardDiskPortIoReadHandler);
	
	for (i = 0; i < 8; i++)
		RegisterPortIoHandler(AtaChannel[1].IoAddr2 + i, HardDiskPortIoWriteHandler, HardDiskPortIoReadHandler);
	
	//CdRomInitialize("bootcd.iso");

	//controller initialize
	AtaChannel[1].Drive[0].Controller.Status = IDE_STATUS_DRIVE_RDY;
	AtaChannel[1].Drive[0].Controller.Status |= IDE_STATUS_DEV_SEEK_COMP;
	AtaChannel[1].Drive[0].Controller.ErrorRegister = 1;
	AtaChannel[1].Drive[0].Controller.SectorCount = 1;
	AtaChannel[1].Drive[0].Controller.SectorNumber = 1;

	/* two none devices */
	
	AtaChannel[2].IoAddr1 = 0x1e8;
	AtaChannel[2].IoAddr2 = 0x3e0;
	for (i = 0; i < 8; i++)
		RegisterPortIoHandler(AtaChannel[2].IoAddr1 + i, HardDiskPortIoWriteHandler, HardDiskPortIoReadHandler);

	for (i = 0; i < 8; i++)
		RegisterPortIoHandler(AtaChannel[2].IoAddr2 + i, HardDiskPortIoWriteHandler, HardDiskPortIoReadHandler);
	AtaChannel[3].IoAddr1 = 0x168;
	AtaChannel[3].IoAddr2 = 0x360;
	for (i = 0; i < 8; i++)
		RegisterPortIoHandler(AtaChannel[3].IoAddr1 + i, HardDiskPortIoWriteHandler, HardDiskPortIoReadHandler);

	for (i = 0; i < 16; i++)
		RegisterPortIoHandler(AtaChannel[3].IoAddr2 + i, HardDiskPortIoWriteHandler, HardDiskPortIoReadHandler);
	
	//auto detect traslation
	CmosSetRegister(0x3a, 0);
	CmosSetRegister(0x3b, 0);
	//0x3e8
	//TimerRegister(MSECONDS_TO_NS(10), HardDiskSeekTimer, NULL);
	TimerRegister(TICK_PERIOD * 5, HardDiskSeekTimer, NULL);
	
}


