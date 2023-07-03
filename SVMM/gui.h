#pragma once
#include <Windows.h>

#define BX_KEY_PRESSED  0x00000000
#define BX_KEY_RELEASED 0x80000000

#define BX_KEY_UNHANDLED 0x10000000

// header bar and status bar stuff
#define BX_HEADER_BAR_Y 32

#define BX_MAX_PIXMAPS 17
#define BX_MAX_HEADERBAR_ENTRIES 12

// align pixmaps towards left or right side of header bar
#define BX_GRAVITY_LEFT 10
#define BX_GRAVITY_RIGHT 11

#define BX_MAX_STATUSITEMS 10

// gui dialog capabilities
#define BX_GUI_DLG_FLOPPY       0x01
#define BX_GUI_DLG_CDROM        0x02
#define BX_GUI_DLG_SNAPSHOT     0x04
#define BX_GUI_DLG_RUNTIME      0x08
#define BX_GUI_DLG_USER         0x10
#define BX_GUI_DLG_SAVE_RESTORE 0x20
#define BX_GUI_DLG_ALL          0x3F

// text mode blink feature
#define BX_TEXT_BLINK_MODE      0x01
#define BX_TEXT_BLINK_TOGGLE    0x02
#define BX_TEXT_BLINK_STATE     0x04

// modifier keys
#define BX_MOD_KEY_SHIFT        0x01
#define BX_MOD_KEY_CTRL         0x02
#define BX_MOD_KEY_ALT          0x04
#define BX_MOD_KEY_CAPS         0x08

// mouse capture toggle feature
#define BX_MT_KEY_CTRL          0x01
#define BX_MT_KEY_ALT           0x02
#define BX_MT_KEY_F10           0x04
#define BX_MT_KEY_F12           0x08
#define BX_MT_MBUTTON           0x10
#define BX_MT_LBUTTON           0x20
#define BX_MT_RBUTTON           0x40

#define BX_GUI_MT_CTRL_MB       (BX_MT_KEY_CTRL | BX_MT_MBUTTON)
#define BX_GUI_MT_CTRL_LRB      (BX_MT_KEY_CTRL | BX_MT_LBUTTON | BX_MT_RBUTTON)
#define BX_GUI_MT_CTRL_F10      (BX_MT_KEY_CTRL | BX_MT_KEY_F10)
#define BX_GUI_MT_F12           (BX_MT_KEY_F12)
#define BX_GUI_MT_CTRL_ALT      (BX_MT_KEY_CTRL | BX_MT_KEY_ALT)

typedef struct {
    WORD  start_address;
    BYTE   cs_start;
    BYTE   cs_end;
    WORD  line_offset;
    WORD  line_compare;
    BYTE   h_panning;
    BYTE   v_panning;
    BOOL line_graphics;
    BOOL split_hpanning;
    BYTE   blink_flags;
    BYTE   actl_palette[16];
} bx_vga_tminfo_t;

typedef struct {
	WORD bpp, pitch;
	BYTE red_shift, green_shift, blue_shift;
	BYTE is_indexed, is_little_endian;
	unsigned long red_mask, green_mask, blue_mask;
    BOOL snapshot_mode;
} bx_svga_tileinfo_t;

// command mode
struct command_mode {
    BOOL present;
    BOOL active;
};

enum {
    BX_KEY_CTRL_L,
    BX_KEY_SHIFT_L,

    BX_KEY_F1,
    BX_KEY_F2,
    BX_KEY_F3,
    BX_KEY_F4,
    BX_KEY_F5,
    BX_KEY_F6,
    BX_KEY_F7,
    BX_KEY_F8,
    BX_KEY_F9,
    BX_KEY_F10,
    BX_KEY_F11,
    BX_KEY_F12,

    BX_KEY_CTRL_R,
    BX_KEY_SHIFT_R,
    BX_KEY_CAPS_LOCK,
    BX_KEY_NUM_LOCK,
    BX_KEY_ALT_L,
    BX_KEY_ALT_R,

    BX_KEY_A,
    BX_KEY_B,
    BX_KEY_C,
    BX_KEY_D,
    BX_KEY_E,
    BX_KEY_F,
    BX_KEY_G,
    BX_KEY_H,
    BX_KEY_I,
    BX_KEY_J,
    BX_KEY_K,
    BX_KEY_L,
    BX_KEY_M,
    BX_KEY_N,
    BX_KEY_O,
    BX_KEY_P,
    BX_KEY_Q,
    BX_KEY_R,
    BX_KEY_S,
    BX_KEY_T,
    BX_KEY_U,
    BX_KEY_V,
    BX_KEY_W,
    BX_KEY_X,
    BX_KEY_Y,
    BX_KEY_Z,

