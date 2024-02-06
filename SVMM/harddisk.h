#pragma once
#include <Windows.h>

#define DRIVE_TYPE_NONE		0
#define DRIVE_TYPE_CDROM	1
#define DRIVE_TYPE_DISK 	2

#define LBA_28				(1 << 0)
#define LBA_48				(1 << 1)

#define MAX_ATA_CHANNEL		4
#define MAX_MULTIPLE_SECTORS 16

#define HARDDISK_CYLINDERS		0
#define HARDDISK_HEADS			16
#define HARDDISK_SECTORS		63
#define HARDDISK_SPT			63

#define HARDDISK_SECTOR_SIZE	512

#define PACKET_SIZE	12

//
// ATAPI specific scsiops
//
#define ATAPI_MODE_SENSE        0x5A
#define ATAPI_MODE_SELECT       0x55
#define ATAPI_LS120_FORMAT_UNIT 0x24

//
// IDE driveSelect register bit for LBA mode
//

#define IDE_LBA_MODE			(1 << 6)
#define IDE_512_SECTOR_SIZE		(1 << 5)
#define IDE_DRIVE_SELECT		(1 << 4)
#define IDE_HEAD_NUMBER			((1 << 4) - 1)

//
// IDE drive control definitions
//
#define IDE_DC_DISABLE_INTERRUPTS    0x02
#define IDE_DC_RESET_CONTROLLER      0x04
#define IDE_DC_REENABLE_CONTROLLER   0x00

//
// IDE status definitions
//
#define IDE_STATUS_ERROR             (1 << 0)
#define IDE_STATUS_INDEX_PULSE       (1 << 1)
#define IDE_STATUS_CORRECTED_DATA    (1 << 2)
#define IDE_STATUS_DEV_REQ	         (1 << 3)
#define IDE_STATUS_DEV_SEEK_COMP     (1 << 4)
#define IDE_STATUS_WRITE_FAULT       (1 << 5)
#define IDE_STATUS_DRIVE_RDY         (1 << 6)
#define IDE_STATUS_BUSY              (1 << 7)
#define IDE_STATUS_IDLE              0x50
//
// IDE error definitions
//
#define IDE_ERROR_BAD_BLOCK          0x80
#define IDE_ERROR_CRC_ERROR          IDE_ERROR_BAD_BLOCK
#define IDE_ERROR_DATA_ERROR         0x40
#define IDE_ERROR_MEDIA_CHANGE       0x20
#define IDE_ERROR_ID_NOT_FOUND       0x10
#define IDE_ERROR_MEDIA_CHANGE_REQ   0x08
#define IDE_ERROR_COMMAND_ABORTED    0x04
#define IDE_ERROR_END_OF_MEDIA       0x02
#define IDE_ERROR_ILLEGAL_LENGTH     0x01
#define IDE_ERROR_ADDRESS_NOT_FOUND  IDE_ERROR_ILLEGAL_LENGTH

#define INDEX_PULSE_CYCLE			10

//
// IDE command definitions
//
#define IDE_COMMAND_NOP                         0x00
#define IDE_COMMAND_REQUEST_SENSE				0x03
#define IDE_COMMAND_DATA_SET_MANAGEMENT         0x06
#define IDE_COMMAND_ATAPI_RESET                 0x08
#define IDE_COMMAND_GET_PHYSICAL_ELEMENT_STATUS 0x12
#define IDE_COMMAND_MODE_SENSE					0x1A

#define IDE_COMMAND_START_STOP_UNIT				0x1B

