#pragma once
#include <Windows.h>
#include "gui.h"

#define EXIT_GUI_SHUTDOWN        1
#define EXIT_GMH_FAILURE         2
#define EXIT_FONT_BITMAP_ERROR   3
#define EXIT_NORMAL              4
#define EXIT_HEADER_BITMAP_ERROR 5

#define BX_SB_MAX_TEXT_ELEMENTS    2
#define SIZE_OF_SB_ELEMENT        40
#define SIZE_OF_SB_MOUSE_MESSAGE 170
#define SIZE_OF_SB_IPS_MESSAGE    90

#define COMMAND_MODE_VKEY VK_F7


// Keyboard/mouse stuff
#define SCANCODE_BUFSIZE    20
#define MOUSE_PRESSED       0x20000000
#define TOOLBAR_CLICKED     0x08000000
#define MOUSE_MOTION        0x22000000
#define FOCUS_CHANGED       0x44000000
#define BX_SYSKEY           (KF_UP|KF_REPEAT|KF_ALTDOWN)

void enq_key_event(DWORD, DWORD);
void enq_mouse_event(void);

struct QueueEvent {
	DWORD key_event;
	int mouse_x;
	int mouse_y;
	int mouse_z;
	int mouse_button_state;
};

struct bx_bitmaps {
	HBITMAP bmap;
	unsigned xdim;
	unsigned ydim;
};


struct win32_toolbar {
	unsigned bmap_id;
	void (*f)(void);
	const char* tooltip;
};


typedef struct {
	HINSTANCE hInstance;

	CRITICAL_SECTION drawCS;
	CRITICAL_SECTION keyCS;
	CRITICAL_SECTION mouseCS;

	int kill;  // reason for terminateEmul(int)
	BOOL UIinited;
	HWND mainWnd;
	HWND simWnd;
} sharedThreadInfo;



struct bx_headerbar {
    unsigned bmap_id;
    unsigned xdim;
    unsigned ydim;
    unsigned xorigin;
    unsigned alignment;
    void (*f)(void);
};


struct statusitem {
    BOOL in_use;
    char text[8];
    BOOL active;
    BOOL mode; // read/write
    BOOL auto_off;
    BYTE counter;
};


struct palette {
    BYTE blue;
    BYTE green;
    BYTE red;
    BYTE reserved;
};



#if BX_USE_GUI_CONSOLE
struct {
    bool present;
    bool running;
    Bit8u* screen;
    Bit8u* oldscreen;
    Bit8u saved_fwidth;
    Bit8u saved_fheight;
    Bit16u saved_xres;
    Bit16u saved_yres;
    Bit8u  saved_bpp;
    Bit8u saved_palette[32];
    Bit8u saved_charmap[0x2000];
    unsigned cursor_x;
    unsigned cursor_y;
    Bit16u cursor_addr;
    bx_vga_tminfo_t tminfo;
    Bit8u keys[16];
    Bit8u n_keys;
} console;
#endif
// command mode



VOID Win32GuiSpecificInit(int argc, char** argv, unsigned headerbar_y);
unsigned create_bitmap(const unsigned char* bmap, unsigned xdim, unsigned ydim);
void show_headerbar(void);
void draw_char(BYTE ch, BYTE fc, BYTE bc, WORD xc, WORD yc,
    BYTE fw, BYTE fh, BYTE fx, BYTE fy,
    BOOL gfxcharw9, BYTE cs, BYTE ce, BOOL curs);
void set_font(BOOL lg);
void text_update(BYTE* old_text, BYTE* new_text,
    unsigned long cursor_x, unsigned long cursor_y,
    bx_vga_tminfo_t* tm_info);
void graphics_tile_update(BYTE* tile, unsigned x0, unsigned y0);
BOOL palette_change(BYTE index, BYTE red, BYTE green,
    BYTE blue);
void statusbar_setitem_specific(int element, BOOL active, BOOL w);
void mouse_enabled_changed_specific(BOOL val);
void replace_bitmap(unsigned hbar_id, unsigned bmap_id);

struct QueueEvent* deq_key_event(void);
LRESULT CALLBACK mainWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK simWndProc(HWND, UINT, WPARAM, LPARAM);
DWORD WINAPI UIThread(PVOID);
void SetStatusText(unsigned Num, const char* Text, BOOL active, BYTE color);
void terminateEmul(int);
void create_vga_font(void);
void DrawBitmap(HDC, HBITMAP, int, int, int, int, int, int, BYTE, BYTE);
void updateUpdated(int, int, int, int);
static void win32_toolbar_click(int x);
void set_tooltip(unsigned hbar_id, const char* tip);
void flush(void);
void dimension_update(unsigned x, unsigned y, unsigned fheight, unsigned fwidth, unsigned bpp);
void clear_screen(void);
