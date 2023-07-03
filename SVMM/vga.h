#pragma once
#include <Windows.h>


#define VBE_DISPI_TOTAL_VIDEO_MEMORY_MB  16
#define VBE_DISPI_4BPP_PLANE_SHIFT       22

#define VBE_DISPI_BANK_ADDRESS           0xA0000

#define VBE_DISPI_MAX_XRES               2560
#define VBE_DISPI_MAX_YRES               1600
#define VBE_DISPI_MAX_BPP                32

#define VBE_DISPI_IOPORT_INDEX           0x01CE
#define VBE_DISPI_IOPORT_DATA            0x01CF

#define VBE_DISPI_INDEX_ID               0x0
#define VBE_DISPI_INDEX_XRES             0x1
#define VBE_DISPI_INDEX_YRES             0x2
#define VBE_DISPI_INDEX_BPP              0x3
#define VBE_DISPI_INDEX_ENABLE           0x4
#define VBE_DISPI_INDEX_BANK             0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH       0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT      0x7
#define VBE_DISPI_INDEX_X_OFFSET         0x8
#define VBE_DISPI_INDEX_Y_OFFSET         0x9
#define VBE_DISPI_INDEX_VIDEO_MEMORY_64K 0xa
#define VBE_DISPI_INDEX_DDC              0xb

#define VBE_DISPI_ID0                    0xB0C0
#define VBE_DISPI_ID1                    0xB0C1
#define VBE_DISPI_ID2                    0xB0C2
#define VBE_DISPI_ID3                    0xB0C3
#define VBE_DISPI_ID4                    0xB0C4
#define VBE_DISPI_ID5                    0xB0C5

#define VBE_DISPI_BPP_4                  0x04
#define VBE_DISPI_BPP_8                  0x08
#define VBE_DISPI_BPP_15                 0x0F
#define VBE_DISPI_BPP_16                 0x10
#define VBE_DISPI_BPP_24                 0x18
#define VBE_DISPI_BPP_32                 0x20

#define VBE_DISPI_DISABLED               0x00
#define VBE_DISPI_ENABLED                0x01
#define VBE_DISPI_GETCAPS                0x02
#define VBE_DISPI_BANK_GRANULARITY_32K   0x10
#define VBE_DISPI_8BIT_DAC               0x20
#define VBE_DISPI_LFB_ENABLED            0x40
#define VBE_DISPI_NOCLEARMEM             0x80

#define VBE_DISPI_BANK_WR                0x4000
#define VBE_DISPI_BANK_RD                0x8000
#define VBE_DISPI_BANK_RW                0xc000

#define VBE_DISPI_LFB_PHYSICAL_ADDRESS   0xE0000000

#define VBE_DISPI_TOTAL_VIDEO_MEMORY_KB  (VBE_DISPI_TOTAL_VIDEO_MEMORY_MB * 1024)
#define VBE_DISPI_TOTAL_VIDEO_MEMORY_BYTES (VBE_DISPI_TOTAL_VIDEO_MEMORY_KB * 1024)


enum {
    DDC_STAGE_START,
    DDC_STAGE_ADDRESS,
    DDC_STAGE_RW,
    DDC_STAGE_DATA_IN,
    DDC_STAGE_DATA_OUT,
    DDC_STAGE_ACK_IN,
    DDC_STAGE_ACK_OUT,
    DDC_STAGE_STOP
};