#define IDE_COMMAND_READ                        0x20
#define IDE_COMMAND_READ_WITHOUT_RETRIES        0x21
#define IDE_COMMAND_READ_EXT                    0x24
#define IDE_COMMAND_READ_DMA_EXT                0x25
#define IDE_COMMAND_READ_DMA_QUEUED_EXT         0x26
#define IDE_COMMAND_READ_NATIVE_ADDRESS_EXT		0x27
#define IDE_COMMAND_READ_MULTIPLE_EXT           0x29
#define IDE_COMMAND_READ_LOG_EXT                0x2f
#define IDE_COMMAND_WRITE                       0x30
#define IDE_COMMAND_WRITE_EXT                   0x34
#define IDE_COMMAND_WRITE_DMA_EXT               0x35
#define IDE_COMMAND_WRITE_DMA_QUEUED_EXT        0x36
#define IDE_COMMAND_WRITE_MULTIPLE_EXT          0x39
#define IDE_COMMAND_WRITE_DMA_FUA_EXT           0x3D
#define IDE_COMMAND_WRITE_DMA_QUEUED_FUA_EXT    0x3E
#define IDE_COMMAND_WRITE_LOG_EXT               0x3f
#define IDE_COMMAND_VERIFY                      0x40
#define IDE_COMMAND_VERIFY_NO_RETRY				0x41
#define IDE_COMMAND_VERIFY_EXT                  0x42
#define IDE_COMMAND_ZAC_MANAGEMENT_IN           0x4A        // Report Zones Ext
#define IDE_COMMAND_WRITE_LOG_DMA_EXT           0x57
#define IDE_COMMAND_TRUSTED_NON_DATA            0x5B
#define IDE_COMMAND_TRUSTED_RECEIVE             0x5C
#define IDE_COMMAND_TRUSTED_RECEIVE_DMA         0x5D
#define IDE_COMMAND_TRUSTED_SEND                0x5E
#define IDE_COMMAND_TRUSTED_SEND_DMA            0x5F
#define IDE_COMMAND_READ_FPDMA_QUEUED           0x60        // NCQ Read command
#define IDE_COMMAND_WRITE_FPDMA_QUEUED          0x61        // NCQ Write command
#define IDE_COMMAND_NCQ_NON_DATA                0x63        // NCQ Non-Data command
#define IDE_COMMAND_SEND_FPDMA_QUEUED           0x64        // NCQ Send command
#define IDE_COMMAND_RECEIVE_FPDMA_QUEUED        0x65        // NCQ Receive command
#define IDE_COMMAND_SEEK				        0x70        // NCQ Receive command

#define IDE_COMMAND_SET_DATE_AND_TIME           0x77        // optional 48bit command
#define IDE_COMMAND_REMOVE_ELEMENT_AND_TRUNCATE 0x7C
#define IDE_COMMAND_EXECUTE_DEVICE_DIAGNOSTIC   0x90
#define IDE_COMMAND_SET_DRIVE_PARAMETERS        0x91
#define IDE_COMMAND_DOWNLOAD_MICROCODE          0x92        // Optional 28bit command
#define IDE_COMMAND_DOWNLOAD_MICROCODE_DMA      0x93        // Optional 28bit command
#define IDE_COMMAND_ZAC_MANAGEMENT_OUT          0x9F        // Close Zone Ext; Finish Zone Ext; Open Zone Ext; Reset Write Pointer Ext.
#define IDE_COMMAND_ATAPI_PACKET                0xA0
#define IDE_COMMAND_ATAPI_IDENTIFY              0xA1
#define IDE_COMMAND_ATAPI_SERVICE				0xA2
#define IDE_COMMAND_SMART                       0xB0
#define IDE_COMMAND_READ_LOG_DMA_EXT            0xB1
#define IDE_COMMAND_SANITIZE_DEVICE             0xB4
#define IDE_COMMAND_READ_MULTIPLE               0xC4
#define IDE_COMMAND_WRITE_MULTIPLE              0xC5
#define IDE_COMMAND_SET_MULTIPLE                0xC6
#define IDE_COMMAND_READ_DMA                    0xC8
#define IDE_COMMAND_WRITE_DMA                   0xCA
#define IDE_COMMAND_WRITE_DMA_QUEUED            0xCC
#define IDE_COMMAND_WRITE_MULTIPLE_FUA_EXT      0xCE
#define IDE_COMMAND_GET_MEDIA_STATUS            0xDA
#define IDE_COMMAND_DOOR_LOCK                   0xDE
#define IDE_COMMAND_DOOR_UNLOCK                 0xDF
#define IDE_COMMAND_STANDBY_IMMEDIATE           0xE0
#define IDE_COMMAND_IDLE_IMMEDIATE              0xE1
#define IDE_COMMAND_CHECK_POWER                 0xE5
#define IDE_COMMAND_SLEEP                       0xE6
#define IDE_COMMAND_FLUSH_CACHE                 0xE7
#define IDE_COMMAND_FLUSH_CACHE_EXT             0xEA
#define IDE_COMMAND_IDENTIFY                    0xEC
#define IDE_COMMAND_MEDIA_EJECT                 0xED
#define IDE_COMMAND_SET_FEATURE                 0xEF
#define IDE_COMMAND_SECURITY_SET_PASSWORD       0xF1
#define IDE_COMMAND_SECURITY_UNLOCK             0xF2
#define IDE_COMMAND_SECURITY_ERASE_PREPARE      0xF3
#define IDE_COMMAND_SECURITY_ERASE_UNIT         0xF4
#define IDE_COMMAND_SECURITY_FREEZE_LOCK        0xF5
#define IDE_COMMAND_SECURITY_DISABLE_PASSWORD   0xF6
#define IDE_COMMAND_READ_NATIVE_ADDRESS		    0xF8

