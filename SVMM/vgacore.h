#pragma once
#include <Windows.h>

#define X_TILESIZE 16
#define Y_TILESIZE 24

struct VgaCore {
    struct {
        BOOL color_emulation;     // 1=color emulation, base address = 3Dx
                                  // 0=mono emulation,  base address = 3Bx
        BOOL enable_ram;          // enable CPU access to video memory if set
        BYTE   clock_select;     // 0=25Mhz 1=28Mhz
        BOOL select_high_bank;    // when in odd/even modes, select
                                  // high 64k bank if set
        BOOL horiz_sync_pol;      // bit6: negative if set
        BOOL vert_sync_pol;       // bit7: negative if set
                                  //   bit7,bit6 represent number of lines on display:
                                  //   0 = reserved
                                  //   1 = 400 lines
                                  //   2 = 350 lines
                                  //   3 - 480 lines
    } misc_output;

    struct {
        BYTE   address;
        BYTE   reg[0x19];
        BOOL write_protect;
    } CRTC;

    struct {
        BOOL  flip_flop;  /* 0 = address, 1 = data-write */
        unsigned address; /* register number */
        BOOL  video_enabled;
        BYTE    palette_reg[16];
        BYTE    overscan_color;
        BYTE    color_plane_enable;
        BYTE    horiz_pel_panning;
        BYTE    color_select;
        struct {
            BOOL graphics_alpha;
            BOOL display_type;
            BOOL enable_line_graphics;
            BOOL blink_intensity;
            BOOL pixel_panning_compat;
            BOOL pixel_clock_select;
            BOOL internal_palette_size;
        } mode_ctrl;
    } attribute_ctrl;

    struct {
        BYTE write_data_register;
        BYTE write_data_cycle; /* 0, 1, 2 */
        BYTE read_data_register;
        BYTE read_data_cycle; /* 0, 1, 2 */
        BYTE dac_state;
        struct {
            BYTE red;
            BYTE green;
            BYTE blue;
        } data[256];
        BYTE mask;
    } pel;

    struct {
        BYTE   index;
        BYTE   set_reset;
        BYTE   enable_set_reset;
        BYTE   color_compare;
        BYTE   data_rotate;
        BYTE   raster_op;
        BYTE   read_map_select;
        BYTE   write_mode;
        DWORD  read_mode;
        BOOL odd_even;
        BOOL chain_odd_even;
        BYTE   shift_reg;
        BOOL graphics_alpha;
        BYTE   memory_mapping; /* 0 = use A0000-BFFFF
                                 * 1 = use A0000-AFFFF EGA/VGA graphics modes
                                 * 2 = use B0000-B7FFF Monochrome modes
                                 * 3 = use B8000-BFFFF CGA modes
                                 */
        BYTE   color_dont_care;
        BYTE   bitmask;
        BYTE   latch[4];
    } graphics_ctrl;

    struct {
        BYTE   index;
        BYTE   map_mask;
        BOOL reset1;
        BOOL reset2;
        BYTE   reg1;
        BYTE   char_map_select;
        BOOL extended_mem;
        BOOL odd_even;
        BOOL chain_four;
        BOOL clear_screen;
    } sequencer;

    BOOL  vga_enabled;
    BOOL  vga_mem_updated;
    unsigned line_offset;
    unsigned line_compare;
    unsigned vertical_display_end;
    unsigned blink_counter;
    BOOL* vga_tile_updated;
    BYTE* memory;
    DWORD memsize;
    BYTE text_snapshot[128 * 1024]; // current text snapshot
    BYTE tile[X_TILESIZE * Y_TILESIZE * 4]; /**< Currently allocates the tile as large as needed. */
    WORD charmap_address;
    BOOL x_dotclockdiv2;
    BOOL y_doublescan;
    // h/v retrace timing
    DWORD vclk[4];
    DWORD htotal_usec;
    DWORD hbstart_usec;
    DWORD hbend_usec;
    DWORD vtotal_usec;
    DWORD vblank_usec;
    DWORD vrstart_usec;
    DWORD vrend_usec;
    // shift values for extensions
    BYTE  plane_shift;
    BYTE  dac_shift;
    DWORD ext_offset;
    BOOL   ext_y_dblsize;
    // last active resolution and bpp
    WORD last_xres;
    WORD last_yres;
    BYTE last_bpp;
    BYTE last_fw;
    BYTE last_fh;
    // maximum resolution and number of tiles
    WORD max_xres;
    WORD max_yres;
    WORD num_x_tiles;
    WORD num_y_tiles;
    // vga override mode
    BOOL vga_override;

    int timer_id;
    BOOL update_realtime;
    BOOL vsync_realtime;
    BOOL pci_enabled;
};

typedef struct {
    WORD htotal;
    WORD vtotal;
    WORD vrstart;
} bx_crtc_params_t;

VOID VgaCoreInitialize();
VOID VgaCorePortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length);
ULONG VgaCorePortIoReadHandler(ULONG64 Address, ULONG Length);
VOID VgaCoreUpdate(VOID);
BYTE get_vga_pixel(WORD x, WORD y, WORD saddr, WORD lc, BOOL bs, BYTE** plane);
void determine_screen_dimensions(unsigned* piHeight, unsigned* piWidth);