    BX_KEY_0,
    BX_KEY_1,
    BX_KEY_2,
    BX_KEY_3,
    BX_KEY_4,
    BX_KEY_5,
    BX_KEY_6,
    BX_KEY_7,
    BX_KEY_8,
    BX_KEY_9,

    BX_KEY_ESC,

    BX_KEY_SPACE,
    BX_KEY_SINGLE_QUOTE,
    BX_KEY_COMMA,
    BX_KEY_PERIOD,
    BX_KEY_SLASH,

    BX_KEY_SEMICOLON,
    BX_KEY_EQUALS,

    BX_KEY_LEFT_BRACKET,
    BX_KEY_BACKSLASH,
    BX_KEY_RIGHT_BRACKET,
    BX_KEY_MINUS,
    BX_KEY_GRAVE,

    BX_KEY_BACKSPACE,
    BX_KEY_ENTER,
    BX_KEY_TAB,

    BX_KEY_LEFT_BACKSLASH,
    BX_KEY_PRINT,
    BX_KEY_SCRL_LOCK,
    BX_KEY_PAUSE,

    BX_KEY_INSERT,
    BX_KEY_DELETE,
    BX_KEY_HOME,
    BX_KEY_END,
    BX_KEY_PAGE_UP,
    BX_KEY_PAGE_DOWN,

    BX_KEY_KP_ADD,
    BX_KEY_KP_SUBTRACT,
    BX_KEY_KP_END,
    BX_KEY_KP_DOWN,
    BX_KEY_KP_PAGE_DOWN,
    BX_KEY_KP_LEFT,
    BX_KEY_KP_RIGHT,
    BX_KEY_KP_HOME,
    BX_KEY_KP_UP,
    BX_KEY_KP_PAGE_UP,
    BX_KEY_KP_INSERT,
    BX_KEY_KP_DELETE,
    BX_KEY_KP_5,

    BX_KEY_UP,
    BX_KEY_DOWN,
    BX_KEY_LEFT,
    BX_KEY_RIGHT,

    BX_KEY_KP_ENTER,
    BX_KEY_KP_MULTIPLY,
    BX_KEY_KP_DIVIDE,

    BX_KEY_WIN_L,
    BX_KEY_WIN_R,
    BX_KEY_MENU,

    BX_KEY_ALT_SYSREQ,
    BX_KEY_CTRL_BREAK,

    BX_KEY_INT_BACK,
    BX_KEY_INT_FORWARD,
    BX_KEY_INT_STOP,
    BX_KEY_INT_MAIL,
    BX_KEY_INT_SEARCH,
    BX_KEY_INT_FAV,
    BX_KEY_INT_HOME,

    BX_KEY_POWER_MYCOMP,
    BX_KEY_POWER_CALC,
    BX_KEY_POWER_SLEEP,
    BX_KEY_POWER_POWER,
    BX_KEY_POWER_WAKE,

    BX_KEY_NBKEYS
};

#define BX_KEY_UNKNOWN 0x7fffffff
#define N_USER_KEYS 38

typedef struct {
    const char* key;
    DWORD symbol;
} user_key_t;

enum {
    BX_MOUSE_TOGGLE_CTRL_MB,
    BX_MOUSE_TOGGLE_CTRL_F10,
    BX_MOUSE_TOGGLE_CTRL_ALT,
    BX_MOUSE_TOGGLE_F12
};
BOOL command_mode_active(void);
BOOL mouse_toggle_check(DWORD key, BOOL pressed);
BYTE set_modifier_keys(BYTE modifier, BOOL pressed);
BYTE get_modifier_keys(void);
void marklog_handler(void);
BOOL has_command_mode(void);
BYTE get_mouse_headerbar_id();
BYTE reverse_bitorder(BYTE b);
void set_command_mode(BOOL active);
void GuiInitialize(int argc, char** argv, unsigned arg_max_xres, unsigned arg_max_yres,
    unsigned tilewidth, unsigned tileheight);

void graphics_tile_update_common(BYTE* tile, unsigned x, unsigned y);
void set_text_charbyte(WORD address, BYTE data);
void text_update_common(BYTE* old_text, BYTE* new_text,
    WORD cursor_address,
    bx_vga_tminfo_t* arg_tm_info);
bx_svga_tileinfo_t* graphics_tile_info_common(bx_svga_tileinfo_t* info);

BYTE* get_snapshot_buffer(void);
BYTE* graphics_tile_get(unsigned x0, unsigned y0,
    unsigned* w, unsigned* h);
void graphics_tile_update_in_place(unsigned x0, unsigned y0,
    unsigned w, unsigned h);
BOOL palette_change_common(BYTE index, BYTE red, BYTE green, BYTE blue);