#define IDE_COMMAND_NOT_VALID                   0xFF

#define SCSIOP_READ_FORMATTED_CAPACITY  0x23
#define SCSIOP_READ_CAPACITY            0x25
#define SCSIOP_READ                     0x28
#define SCSIOP_WRITE                    0x2A
#define SCSIOP_SEEK                     0x2B
#define SCSIOP_LOCATE                   0x2B
#define SCSIOP_POSITION_TO_ELEMENT      0x2B
#define SCSIOP_WRITE_VERIFY             0x2E
#define SCSIOP_VERIFY                   0x2F
#define SCSIOP_SEARCH_DATA_HIGH         0x30
#define SCSIOP_SEARCH_DATA_EQUAL        0x31
#define SCSIOP_SEARCH_DATA_LOW          0x32
#define SCSIOP_SET_LIMITS               0x33
#define SCSIOP_READ_POSITION            0x34
#define SCSIOP_SYNCHRONIZE_CACHE        0x35
#define SCSIOP_COMPARE                  0x39
#define SCSIOP_COPY_COMPARE             0x3A
#define SCSIOP_WRITE_DATA_BUFF          0x3B
#define SCSIOP_READ_DATA_BUFF           0x3C
#define SCSIOP_WRITE_LONG               0x3F
#define SCSIOP_CHANGE_DEFINITION        0x40
#define SCSIOP_WRITE_SAME               0x41
#define SCSIOP_READ_SUB_CHANNEL         0x42
#define SCSIOP_UNMAP                    0x42 
#define SCSIOP_READ_TOC                 0x43
#define SCSIOP_READ_HEADER              0x44
#define SCSIOP_REPORT_DENSITY_SUPPORT   0x44 
#define SCSIOP_PLAY_AUDIO               0x45
#define SCSIOP_GET_CONFIGURATION        0x46
#define SCSIOP_PLAY_AUDIO_MSF           0x47
#define SCSIOP_PLAY_TRACK_INDEX         0x48
#define SCSIOP_SANITIZE                 0x48 
#define SCSIOP_PLAY_TRACK_RELATIVE      0x49
#define SCSIOP_GET_EVENT_STATUS         0x4A
#define SCSIOP_PAUSE_RESUME             0x4B
#define SCSIOP_LOG_SELECT               0x4C
#define SCSIOP_LOG_SENSE                0x4D
#define SCSIOP_STOP_PLAY_SCAN           0x4E
#define SCSIOP_XDWRITE                  0x50
#define SCSIOP_XPWRITE                  0x51
#define SCSIOP_READ_DISK_INFORMATION    0x51
#define SCSIOP_READ_TRACK_INFORMATION   0x52
#define SCSIOP_XDWRITE_READ             0x53
#define SCSIOP_RESERVE_TRACK_RZONE      0x53
#define SCSIOP_SEND_OPC_INFORMATION     0x54 
#define SCSIOP_MODE_SELECT10            0x55
#define SCSIOP_RESERVE_UNIT10           0x56
#define SCSIOP_RESERVE_ELEMENT          0x56
#define SCSIOP_RELEASE_UNIT10           0x57
#define SCSIOP_RELEASE_ELEMENT          0x57
#define SCSIOP_REPAIR_TRACK             0x58
#define SCSIOP_MODE_SENSE10             0x5A
#define SCSIOP_CLOSE_TRACK_SESSION      0x5B
#define SCSIOP_READ_BUFFER_CAPACITY     0x5C
#define SCSIOP_SEND_CUE_SHEET           0x5D
#define SCSIOP_PERSISTENT_RESERVE_IN    0x5E
#define SCSIOP_PERSISTENT_RESERVE_OUT   0x5F