const BYTE vesa_EDID[128] = {
  0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,
  /* 0x0000 8-byte header */
0x04,0x21,                       /* 0x0008 Vendor ID ("AAA") */
0xAB,0xCD,                       /* 0x000A Product ID */
0x00,0x00,0x00,0x00,             /* 0x000C Serial number (none) */
12, 11,                          /* 0x0010 Week of manufacture (12) and year of manufacture (2001) */
0x01, 0x03,                      /* 0x0012 EDID version number (1.3) */
0x0F,                            /* 0x0014 Video signal interface (analogue, 0.700 : 0.300 : 1.000 V p-p,
                                           Video Setup: Blank Level = Black Level, Separate Sync H & V Signals are
                                           supported, Composite Sync Signal on Horizontal is supported, Composite
                                           Sync Signal on Green Video is supported, Serration on the Vertical Sync
                                           is supported) */
0x21,0x19,                       /* 0x0015 Scren size (330 mm * 250 mm) */
0x78,                            /* 0x0017 Display gamma (2.2) */
0x0F,                            /* 0x0018 Feature flags (no DMPS states, RGB, preferred timing mode, display is continuous frequency) */
0x78,0xF5,                       /* 0x0019 Least significant bits for chromaticity and default white point */
0xA6,0x55,0x48,0x9B,0x26,0x12,0x50,0x54,
/* 0x001B Most significant bits for chromaticity and default white point */

0xFF,                            /* 0x0023 Established timings 1 (720 x 400 @ 70Hz, 720 x 400 @ 88Hz,
                                           640 x 480 @ 60Hz, 640 x 480 @ 67Hz, 640 x 480 @ 72Hz, 640 x 480 @ 75Hz,
                                           800 x 600 @ 56Hz, 800 x 600 @ 60Hz) - historical resolutions */
0xEF,                            /* 0x0024 Established timings 2 (800 x 600 @ 72Hz, 800 x 600 @ 75Hz, 832 x 624 @ 75Hz
                                           not 1024 x 768 @ 87Hz(I), 1024 x 768 @ 60Hz, 1024 x 768 @ 70Hz,
                                           1024 x 768 @ 75Hz, 1280 x 1024 @ 75Hz) - historical resolutions */
0x80,                            /* 0x0025 Established timings 2 (1152 x 870 @ 75Hz and no manufacturer timings) */

                                 /* Standard timing */
                                 /* First byte: X resolution, divided by 8, less 31 (256–2288 pixels) */
                                 /* bit 7-6, X:Y pixel ratio: 00=16:10; 01=4:3; 10=5:4; 11=16:9 */
                                 /* bit 5-0, Vertical frequency, less 60 (60–123 Hz), nop 01 01 */
0x31, 0x59,                      /* 0x0026 Standard timing #1 (640 x 480 @ 85 Hz) */
0x45, 0x59,                      /* 0x0028 Standard timing #2 (800 x 600 @ 85 Hz) */
0x61, 0x59,                      /* 0x002A Standard timing #3 (1024 x 768 @ 85 Hz) */
0x81, 0xCA,                      /* 0x002C Standard timing #4 (1280 x 720 @ 70 Hz) */
0x81, 0x0A,                      /* 0x002E Standard timing #5 (1280 x 800 @ 70 Hz) */
0xA9, 0xC0,                      /* 0x0030 Standard timing #6 (1600 x 900 @ 60 Hz) */
0xA9, 0x40,                      /* 0x0034 Standard timing #7 (1600 x 1200 @ 60 Hz) */
0xD1, 0x00,                      /* 0x0032 Standard timing #8 (1920 x 1080 @ 60 Hz) */

                                 /* 0x0036 First 18-byte descriptor (1920 x 1200) */
0x3C, 0x28,                      /*        Pixel clock = 154000000 Hz */
0x80,                            /* 0x0038 Horizontal addressable pixels low byte (0x0780 & 0xFF) */
0xA0,                            /* 0x0039 Horizontal blanking low byte (0x00A0 & 0xFF) */
0x70,                            /* 0x003A Horizontal addressable pixels high 4 bits ((0x0780 & 0x0F00) >> 4), and */
                                 /*        Horizontal blanking high 4 bits ((0x00A0 & 0x0F00 ) >> 8) as low bits */
0xB0,                            /* 0x003B Vertical addressable pixels low byte (0x04B0 & 0xFF) */
0x23,                            /* 0x003C Vertical blanking low byte (0x0023 & 0xFF) */
0x40,                            /* 0x003D Vertical addressable pixels high 4 bits ((0x04B0 & 0x0F00) >> 4), and */
                                 /*        Vertical blanking high 4 bits ((0x0024 & x0F00) >> 8) */
0x30,                            /* 0x003E Horizontal front porch in pixels low byte (0x0030 & 0xFF) */
0x20,                            /* 0x003F Horizontal sync pulse width in pixels low byte (0x0020 & 0xFF) */
0x36,                            /* 0x0040 Vertical front porch in lines low 4 bits ((0x0003 & 0x0F) << 4), and */
                                 /*        Vertical sync pulse width in lines low 4 bits (0x0006 & 0x0F) */
0x00,                            /* 0x0041 Horizontal front porch pixels high 2 bits (0x0030 >> 8), and */
                                 /*        Horizontal sync pulse width in pixels high 2 bits (0x0020 >> 8), and */
                                 /*        Vertical front porch in lines high 2 bits (0x0003 >> 4), and */
                                 /*        Vertical sync pulse width in lines high 2 bits (0x0006 >> 4) */
0x06,                            /* 0x0042 Horizontal addressable video image size in mm low 8 bits (0x0206 & 0xFF) */
0x44,                            /* 0x0043 Vertical addressable video image size in mm low 8 bits (0x0144 & 0xFF) */
0x21,                            /* 0x0044 Horizontal addressable video image size in mm high 8 bits (0x0206 >> 8), and */
                                 /*        Vertical addressable video image size in mm high 8 bits (0x0144 >> 8) */
0x00,                            /* 0x0045 Left and right border size in pixels (0x00) */
0x00,                            /* 0x0046 Top and bottom border size in lines (0x00) */
0x1E,                            /* 0x0047 Flags (non-interlaced, no stereo, analog composite sync, sync on */
                                 /*        all three (RGB) video signals) */


                                 /* 0x0048 Second 18-byte descriptor (1280 x 1024) */
0x30, 0x2a,                      /*        Pixel clock = 108000000 Hz */
0x00,                            /* 0x004A Horizontal addressable pixels low byte (0x0500 & 0xFF) */
0x98,                            /* 0x004B Horizontal blanking low byte (0x0198 & 0xFF) */
0x51,                            /* 0x004C Horizontal addressable pixels high 4 bits (0x0500 >> 8), and */
                                 /*        Horizontal blanking high 4 bits (0x0198 >> 8) */
0x00,                            /* 0x004D Vertical addressable pixels low byte (0x0400 & 0xFF) */
0x2A,                            /* 0x004E Vertical blanking low byte (0x002A & 0xFF) */
0x40,                            /* 0x004F Vertical addressable pixels high 4 bits (0x0400 >> 8), and */
                                 /*        Vertical blanking high 4 bits (0x002A >> 8) */
0x30,                            /* 0x0050 Horizontal front porch in pixels low byte (0x0030 & 0xFF) */
0x70,                            /* 0x0051 Horizontal sync pulse width in pixels low byte (0x0070 & 0xFF) */
0x13,                            /* 0x0052 Vertical front porch in lines low 4 bits (0x0001 & 0x0F), and */
                                 /*        Vertical sync pulse width in lines low 4 bits (0x0003 & 0x0F) */
0x00,                            /* 0x0053 Horizontal front porch pixels high 2 bits (0x0030 >> 8), and */
                                 /*        Horizontal sync pulse width in pixels high 2 bits (0x0070 >> 8), and */
                                 /*        Vertical front porch in lines high 2 bits (0x0001 >> 4), and */
                                 /*        Vertical sync pulse width in lines high 2 bits (0x0003 >> 4) */
0x2C,                            /* 0x0054 Horizontal addressable video image size in mm low 8 bits (0x012C & 0xFF) */
0xE1,                            /* 0x0055 Vertical addressable video image size in mm low 8 bits (0x00E1 & 0xFF) */
0x10,                            /* 0x0056 Horizontal addressable video image size in mm high 8 bits (0x012C >> 8), and */
                                 /*        Vertical addressable video image size in mm high 8 bits (0x00E1 >> 8) */
0x00,                            /* 0x0057 Left and right border size in pixels (0x00) */
0x00,                            /* 0x0058 Top and bottom border size in lines (0x00) */
0x1E,                            /* 0x0059 Flags (non-interlaced, no stereo, analog composite sync, sync on */
                                 /*        all three (RGB) video signals) */


0x00,0x00,0x00,0xFF,0x00,        /* 0x005A Third 18-byte descriptor - display product serial number */
'0','1','2','3','4','5','6','7','8','9',
0x0A,0x20,0x20,

0x00,0x00,0x00,0xFC,0x00,        /* 0x006C Fourth 18-byte descriptor - display product name  */
'B','o','c','h','s',' ','S','c','r','e','e','n',
0x0A,

0x00,                            /* 0x007E Extension block count (none)  */
0x00,                            /* 0x007F Checksum (set by constructor) */
};

