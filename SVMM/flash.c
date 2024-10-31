#include "flash.h"
#include "pci.h"
#include <stdio.h>

#define FLASH_MMIO   0xffc00000 
#define FLASH_SIZE   0x400000

struct Flash Flash;



VOID FlashWriteMmioHandler(ULONG Address, BYTE* Data, ULONG Length)
{
    Address -= FLASH_MMIO;

    switch (Flash.Wcycle) {
        case 0:
            switch (Flash.Cmd) {
                case 0x10: /* Single Byte Program */
                case 0x40: /* Single Byte Program */
                case 0x60: /* Block (un)lock */
                case 0x98: /* CFI query */
                    Flash.Wcycle++;
                    Flash.Cmd = *(BYTE*)Data;
                    break;
                case 0x70: /* Status Register */
                case 0x90: /* Read Device ID */
                    Flash.Cmd = *(BYTE*)Data;
                    break;
                case 0xe8: /* Write to buffer */
                    Flash.Status |= 0x80;
                    Flash.Wcycle++;
                    Flash.Cmd = *(BYTE*)Data;
                    break;
                case 0x20: /* Block erase */
                    /*
                    p = pfl->storage;
                    offset &= ~(pfl->sector_len - 1);

                    trace_pflash_write_block_erase(pfl->name, offset, pfl->sector_len);

                    if (!pfl->ro) {
                        memset(p + offset, 0xff, pfl->sector_len);
                        pflash_update(pfl, offset, pfl->sector_len);
                    }
                    else {
                        Flash.Status |= 0x20;
                    }
                    Flash.Status |= 0x80;
                    */
                    break;
                case 0x50: /* Clear status bits */
                    Flash.Status = 0x0;
                    Flash.Wcycle = 0;
                    Flash.Cmd = 0x00;
                    break;
                case 0x00:
                case 0xf0: /* Probe for AMD flash */
                case 0xff: /* Read Array */
                default:
                    Flash.Wcycle = 0;
                    Flash.Cmd = 0;
                    break;
            }
            break;
        case 1:
            switch (Flash.Cmd) {
                case 0x10: /* Single Byte Program */
                case 0x40: /* Single Byte Program */
                    /*
                    if (!pfl->ro) {
                        pflash_data_write(pfl, offset, value, width, be);
                        pflash_update(pfl, offset, width);
                    }
                    else {
                        Flash.Status |= 0x10; 
                    }
                    */
                    //Flash.Status |= 0x10; /* Programming error */
                    if (Address + Length >= FLASH_SIZE)
                        return;
                    memcpy(Flash.Storage + Address, Data, Length);
                    Flash.Status |= 0x80; /* Ready! */
                    Flash.Wcycle = 0;
                    break;
                case 0x20: /* Block erase */
                case 0x28:
                    if (*(BYTE *)Data == 0xd0) { /* confirm */
                        Flash.Wcycle = 0;
                        Flash.Status |= 0x80;
                    }
                    else {
                        Flash.Wcycle = 0;
                        Flash.Cmd = 0;
                    }
                    /*
                    else if (*(BYTE*)Data == 0xff) { 
                        Flash.Wcycle = 0;
                        Flash.Cmd = 0;
                    }
                    else 
                        goto error_flash;
                    */
                    break;
                case 0xe8:
                    /*
                     * Mask writeblock size based on device width, or bank width if
                     * device width not specified.
                     */
                     /* FIXME check @offset, @width */
                    /*
                    if (pfl->device_width) {
                        value = extract32(value, 0, pfl->device_width * 8);
                    }
                    else {
                        value = extract32(value, 0, pfl->bank_width * 8);
                    }
                    
                    pfl->counter = value;
                    */
                    Flash.Wcycle++;
                    break;
                case 0x60:
                    if (*(BYTE*)Data == 0xd0) {
                        Flash.Wcycle = 0;
                        Flash.Status |= 0x80;
                    }
                    else if (*(BYTE*)Data == 0x01) {
                        Flash.Wcycle = 0;
                        Flash.Status |= 0x80;
                    }
                    else {
                        Flash.Wcycle = 0;
                        Flash.Cmd = 0;
                    }
                    break;
                case 0x98:
                    if (*(BYTE*)Data == 0xff) { /* Read Array */
                        Flash.Wcycle = 0;
                        Flash.Cmd = 0;
                    }
                    break;
                default:
                    Flash.Wcycle = 0;
                    Flash.Cmd = 0;
                    break;
            }
            break;
        case 2:
            switch (Flash.Cmd) {
                case 0xe8: /* Block write */
                    /* FIXME check @offset, @width */
                    /*
                    if (!pfl->ro) {
                        if (Address + Length >= 0x40000)
                            return;
                        memcpy(Flash.Storage + Address, Data, Length);
                        pflash_data_write(pfl, offset, value, width, be);
                    }
                    else {
                        Flash.Status |= 0x10; 
                    }
                    */
                    if (Address + Length >= FLASH_SIZE)
                        return;
                    memcpy(Flash.Storage + Address, Data, Length);
                    Flash.Status |= 0x80;
                    //pfl->counter--;
                    break;
                default:
                    Flash.Wcycle = 0;
                    Flash.Cmd = 0;
                    break;
            }
            break;
        case 3:
            switch (Flash.Cmd) {
                case 0xe8: /* Block write */
                    if ((*(BYTE*)Data == 0xd0) && !(Flash.Status & 0x10)) {
                        //pflash_blk_write_flush(pfl);
                        Flash.Wcycle = 0;
                        Flash.Status |= 0x80;
                    }
                    else {
                        Flash.Wcycle = 0;
                        Flash.Cmd = 0;
                        break;
                    }
                    break;
                default:
                    Flash.Wcycle = 0;
                    Flash.Cmd = 0;
                    break;
            }

            break;
    }
    /*
    
    */
}