// 12-byte commands
#define SCSIOP_REPORT_LUNS              0xA0
#define SCSIOP_BLANK                    0xA1
#define SCSIOP_ATA_PASSTHROUGH12        0xA1
#define SCSIOP_SEND_EVENT               0xA2
#define SCSIOP_SECURITY_PROTOCOL_IN     0xA2
#define SCSIOP_SEND_KEY                 0xA3
#define SCSIOP_MAINTENANCE_IN           0xA3
#define SCSIOP_REPORT_KEY               0xA4
#define SCSIOP_MAINTENANCE_OUT          0xA4
#define SCSIOP_MOVE_MEDIUM              0xA5
#define SCSIOP_LOAD_UNLOAD_SLOT         0xA6
#define SCSIOP_EXCHANGE_MEDIUM          0xA6
#define SCSIOP_SET_READ_AHEAD           0xA7
#define SCSIOP_MOVE_MEDIUM_ATTACHED     0xA7
#define SCSIOP_READ12                   0xA8
#define SCSIOP_GET_MESSAGE              0xA8
#define SCSIOP_SERVICE_ACTION_OUT12     0xA9
#define SCSIOP_WRITE12                  0xAA
#define SCSIOP_SEND_MESSAGE             0xAB
#define SCSIOP_SERVICE_ACTION_IN12      0xAB
#define SCSIOP_GET_PERFORMANCE          0xAC
#define SCSIOP_READ_DVD_STRUCTURE       0xAD
#define SCSIOP_WRITE_VERIFY12           0xAE
#define SCSIOP_VERIFY12                 0xAF
#define SCSIOP_SEARCH_DATA_HIGH12       0xB0
#define SCSIOP_SEARCH_DATA_EQUAL12      0xB1
#define SCSIOP_SEARCH_DATA_LOW12        0xB2
#define SCSIOP_SET_LIMITS12             0xB3
#define SCSIOP_READ_ELEMENT_STATUS_ATTACHED 0xB4
#define SCSIOP_REQUEST_VOL_ELEMENT      0xB5
#define SCSIOP_SECURITY_PROTOCOL_OUT    0xB5
#define SCSIOP_SEND_VOLUME_TAG          0xB6
#define SCSIOP_SET_STREAMING            0xB6 // C/DVD
#define SCSIOP_READ_DEFECT_DATA         0xB7
#define SCSIOP_READ_ELEMENT_STATUS      0xB8
#define SCSIOP_READ_CD_MSF              0xB9
#define SCSIOP_SCAN_CD                  0xBA
#define SCSIOP_REDUNDANCY_GROUP_IN      0xBA
#define SCSIOP_SET_CD_SPEED             0xBB
#define SCSIOP_REDUNDANCY_GROUP_OUT     0xBB
#define SCSIOP_PLAY_CD                  0xBC
#define SCSIOP_SPARE_IN                 0xBC
#define SCSIOP_MECHANISM_STATUS         0xBD
#define SCSIOP_SPARE_OUT                0xBD
#define SCSIOP_READ_CD                  0xBE
#define SCSIOP_VOLUME_SET_IN            0xBE
#define SCSIOP_SEND_DVD_STRUCTURE       0xBF
#define SCSIOP_VOLUME_SET_OUT           0xBF
#define SCSIOP_INIT_ELEMENT_RANGE       0xE7

//
// IDE Set Transfer Mode
//
#define IDE_SET_DEFAULT_PIO_MODE(mode)      ((UCHAR) 1)     // disable I/O Ready
#define IDE_SET_ADVANCE_PIO_MODE(mode)      ((UCHAR) ((1 << 3) | (mode)))
#define IDE_SET_SWDMA_MODE(mode)            ((UCHAR) ((1 << 4) | (mode)))
#define IDE_SET_MWDMA_MODE(mode)            ((UCHAR) ((1 << 5) | (mode)))
#define IDE_SET_UDMA_MODE(mode)             ((UCHAR) ((1 << 6) | (mode)))

//
// Set features parameter list
//