enum {
    BX_DDC_MODE_DISABLED,
    BX_DDC_MODE_BUILTIN,
    BX_DDC_MODE_FILE
};


struct bx_ddc_c {
    BYTE ddc_mode;
    BOOL  DCKhost;
    BOOL  DDAhost;
    BOOL  DDAmon;
    BYTE ddc_stage;
    BYTE ddc_bitshift;
    BOOL  ddc_ack;
    BOOL  ddc_rw;
    BYTE ddc_byte;
    BYTE edid_index;
    BOOL  edid_extblock;
    BYTE edid_data[256];
};  // state information

struct VGA {
    BOOL vbe_present;
    struct {
        WORD  cur_dispi;
        DWORD  base_address;
        WORD  xres;
        WORD  yres;
        WORD  bpp;
        WORD  max_xres;
        WORD  max_yres;
        WORD  max_bpp;
        WORD  bank[2];
        WORD  bank_granularity_kb;
        BOOL    enabled;
        WORD  curindex;
        DWORD  visible_screen_size; /**< in bytes */
        WORD  offset_x;            /**< Virtual screen x start (in pixels) */
        WORD  offset_y;            /**< Virtual screen y start (in pixels) */
        WORD  virtual_xres;
        WORD  virtual_yres;
        DWORD  virtual_start;   /**< For dealing with bpp>8, this is where the virtual screen starts. */
        BYTE   bpp_multiplier;  /**< We have to save this b/c sometimes we need to recalculate stuff with it. */
        BOOL    get_capabilities;
        BOOL    dac_8bit;
        BOOL    ddc_enabled;
    } vbe;  // VBE state information

    struct bx_ddc_c ddc;
};
VOID VgaInitialize();
VOID VgaPortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length);