VOID FlashReadMmioHandler(ULONG Address, BYTE* Data, ULONG Length)
{
    Address -= FLASH_MMIO;
    switch (Flash.Cmd) {
        default:
            /* This should never happen : reset state & treat it as a read */
            Flash.Wcycle = 0;
            Flash.Cmd = 0x00;
            /* fall through to read code */
        case 0x00: /* This model reset value for READ_ARRAY (not CFI compliant) */
            /* Flash area read */
            if (Address + Length >= FLASH_SIZE)
                return;
            memcpy(Data, Flash.Storage + Address, Length);
            break;
        case 0x10: /* Single byte program */
        case 0x20: /* Block erase */
        case 0x28: /* Block erase */
        case 0x40: /* single byte program */
        case 0x50: /* Clear status register */
        case 0x60: /* Block /un)lock */
        case 0x70: /* Status Register */
        case 0xe8: /* Write block */
            /*
             * Status register read.  Return status from each device in
             * bank.
             */

            *(BYTE*)Data = Flash.Status;
            break;
        case 0x90:
            
            break;
        case 0x98: /* Query mode */
            *(BYTE*)Data = Flash.CfiTable[Address & 0x51];
            break;
    }
}


VOID FlashInitialize(BYTE *BiosRom)
{
    HANDLE hFile;
    DWORD fileSize;
    BYTE* buf;
    DWORD retSize;
    NTSTATUS status;
    
    MmioRegisterHandler(FLASH_MMIO, FLASH_SIZE - 128 * 1024, FlashWriteMmioHandler, FlashReadMmioHandler);

    hFile = CreateFileA("OVMF.fd", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error in CreateFileA 0x%lx\n", GetLastError());
        return -1;
    }
    fileSize = GetFileSize(hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE) {
        fprintf(stderr, "Error in GetFileSize fileSize > ROM_SIZE \n");
        return -1;
    }
    

    Flash.Storage = malloc(fileSize);
    memset(Flash.Storage, 0xff, fileSize);
    status = ReadFile(hFile, Flash.Storage, fileSize, &retSize, NULL);
    if (!status) {
        fprintf(stderr, "Error in ReadFile 0x%lx\n", GetLastError());
        return -1;
    }
    //copy the last 128k to BIOS rom
    memcpy(BiosRom, Flash.Storage + fileSize - 128 * 1024, 128 * 1024);
    CloseHandle(hFile);

    /* Hardcoded CFI table */
    /* Standard "QRY" string */
    Flash.CfiTable[0x10] = 'Q';
    Flash.CfiTable[0x11] = 'R';
    Flash.CfiTable[0x12] = 'Y';
    /* Command set (Intel) */
    Flash.CfiTable[0x13] = 0x01;
    Flash.CfiTable[0x14] = 0x00;
    /* Primary extended table address (none) */
    Flash.CfiTable[0x15] = 0x31;
    Flash.CfiTable[0x16] = 0x00;
    /* Alternate command set (none) */
    Flash.CfiTable[0x17] = 0x00;
    Flash.CfiTable[0x18] = 0x00;
    /* Alternate extended table (none) */
    Flash.CfiTable[0x19] = 0x00;
    Flash.CfiTable[0x1A] = 0x00;
    /* Vcc min */
    Flash.CfiTable[0x1B] = 0x45;
    /* Vcc max */
    Flash.CfiTable[0x1C] = 0x55;
    /* Vpp min (no Vpp pin) */
    Flash.CfiTable[0x1D] = 0x00;
    /* Vpp max (no Vpp pin) */
    Flash.CfiTable[0x1E] = 0x00;
    /* Reserved */
    Flash.CfiTable[0x1F] = 0x07;
    /* Timeout for min size buffer write */
    Flash.CfiTable[0x20] = 0x07;
    /* Typical timeout for block erase */
    Flash.CfiTable[0x21] = 0x0a;
    /* Typical timeout for full chip erase (4096 ms) */
    Flash.CfiTable[0x22] = 0x00;
    /* Reserved */
    Flash.CfiTable[0x23] = 0x04;
    /* Max timeout for buffer write */
    Flash.CfiTable[0x24] = 0x04;
    /* Max timeout for block erase */
    Flash.CfiTable[0x25] = 0x04;
    /* Max timeout for chip erase */
    Flash.CfiTable[0x26] = 0x00;
    /* Device size */
    Flash.CfiTable[0x27] = 0x22; /* + 1; */
    /* Flash device interface (8 & 16 bits) */
    Flash.CfiTable[0x28] = 0x02;
    Flash.CfiTable[0x29] = 0x00;
    /* Max number of bytes in multi-bytes write */
    Flash.CfiTable[0x2A] = 0x08;
   
    /*
    pfl->writeblock_size = 1 << Flash.CfiTable[0x2A];
    if (!pfl->old_multiple_chip_handling) {
        pfl->writeblock_size *= num_devices;
    }
    */

    Flash.CfiTable[0x2B] = 0x00;
    /* Number of erase block regions (uniform) */
    Flash.CfiTable[0x2C] = 0x01;
    /* Erase block region 1 */
    Flash.CfiTable[0x2D] = 0x400 - 1;
    Flash.CfiTable[0x2E] = (0x400 - 1) >> 8;
    Flash.CfiTable[0x2F] = 0x1000 >> 8;
    Flash.CfiTable[0x30] = 0x1000 >> 16;

    /* Extended */
    Flash.CfiTable[0x31] = 'P';
    Flash.CfiTable[0x32] = 'R';
    Flash.CfiTable[0x33] = 'I';

    Flash.CfiTable[0x34] = '1';
    Flash.CfiTable[0x35] = '0';

    Flash.CfiTable[0x36] = 0x00;
    Flash.CfiTable[0x37] = 0x00;
    Flash.CfiTable[0x38] = 0x00;
    Flash.CfiTable[0x39] = 0x00;

    Flash.CfiTable[0x3a] = 0x00;

    Flash.CfiTable[0x3b] = 0x00;
    Flash.CfiTable[0x3c] = 0x00;

    Flash.CfiTable[0x3f] = 0x01; /* Number of protection fields */


}