#define IDE_FEATURE_ENABLE_WRITE_CACHE          0x2
#define IDE_FEATURE_SET_TRANSFER_MODE           0x3
#define IDE_FEATURE_ENABLE_PUIS                 0x6
#define IDE_FEATURE_PUIS_SPIN_UP                0x7
#define IDE_FEATURE_ENABLE_SATA_FEATURE         0x10
#define IDE_FEATURE_DISABLE_MSN                 0x31
#define IDE_FEATURE_DISABLE_REVERT_TO_POWER_ON  0x66
#define IDE_FEATURE_DISABLE_WRITE_CACHE         0x82
#define IDE_FEATURE_DISABLE_PUIS                0x86
#define IDE_FEATURE_DISABLE_SATA_FEATURE        0x90
#define IDE_FEATURE_ENABLE_MSN                  0x95

//
// SATA Features Sector Count parameter list
//

#define IDE_SATA_FEATURE_NON_ZERO_DMA_BUFFER_OFFSET         0x1
#define IDE_SATA_FEATURE_DMA_SETUP_FIS_AUTO_ACTIVATE        0x2
#define IDE_SATA_FEATURE_DEVICE_INITIATED_POWER_MANAGEMENT  0x3
#define IDE_SATA_FEATURE_GUARANTEED_IN_ORDER_DELIVERY       0x4
#define IDE_SATA_FEATURE_ASYNCHRONOUS_NOTIFICATION          0x5
#define IDE_SATA_FEATURE_SOFTWARE_SETTINGS_PRESERVATION     0x6
#define IDE_SATA_FEATURE_DEVICE_AUTO_PARTIAL_TO_SLUMBER     0x7
#define IDE_SATA_FEATURE_ENABLE_HARDWARE_FEATURE_CONTROL    0x8
#define IDE_SATA_FEATURE_DEVSLP                             0x9
#define IDE_SATA_FEATURE_HYBRID_INFORMATION                 0xa

//
// SMART sub command list
//
#define IDE_SMART_READ_ATTRIBUTES               0xD0
#define IDE_SMART_READ_THRESHOLDS               0xD1
#define IDE_SMART_ENABLE_DISABLE_AUTOSAVE       0xD2
#define IDE_SMART_SAVE_ATTRIBUTE_VALUES         0xD3
#define IDE_SMART_EXECUTE_OFFLINE_DIAGS         0xD4
#define IDE_SMART_READ_LOG                      0xD5
#define IDE_SMART_WRITE_LOG                     0xD6
#define IDE_SMART_ENABLE                        0xD8
#define IDE_SMART_DISABLE                       0xD9
#define IDE_SMART_RETURN_STATUS                 0xDA
#define IDE_SMART_ENABLE_DISABLE_AUTO_OFFLINE   0xDB

//
// Features for IDE_COMMAND_DATA_SET_MANAGEMENT
//
#define IDE_DSM_FEATURE_TRIM                  0x0001    //bit 0 of WORD

//
// NCQ sub command list
//

// SubCommand of IDE_COMMAND_NCQ_NON_DATA
#define IDE_NCQ_NON_DATA_ABORT_NCQ_QUEUE                0x00
#define IDE_NCQ_NON_DATA_DEADLINE_HANDLING              0x01
#define IDE_NCQ_NON_DATA_HYBRID_CHANGE_BY_SIZE          0x02    // this subCommand has been renamed to Hybrid Demote by Size.
#define IDE_NCQ_NON_DATA_HYBRID_DEMOTE_BY_SIZE          0x02
#define IDE_NCQ_NON_DATA_HYBRID_CHANGE_BY_LBA_RANGE     0x03
#define IDE_NCQ_NON_DATA_HYBRID_CONTROL                 0x04

// SubCommand of IDE_COMMAND_SEND_FPDMA_QUEUED
#define IDE_NCQ_SEND_DATA_SET_MANAGEMENT                0x00
#define IDE_NCQ_SEND_HYBRID_EVICT                       0x01


#define IDE_SENSE_NONE				0
#define IDE_SENSE_NOT_READY			2
#define IDE_SENSE_ILLEGAL_REQUEST   5
#define IDE_SENSE_UNIT_ATTENTION    6

#define IDE_ASC_ILLEGAL_OPCODE						0x20
#define IDE_ASC_LOGICAL_BLOCK_OOR					0x21
#define IDE_ASC_INV_FIELD_IN_CMD_PACKET				0x24
#define IDE_ASC_MEDIUM_MAY_HAVE_CHANGED				0x28
#define IDE_ASC_SAVING_PARAMETERS_NOT_SUPPORTED		0x39
#define IDE_ASC_MEDIUM_NOT_PRESENT					0x3a

enum {
	BX_ATA_TRANSLATION_NONE,
	BX_ATA_TRANSLATION_LBA,
	BX_ATA_TRANSLATION_LARGE,
	BX_ATA_TRANSLATION_RECHS,
	BX_ATA_TRANSLATION_AUTO
};



typedef struct _SENSE_INFO {
	BYTE sense_key;
	struct {
		BYTE arr[4];
	} information;
	struct {
		BYTE arr[4];
	} specific_inf;
	struct {
		BYTE arr[3];
	} key_spec;
	BYTE fruc;
	BYTE asc;
	BYTE ascq;
}SENSE_INFO, *PSENSE_INFO;

typedef struct _ATAPI {
	BYTE AtapiCommand;
	DWORD DrqBytes;
	DWORD TotalBytesRemaining;
} ATAPI, *PATAPI;

typedef struct _HOB {
	BYTE Feature;
	BYTE LowCylinder;
	BYTE HighCylinder;
	BYTE Sector;
	BYTE Nsector;
} HOB;

typedef struct _CONTROLLER {
	/* buffer */
	BYTE* Buffer;
	DWORD BufferTotalSize;
	DWORD BufferSize;
	DWORD BufferIndex;
	

	/* sectors */
	DWORD NumberOfSectors;
	union {
		BYTE    SectorCount;
		struct {
			unsigned c_d : 1;
			unsigned i_o : 1;
			unsigned rel : 1;
			unsigned tag : 5;

		} interrupt_reason;
	};
	BYTE  SectorNumber;
	BYTE MultipleSectors;

	/* lb Address */
	BYTE AddressMode;
	DWORD LbAddress;
	BYTE Lba48;
	HOB Hob;

	/* head */
	BYTE HeadNumber;
	
	/*  Cylinder */
	WORD CylinderNumber;

	/* status */
	BYTE Status;
	BYTE ErrorRegister;
	
	/* reset */
	BYTE IndexPulse;
	BYTE ResetInProgress;
	BYTE Reset;

	/* dma */
	BYTE MdmaMode;
	BYTE UdmaMode;
	BYTE PacketDma;

	/* IDE */
	BYTE  CurrentCommand;
	DWORD DrqIndex;
	BYTE Features;
	BYTE DisableIrq;
	
	/* cd rom */
	ATAPI Atapi;
	SENSE_INFO Sense;
	DWORD RemainingBlocks;
	BYTE Locked;
} CONTROLLER, *PCONTROLLER;


typedef struct _DRIVE {
	WORD DriveId[256];
	BYTE Model[41];
	BYTE DriveType;
	CONTROLLER Controller;
	
	ULONG64 Cylinders;
	ULONG64 Heads;
	ULONG64 Spt;
	ULONG64 NextLSector;
	ULONG64 CurrentLSector;
	BYTE IdentifySet;
} DRIVE, *PDRIVE;

typedef struct _ATA_CHANNEL {
	WORD IoAddr1;
	WORD IoAddr2;
	BYTE Irq;
	DRIVE Drive[2];
	BYTE DriveSelect;
} ATA_CHANNEL, *PATA_CHANNEL;

VOID HardDiskInitialize();
VOID HardDiskPortIoWriteHandler(ULONG Address, ULONG Value, ULONG Length);
ULONG HardDiskPortIoReadHandler(ULONG Address, ULONG Length);
VOID HardDiskReadySendAtapi(BYTE Channel);
VOID HardDiskInitAtapiCommand(BYTE Channel, BYTE Command, DWORD ReqLength, DWORD AllocLength);
ULONG64 HardDiskCalculateCHS(BYTE Channel, ULONG64 LogicalSector);
ULONG64 HardDiskCalculateLogicalAddress(BYTE Channel);
ULONG64 HardDiskBmdmaReadSector(BYTE Channel, BYTE* Buffer, DWORD BufferSize);
ULONG64 HardDiskBmdmaWriteSector(BYTE Channel, BYTE* Buffer, DWORD BufferSize);
VOID HardDiskBdmaComplete(BYTE Channel);

