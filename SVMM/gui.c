#include "gui.h"
#include "win32gui.h"

#define BX_FLOPPYB_BMAP_X 32
#define BX_FLOPPYB_BMAP_Y 32

#define BX_FLOPPYA_BMAP_X 32
#define BX_FLOPPYA_BMAP_Y 32

static const unsigned char bx_floppya_bmap[(BX_FLOPPYA_BMAP_X * BX_FLOPPYA_BMAP_Y) / 8] = {
   0x00, 0x80, 0x00, 0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0x40, 0x11, 0x00,
   0x00, 0xc0, 0x01, 0x00, 0x00, 0x60, 0x13, 0x00, 0x00, 0x60, 0x03, 0x00,
   0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0x07, 0xe0, 0x01, 0x80, 0x07,
   0x20, 0xfd, 0xbf, 0x04, 0x20, 0x01, 0x80, 0x04, 0xe0, 0xfd, 0xbf, 0x07,
   0xe0, 0x01, 0x80, 0x07, 0xe0, 0xfd, 0xbf, 0x07, 0xe0, 0x01, 0x80, 0x07,
   0xe0, 0xfd, 0xbf, 0x07, 0xe0, 0x01, 0x80, 0x07, 0xe0, 0xfd, 0xbf, 0x07,
   0xe0, 0x01, 0x80, 0x07, 0xe0, 0xfd, 0xbf, 0x07, 0xe0, 0x01, 0x80, 0x07,
   0xe0, 0xff, 0xff, 0x07, 0xe0, 0xff, 0xff, 0x07, 0xe0, 0xff, 0xff, 0x07,
   0xe0, 0xff, 0xff, 0x07, 0xe0, 0xaf, 0xea, 0x07, 0xe0, 0xf7, 0xd5, 0x07,
   0xe0, 0xef, 0xea, 0x07, 0xe0, 0xf7, 0xd5, 0x07, 0xc0, 0xef, 0xea, 0x07,
   0x80, 0x57, 0xd5, 0x07, 0x00, 0xaf, 0xea, 0x07
};

static const unsigned char bx_floppya_eject_bmap[(BX_FLOPPYA_BMAP_X * BX_FLOPPYA_BMAP_Y) / 8] = {
   0x01, 0x80, 0x00, 0x80, 0x02, 0x40, 0x01, 0x40, 0x04, 0x40, 0x11, 0x20,
   0x08, 0xc0, 0x01, 0x10, 0x10, 0x60, 0x13, 0x08, 0x20, 0x60, 0x03, 0x04,
   0x40, 0x00, 0x00, 0x02, 0xe0, 0xff, 0xff, 0x07, 0xe0, 0x01, 0x80, 0x07,
   0x20, 0xff, 0xff, 0x04, 0x20, 0x05, 0xa0, 0x04, 0xe0, 0xfd, 0xbf, 0x07,
   0xe0, 0x11, 0x88, 0x07, 0xe0, 0xfd, 0xbf, 0x07, 0xe0, 0x41, 0x82, 0x07,
   0xe0, 0xfd, 0xbf, 0x07, 0xe0, 0x81, 0x81, 0x07, 0xe0, 0xfd, 0xbf, 0x07,
   0xe0, 0x21, 0x84, 0x07, 0xe0, 0xfd, 0xbf, 0x07, 0xe0, 0x09, 0x90, 0x07,
   0xe0, 0xff, 0xff, 0x07, 0xe0, 0xff, 0xff, 0x07, 0xe0, 0xff, 0xff, 0x07,
   0xe0, 0xff, 0xff, 0x07, 0xe0, 0xaf, 0xea, 0x07, 0xe0, 0xf7, 0xd5, 0x07,
   0xf0, 0xef, 0xea, 0x0f, 0xe8, 0xf7, 0xd5, 0x17, 0xc4, 0xef, 0xea, 0x27,
   0x82, 0x57, 0xd5, 0x47, 0x01, 0xaf, 0xea, 0x87
};
static const unsigned char bx_floppyb_bmap[(BX_FLOPPYB_BMAP_X * BX_FLOPPYB_BMAP_Y) / 8] = {
   0x00, 0xe0, 0x00, 0x00, 0x00, 0x20, 0x01, 0x00, 0x00, 0xe0, 0x08, 0x00,
   0x00, 0x20, 0x01, 0x00, 0x00, 0x20, 0x09, 0x00, 0x00, 0xe0, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0x07, 0xe0, 0x01, 0x80, 0x07,
   0x20, 0xfd, 0xbf, 0x04, 0x20, 0x01, 0x80, 0x04, 0xe0, 0xfd, 0xbf, 0x07,
   0xe0, 0x01, 0x80, 0x07, 0xe0, 0xfd, 0xbf, 0x07, 0xe0, 0x01, 0x80, 0x07,
   0xe0, 0xfd, 0xbf, 0x07, 0xe0, 0x01, 0x80, 0x07, 0xe0, 0xfd, 0xbf, 0x07,
   0xe0, 0x01, 0x80, 0x07, 0xe0, 0xfd, 0xbf, 0x07, 0xe0, 0x01, 0x80, 0x07,
   0xe0, 0xff, 0xff, 0x07, 0xe0, 0xff, 0xff, 0x07, 0xe0, 0xff, 0xff, 0x07,
   0xe0, 0xff, 0xff, 0x07, 0xe0, 0xaf, 0xea, 0x07, 0xe0, 0xf7, 0xd5, 0x07,
   0xe0, 0xef, 0xea, 0x07, 0xe0, 0xf7, 0xd5, 0x07, 0xc0, 0xef, 0xea, 0x07,
   0x80, 0x57, 0xd5, 0x07, 0x00, 0xaf, 0xea, 0x07
};

static const unsigned char bx_floppyb_eject_bmap[(BX_FLOPPYB_BMAP_X * BX_FLOPPYB_BMAP_Y) / 8] = {
   0x01, 0xe0, 0x00, 0x80, 0x02, 0x20, 0x01, 0x40, 0x04, 0xe0, 0x08, 0x20,
   0x08, 0x20, 0x01, 0x10, 0x10, 0x20, 0x09, 0x08, 0x20, 0xe0, 0x00, 0x04,
   0x40, 0x00, 0x00, 0x02, 0xe0, 0xff, 0xff, 0x07, 0xe0, 0x01, 0x80, 0x07,
   0x20, 0xff, 0xff, 0x04, 0x20, 0x05, 0xa0, 0x04, 0xe0, 0xfd, 0xbf, 0x07,
   0xe0, 0x11, 0x88, 0x07, 0xe0, 0xfd, 0xbf, 0x07, 0xe0, 0x41, 0x82, 0x07,
   0xe0, 0xfd, 0xbf, 0x07, 0xe0, 0x81, 0x81, 0x07, 0xe0, 0xfd, 0xbf, 0x07,
   0xe0, 0x21, 0x84, 0x07, 0xe0, 0xfd, 0xbf, 0x07, 0xe0, 0x09, 0x90, 0x07,
   0xe0, 0xff, 0xff, 0x07, 0xe0, 0xff, 0xff, 0x07, 0xe0, 0xff, 0xff, 0x07,
   0xe0, 0xff, 0xff, 0x07, 0xe0, 0xaf, 0xea, 0x07, 0xe0, 0xf7, 0xd5, 0x07,
   0xf0, 0xef, 0xea, 0x0f, 0xe8, 0xf7, 0xd5, 0x17, 0xc4, 0xef, 0xea, 0x27,
   0x82, 0x57, 0xd5, 0x47, 0x01, 0xaf, 0xea, 0x87
};

#define BX_RESET_BMAP_X 32
#define BX_RESET_BMAP_Y 32

static const unsigned char bx_reset_bmap[(BX_RESET_BMAP_X * BX_RESET_BMAP_Y) / 8] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x3c, 0x00, 0x00, 0x10,
  0x48, 0x0c, 0xc7, 0x7c, 0x48, 0x92, 0x20, 0x11, 0x34, 0x1f, 0xf3, 0x09,
  0x24, 0x41, 0x12, 0x48, 0x6e, 0xce, 0xe1, 0x30, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00,
  0x00, 0xe0, 0x0f, 0x00, 0x00, 0xf0, 0x1f, 0x00, 0x00, 0xe0, 0x3f, 0x00,
  0x00, 0xc7, 0x38, 0x00, 0x00, 0x87, 0x38, 0x00, 0x00, 0x07, 0x38, 0x00,
  0x00, 0x07, 0x38, 0x00, 0x00, 0x07, 0x38, 0x00, 0x00, 0x07, 0x38, 0x00,
  0x00, 0x07, 0x38, 0x00, 0x00, 0xff, 0x3f, 0x00, 0x00, 0xfe, 0x1f, 0x00,
  0x00, 0xfc, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


#define BX_POWER_BMAP_X 32
#define BX_POWER_BMAP_Y 32

static const unsigned char bx_power_bmap[(BX_POWER_BMAP_X * BX_POWER_BMAP_Y) / 8] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00,
  0x24, 0x67, 0x66, 0x34, 0xa4, 0x28, 0x92, 0x48, 0x9a, 0xa8, 0xfa, 0x04,
  0x82, 0x64, 0x09, 0x04, 0x07, 0xa3, 0x70, 0x0e, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xf8, 0x03, 0x00, 0x00, 0xfe, 0x0f, 0x00, 0x00, 0x0f, 0x1e, 0x00,
  0x80, 0x03, 0x38, 0x00, 0xc0, 0x00, 0x60, 0x00, 0xe0, 0xe0, 0xe0, 0x00,
  0x60, 0xe0, 0xc0, 0x00, 0x70, 0xe0, 0xc0, 0x01, 0x30, 0xe0, 0x80, 0x01,
  0x30, 0xe0, 0x80, 0x01, 0x30, 0xe0, 0x80, 0x01, 0x30, 0xe0, 0x80, 0x01,
  0x30, 0xe0, 0x80, 0x01, 0x70, 0xe0, 0xc0, 0x01, 0x60, 0xe0, 0xc0, 0x00,
  0xe0, 0xe0, 0xe0, 0x00, 0xc0, 0x00, 0x60, 0x00, 0x80, 0x03, 0x38, 0x00,
  0x00, 0x0f, 0x1e, 0x00, 0x00, 0xfe, 0x0f, 0x00, 0x00, 0xf8, 0x03, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


#define BX_CONFIG_BMAP_X 32
#define BX_CONFIG_BMAP_Y 32

static const unsigned char bx_config_bmap[(BX_CONFIG_BMAP_X * BX_CONFIG_BMAP_Y) / 8] = {
  0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x80, 0x01, 0xfc, 0x7f, 0xc0, 0x03,
  0xfc, 0xff, 0xc1, 0x03, 0xfc, 0xff, 0xc1, 0x03, 0xfc, 0x7f, 0xc0, 0x03,
  0x84, 0x07, 0xc0, 0x03, 0x80, 0x07, 0xc0, 0x03, 0x80, 0x07, 0xc0, 0x03,
  0x80, 0x07, 0xc0, 0x03, 0x80, 0x07, 0xc0, 0x03, 0x80, 0x07, 0xc0, 0x03,
  0x80, 0x07, 0xc0, 0x03, 0x80, 0x07, 0xc0, 0x03, 0x80, 0x07, 0xc0, 0x03,
  0x80, 0x07, 0xc0, 0x03, 0x80, 0x07, 0xc0, 0x03, 0x80, 0x07, 0xc0, 0x03,
  0x80, 0x07, 0xc0, 0x03, 0x80, 0x07, 0xe0, 0x07, 0x80, 0x07, 0xf0, 0x0f,
  0x80, 0x07, 0x70, 0x0e, 0x80, 0x07, 0x30, 0x0c, 0x80, 0x07, 0x30, 0x0c,
  0x80, 0x07, 0x20, 0x04, 0x00, 0x00, 0x00, 0x00, 0x38, 0x27, 0xba, 0x1c,
  0x84, 0x68, 0x8a, 0x02, 0x84, 0xa8, 0xba, 0x32, 0x84, 0x28, 0x8b, 0x22,
  0x38, 0x27, 0x8a, 0x1c, 0x00, 0x00, 0x00, 0x00
};



#define BX_MOUSE_BMAP_X 32
#define BX_MOUSE_BMAP_Y 32

static unsigned char bx_mouse_bmap[(BX_MOUSE_BMAP_X * BX_MOUSE_BMAP_Y) / 8] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x07, 0x00, 0x00, 0xf8, 0x1f, 0x00,
   0x00, 0x0c, 0x30, 0x00, 0x00, 0x06, 0x60, 0x00, 0x00, 0x06, 0x40, 0x00,
   0xf8, 0xff, 0xc0, 0x00, 0x0c, 0x80, 0xc1, 0x00, 0x24, 0x22, 0xc1, 0x00,
   0x74, 0x77, 0x81, 0x00, 0x74, 0x77, 0x81, 0x01, 0x74, 0x77, 0x81, 0x01,
   0x74, 0x77, 0x01, 0x01, 0x74, 0x77, 0x01, 0x03, 0x74, 0x77, 0x01, 0x06,
   0x24, 0x22, 0x01, 0x0c, 0x0c, 0x80, 0x01, 0x38, 0x04, 0x00, 0x01, 0x00,
   0x0c, 0x80, 0x01, 0x00, 0x04, 0x00, 0x01, 0x00, 0x0c, 0x80, 0x01, 0x00,
   0x04, 0x00, 0x01, 0x00, 0x0c, 0x80, 0x01, 0x00, 0x04, 0x00, 0x01, 0x00,
   0x0c, 0x80, 0x01, 0x00, 0x14, 0x40, 0x01, 0x00, 0x2c, 0xa0, 0x01, 0x00,
   0x54, 0x55, 0x01, 0x00, 0xac, 0xaa, 0x01, 0x00, 0xf8, 0xff, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static unsigned char bx_nomouse_bmap[(BX_MOUSE_BMAP_X * BX_MOUSE_BMAP_Y) / 8] = {
   0x01, 0x00, 0x00, 0x80, 0x02, 0xe0, 0x07, 0x40, 0x04, 0xf8, 0x1f, 0x20,
   0x08, 0x0c, 0x30, 0x10, 0x10, 0x06, 0x60, 0x08, 0x20, 0x06, 0x40, 0x04,
   0xf8, 0xff, 0xc0, 0x02, 0x8c, 0x80, 0xc1, 0x01, 0x24, 0x23, 0xc1, 0x00,
   0x74, 0x77, 0xc1, 0x00, 0x74, 0x77, 0xa1, 0x01, 0x74, 0x7f, 0x91, 0x01,
   0x74, 0x77, 0x09, 0x01, 0x74, 0x77, 0x05, 0x03, 0x74, 0x77, 0x03, 0x06,
   0x24, 0xa2, 0x01, 0x0c, 0x0c, 0x80, 0x01, 0x38, 0x04, 0x40, 0x03, 0x00,
   0x0c, 0xa0, 0x05, 0x00, 0x04, 0x10, 0x09, 0x00, 0x0c, 0x88, 0x11, 0x00,
   0x04, 0x04, 0x21, 0x00, 0x0c, 0x82, 0x41, 0x00, 0x04, 0x01, 0x81, 0x00,
   0x8c, 0x80, 0x01, 0x01, 0x54, 0x40, 0x01, 0x02, 0x2c, 0xa0, 0x01, 0x04,
   0x54, 0x55, 0x01, 0x08, 0xac, 0xaa, 0x01, 0x10, 0xfc, 0xff, 0x00, 0x20,
   0x02, 0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0x80
};


#define BX_CDROMD_BMAP_X 32
#define BX_CDROMD_BMAP_Y 32

static const unsigned char bx_cdromd_bmap[(BX_CONFIG_BMAP_X * BX_CONFIG_BMAP_Y) / 8] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x0e, 0x00, 0x00, 0x10, 0x12, 0x00,
  0x00, 0x10, 0x12, 0x00, 0x00, 0x10, 0x12, 0x00, 0x00, 0x60, 0x0e, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x0c, 0x30, 0x00,
  0x00, 0xe2, 0x47, 0x00, 0x00, 0x19, 0x98, 0x00, 0x80, 0xe6, 0x67, 0x01,
  0x40, 0x19, 0x98, 0x02, 0x20, 0xe5, 0xa7, 0x04, 0xa0, 0x12, 0x48, 0x05,
  0x90, 0xca, 0x53, 0x09, 0x50, 0x25, 0xa4, 0x0a, 0x50, 0x15, 0xa8, 0x0a,
  0x50, 0x15, 0xa8, 0x0a, 0x50, 0x15, 0xa8, 0x0a, 0x50, 0x15, 0xa8, 0x0a,
  0x50, 0x25, 0xa4, 0x0a, 0x90, 0xca, 0x53, 0x09, 0xa0, 0x12, 0x48, 0x05,
  0x20, 0xe5, 0xa7, 0x04, 0x40, 0x19, 0x98, 0x02, 0x80, 0xe6, 0x67, 0x01,
  0x00, 0x19, 0x98, 0x00, 0x00, 0xe2, 0x47, 0x00, 0x00, 0x0c, 0x30, 0x00,
  0x00, 0xf0, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const unsigned char bx_cdromd_eject_bmap[(BX_CONFIG_BMAP_X * BX_CONFIG_BMAP_Y) / 8] = {
  0x01, 0x00, 0x00, 0x80, 0x02, 0x60, 0x0e, 0x40, 0x04, 0x10, 0x12, 0x20,
  0x08, 0x10, 0x12, 0x10, 0x10, 0x10, 0x12, 0x08, 0x20, 0x60, 0x0e, 0x04,
  0x40, 0x00, 0x00, 0x02, 0x80, 0xf0, 0x0f, 0x01, 0x00, 0x0d, 0xb0, 0x00,
  0x00, 0xe2, 0x47, 0x00, 0x00, 0x1d, 0xb8, 0x00, 0x80, 0xee, 0x77, 0x01,
  0x40, 0x19, 0x98, 0x02, 0x20, 0xe5, 0xa7, 0x04, 0xa0, 0x52, 0x4a, 0x05,
  0x90, 0xca, 0x53, 0x09, 0x50, 0xa5, 0xa5, 0x0a, 0x50, 0x55, 0xaa, 0x0a,
  0x50, 0x35, 0xac, 0x0a, 0x50, 0x15, 0xa8, 0x0a, 0x50, 0x1d, 0xb8, 0x0a,
  0x50, 0x25, 0xa4, 0x0a, 0x90, 0xca, 0x53, 0x09, 0xa0, 0x13, 0xc8, 0x05,
  0xa0, 0xe5, 0xa7, 0x05, 0x40, 0x19, 0x98, 0x02, 0xa0, 0xe6, 0x67, 0x05,
  0x10, 0x19, 0x98, 0x08, 0x08, 0xe2, 0x47, 0x10, 0x04, 0x0c, 0x30, 0x20,
  0x02, 0xf0, 0x0f, 0x40, 0x01, 0x00, 0x00, 0x80
};



static user_key_t user_keys[N_USER_KEYS] =
{
  { "f1",    BX_KEY_F1 },
  { "f2",    BX_KEY_F2 },
  { "f3",    BX_KEY_F3 },
  { "f4",    BX_KEY_F4 },
  { "f5",    BX_KEY_F5 },
  { "f6",    BX_KEY_F6 },
  { "f7",    BX_KEY_F7 },
  { "f8",    BX_KEY_F8 },
  { "f9",    BX_KEY_F9 },
  { "f10",   BX_KEY_F10 },
  { "f11",   BX_KEY_F11 },
  { "f12",   BX_KEY_F12 },
  { "alt",   BX_KEY_ALT_L },
  { "bksl",  BX_KEY_BACKSLASH },
  { "bksp",  BX_KEY_BACKSPACE },
  { "ctrl",  BX_KEY_CTRL_L },
  { "del",   BX_KEY_DELETE },
  { "down",  BX_KEY_DOWN },
  { "end",   BX_KEY_END },
  { "enter", BX_KEY_ENTER },
  { "esc",   BX_KEY_ESC },
  { "home",  BX_KEY_HOME },
  { "ins",   BX_KEY_INSERT },
  { "left",  BX_KEY_LEFT },
  { "menu",  BX_KEY_MENU },
  { "minus", BX_KEY_MINUS },
  { "pgdwn", BX_KEY_PAGE_DOWN },
  { "pgup",  BX_KEY_PAGE_UP },
  { "plus",  BX_KEY_KP_ADD },
  { "right", BX_KEY_RIGHT },
  { "shift", BX_KEY_SHIFT_L },
  { "space", BX_KEY_SPACE },
  { "tab",   BX_KEY_TAB },
  { "up",    BX_KEY_UP },
  { "win",   BX_KEY_WIN_L },
  { "print", BX_KEY_PRINT },
  { "power", BX_KEY_POWER_POWER },
  { "scrlck", BX_KEY_SCRL_LOCK }
};

struct command_mode command_mode;


BOOL has_command_mode(void) { return command_mode.present; }
BOOL command_mode_active(void) { return command_mode.active; }

DWORD get_user_key(char* key)
{
    int i = 0;

    while (i < N_USER_KEYS) {
        if (!strcmp(key, user_keys[i].key))
            return user_keys[i].symbol;
        i++;
    }
    return BX_KEY_UNKNOWN;
}

// font bitmap helper function

BYTE reverse_bitorder(BYTE b)
{
    BYTE ret = 0;

    for (unsigned i = 0; i < 8; i++) {
        ret |= (b & 0x01) << (7 - i);
        b >>= 1;
    }
    return ret;
}

// common gui implementation
/*
bx_gui_c(void) : disp_mode(DISP_MODE_SIM)
{
    put("GUI"); // Init in specific_init
    bx_headerbar_entries = 0;
    statusitem_count = 0;
    led_timer_index = BX_NULL_TIMER_HANDLE;
    framebuffer = NULL;
    guest_textmode = 1;
    guest_fwidth = 8;
    guest_fheight = 16;
    guest_xres = 640;
    guest_yres = 480;
    guest_bpp = 8;
    snapshot_mode = 0;
    snapshot_buffer = NULL;
    command_mode.present = 0;
    command_mode.active = 0;
    marker_count = 0;
    memset(palette, 0, sizeof(palette));
    memset(vga_charmap, 0, 0x2000);
}
*/



// header bar buttons
BOOL floppyA_status;
BOOL floppyB_status;
BOOL cdrom1_status;
unsigned floppyA_bmap_id, floppyA_eject_bmap_id, floppyA_hbar_id;
unsigned floppyB_bmap_id, floppyB_eject_bmap_id, floppyB_hbar_id;
unsigned cdrom1_bmap_id, cdrom1_eject_bmap_id, cdrom1_hbar_id;
unsigned power_bmap_id, power_hbar_id;
unsigned reset_bmap_id, reset_hbar_id;
unsigned copy_bmap_id, copy_hbar_id;
unsigned paste_bmap_id, paste_hbar_id;
unsigned snapshot_bmap_id, snapshot_hbar_id;
unsigned config_bmap_id, config_hbar_id;
unsigned mouse_bmap_id, nomouse_bmap_id, mouse_hbar_id;
unsigned user_bmap_id, user_hbar_id;
unsigned save_restore_bmap_id, save_restore_hbar_id;
// the "classic" Bochs headerbar
unsigned bx_headerbar_entries;
// text charmap
BYTE vga_charmap[0x2000];
BOOL charmap_updated;
BOOL char_changed[256];
// status bar items
unsigned statusitem_count;
int led_timer_index;

// display mode
//disp_mode_t disp_mode;

// new graphics API (with compatibility mode)
BOOL new_gfx_api;
WORD host_xres;
WORD host_yres;
WORD host_pitch;
BYTE host_bpp;
BYTE* framebuffer;
// new text update API
BOOL new_text_api;
WORD cursor_address;
bx_vga_tminfo_t tm_info;
// maximum guest display size and tile size
unsigned max_xres;
unsigned max_yres;
unsigned x_tilesize;
unsigned y_tilesize;
// current guest display settings
BOOL guest_textmode;
BYTE   guest_fwidth;
BYTE   guest_fheight;
WORD  guest_xres;
WORD  guest_yres;
BYTE   guest_bpp;
// graphics snapshot
BOOL snapshot_mode;
BYTE* snapshot_buffer;

// modifier keys
BYTE keymodstate;
// mouse toggle setup
BYTE toggle_method;
DWORD toggle_keystate;
char mouse_toggle_text[20];
// userbutton shortcut
DWORD user_shortcut[4];
int user_shortcut_len;
BOOL user_shortcut_error;
// gui dialog capabilities
DWORD dialog_caps;
BOOL fullscreen_mode;
DWORD marker_count;



extern struct win32_toolbar win32_toolbar_entry[BX_MAX_HEADERBAR_ENTRIES];
extern struct statusitem statusitem[BX_MAX_STATUSITEMS];
extern struct palette palette[256];
extern struct bx_bitmaps bx_bitmaps[BX_MAX_PIXMAPS];
extern struct bx_headerbar bx_headerbar_entry[BX_MAX_HEADERBAR_ENTRIES];

//void set_fullscreen_mode(BOOL active) { fullscreen_mode = active; }


void GuiInitialize(int argc, char** argv, unsigned arg_max_xres, unsigned arg_max_yres,
    unsigned tilewidth, unsigned tileheight)
{
    new_gfx_api = 0;
    host_xres = 640;
    host_yres = 480;
    host_bpp = 8;
    new_text_api = 0;
    memset(&tm_info, 0, sizeof(bx_vga_tminfo_t));
    cursor_address = 0;
    max_xres = arg_max_xres;
    max_yres = arg_max_yres;
    x_tilesize = tilewidth;
    y_tilesize = tileheight;
    dialog_caps = BX_GUI_DLG_RUNTIME | BX_GUI_DLG_SAVE_RESTORE;

    //toggle_method = SIM->get_param_enum(BXPN_MOUSE_TOGGLE)->get();
    toggle_keystate = 0;
    switch (toggle_method) {
    case BX_MOUSE_TOGGLE_CTRL_MB:
        strcpy_s(mouse_toggle_text, 20, "CTRL + 3rd button");
        break;
    case BX_MOUSE_TOGGLE_CTRL_F10:
        strcpy_s(mouse_toggle_text, 20, "CTRL + F10");
        break;
    case BX_MOUSE_TOGGLE_CTRL_ALT:
        strcpy_s(mouse_toggle_text, 20, "CTRL + ALT");
        break;
    case BX_MOUSE_TOGGLE_F12:
        strcpy_s(mouse_toggle_text, 20, "F12");
        break;
    }

    Win32GuiSpecificInit(argc, argv, BX_HEADER_BAR_Y);
    // Define some bitmaps to use in the headerbar
     floppyA_bmap_id = create_bitmap(bx_floppya_bmap,
        BX_FLOPPYA_BMAP_X, BX_FLOPPYA_BMAP_Y);
     floppyA_eject_bmap_id = create_bitmap(bx_floppya_eject_bmap,
        BX_FLOPPYA_BMAP_X, BX_FLOPPYA_BMAP_Y);
     floppyB_bmap_id = create_bitmap(bx_floppyb_bmap,
        BX_FLOPPYB_BMAP_X, BX_FLOPPYB_BMAP_Y);
     floppyB_eject_bmap_id = create_bitmap(bx_floppyb_eject_bmap,
        BX_FLOPPYB_BMAP_X, BX_FLOPPYB_BMAP_Y);
     cdrom1_bmap_id = create_bitmap(bx_cdromd_bmap,
        BX_CDROMD_BMAP_X, BX_CDROMD_BMAP_Y);
     cdrom1_eject_bmap_id = create_bitmap(bx_cdromd_eject_bmap,
        BX_CDROMD_BMAP_X, BX_CDROMD_BMAP_Y);
     mouse_bmap_id = create_bitmap(bx_mouse_bmap,
        BX_MOUSE_BMAP_X, BX_MOUSE_BMAP_Y);
     nomouse_bmap_id = create_bitmap(bx_nomouse_bmap,
        BX_MOUSE_BMAP_X, BX_MOUSE_BMAP_Y);

     power_bmap_id = create_bitmap(bx_power_bmap, BX_POWER_BMAP_X, BX_POWER_BMAP_Y);
     reset_bmap_id = create_bitmap(bx_reset_bmap, BX_RESET_BMAP_X, BX_RESET_BMAP_Y);
     /*
     snapshot_bmap_id = create_bitmap(bx_snapshot_bmap, BX_SNAPSHOT_BMAP_X, BX_SNAPSHOT_BMAP_Y);
     copy_bmap_id = create_bitmap(bx_copy_bmap, BX_COPY_BMAP_X, BX_COPY_BMAP_Y);
     paste_bmap_id = create_bitmap(bx_paste_bmap, BX_PASTE_BMAP_X, BX_PASTE_BMAP_Y);
     config_bmap_id = create_bitmap(bx_config_bmap, BX_CONFIG_BMAP_X, BX_CONFIG_BMAP_Y);
     user_bmap_id = create_bitmap(bx_user_bmap, BX_USER_BMAP_X, BX_USER_BMAP_Y);
     save_restore_bmap_id = create_bitmap(bx_save_restore_bmap,
        BX_SAVE_RESTORE_BMAP_X, BX_SAVE_RESTORE_BMAP_Y);
    
    // Add the initial bitmaps to the headerbar, and enable callback routine, for use
    // when that bitmap is clicked on. The floppy and cdrom devices are not
    // initialized yet. so we just set the bitmaps to ejected for now.

    // Floppy A:
     floppyA_hbar_id = headerbar_bitmap( floppyA_eject_bmap_id,
        BX_GRAVITY_LEFT, floppyA_handler);
     set_tooltip( floppyA_hbar_id, "Change floppy A: media");

    // Floppy B:
     floppyB_hbar_id = headerbar_bitmap( floppyB_eject_bmap_id,
        BX_GRAVITY_LEFT, floppyB_handler);
     set_tooltip( floppyB_hbar_id, "Change floppy B: media");

    // First CD-ROM
     cdrom1_hbar_id = headerbar_bitmap( cdrom1_eject_bmap_id,
        BX_GRAVITY_LEFT, cdrom1_handler);
     set_tooltip( cdrom1_hbar_id, "Change first CDROM media");
     */
    // Mouse button
     /*
    if (SIM->get_param_BOOL(BXPN_MOUSE_ENABLED)->get())
         mouse_hbar_id = headerbar_bitmap( mouse_bmap_id,
            BX_GRAVITY_LEFT, toggle_mouse_enable);
    else
         mouse_hbar_id = headerbar_bitmap( nomouse_bmap_id,
            BX_GRAVITY_LEFT, toggle_mouse_enable);
    
     set_tooltip( mouse_hbar_id, "Enable mouse capture");

    // These are the buttons on the right side.  They are created in order
    // of right to left.

    // Power button
     power_hbar_id = headerbar_bitmap( power_bmap_id,
        BX_GRAVITY_RIGHT, power_handler);
     set_tooltip( power_hbar_id, "Turn power off");
    // Save/Restore Button
     save_restore_hbar_id = headerbar_bitmap( save_restore_bmap_id,
        BX_GRAVITY_RIGHT, save_restore_handler);
     set_tooltip( save_restore_hbar_id, "Save simulation state");
    // Reset button
     /*
     reset_hbar_id = headerbar_bitmap( reset_bmap_id,
        BX_GRAVITY_RIGHT, reset_handler);
    
     set_tooltip( reset_hbar_id, "Reset the system");
    // Configure button
     config_hbar_id = headerbar_bitmap( config_bmap_id,
        BX_GRAVITY_RIGHT, config_handler);
     set_tooltip( config_hbar_id, "Runtime config dialog");
    // Snapshot button
     snapshot_hbar_id = headerbar_bitmap( snapshot_bmap_id,
        BX_GRAVITY_RIGHT, snapshot_handler);
     set_tooltip( snapshot_hbar_id, "Save snapshot of the Bochs screen");
    // Paste button
     paste_hbar_id = headerbar_bitmap( paste_bmap_id,
        BX_GRAVITY_RIGHT, paste_handler);
     set_tooltip( paste_hbar_id, "Paste clipboard text as emulated keystrokes");
    // Copy button
    
     copy_hbar_id = headerbar_bitmap( copy_bmap_id,
        BX_GRAVITY_RIGHT, copy_handler);
     set_tooltip( copy_hbar_id, "Copy text mode screen to the clipboard");
    // User button
     user_hbar_id = headerbar_bitmap( user_bmap_id,
        BX_GRAVITY_RIGHT, userbutton_handler);
     set_tooltip( user_hbar_id, "Send keyboard shortcut");
    
    if (!parse_user_shortcut(SIM->get_param_string(BXPN_USER_SHORTCUT)->getptr())) {
        SIM->get_param_string(BXPN_USER_SHORTCUT)->set("none");
    }
    */

     charmap_updated = 0;

    if (! new_gfx_api && ( framebuffer == NULL)) {
         framebuffer = malloc(max_xres * max_yres * 4);
    }
    show_headerbar();
    /*
    // register timer for status bar LEDs
    if ( led_timer_index == BX_NULL_TIMER_HANDLE) {
         led_timer_index =
            bx_virt_timer.register_timer(this, led_timer_handler, 100000, 1, 1, 1,
                "status bar LEDs");
    }
    */
}
void cleanup(void)
{
    statusitem_count = 0;
}

void update_drive_status_buttons(void)
{
    /*
     floppyA_status = (SIM->get_param_enum(BXPN_FLOPPYA_STATUS)->get() == BX_INSERTED);
     floppyB_status = (SIM->get_param_enum(BXPN_FLOPPYB_STATUS)->get() == BX_INSERTED);
    bx_param_c* cdrom = SIM->get_first_cdrom();
    if (cdrom != NULL) {
         cdrom1_status = SIM->get_param_enum("status", cdrom)->get();
    }
    else {
         cdrom1_status = BX_EJECTED;
    }
    if ( floppyA_status)
        replace_bitmap( floppyA_hbar_id,  floppyA_bmap_id);
    else {
#if BX_WITH_MACOS
        // If we are using the Mac floppy driver, eject the disk
        // from the floppy drive.  This doesn't work in MacOS X.
        if (!strcmp(SIM->get_param_string(BXPN_FLOPPYA_PATH)->getptr(), SuperDrive))
            DiskEject(1);
#endif
        replace_bitmap( floppyA_hbar_id,  floppyA_eject_bmap_id);
    }
    if ( floppyB_status)
        replace_bitmap( floppyB_hbar_id,  floppyB_bmap_id);
    else {
#if BX_WITH_MACOS
        // If we are using the Mac floppy driver, eject the disk
        // from the floppy drive.  This doesn't work in MacOS X.
        if (!strcmp(SIM->get_param_string(BXPN_FLOPPYB_PATH)->getptr(), SuperDrive))
            DiskEject(1);
#endif
        replace_bitmap( floppyB_hbar_id,  floppyB_eject_bmap_id);
    }
    if ( cdrom1_status)
        replace_bitmap( cdrom1_hbar_id,  cdrom1_bmap_id);
    else {
        replace_bitmap( cdrom1_hbar_id,  cdrom1_eject_bmap_id);
    }
    */
}

void floppyA_handler(void)
{
    int ret = 1;
    /*
    if (SIM->get_param_enum(BXPN_FLOPPYA_DEVTYPE)->get() == BX_FDD_NONE)
        return; // no primary floppy device present
    if (! fullscreen_mode &&
        ( dialog_caps & BX_GUI_DLG_FLOPPY)) {
        // instead of just toggling the status, bring up a dialog asking what disk
        // image you want to switch to.
        ret = SIM->ask_param(BXPN_FLOPPYA);
    }
    else {
        SIM->get_param_enum(BXPN_FLOPPYA_STATUS)->set(! floppyA_status);
    }
    if (ret > 0) {
        SIM->update_runtime_options();
    }
    */
}

void floppyB_handler(void)
{
    int ret = 1;
    /*
    if (SIM->get_param_enum(BXPN_FLOPPYB_DEVTYPE)->get() == BX_FDD_NONE)
        return; // no secondary floppy device present
    if (! fullscreen_mode &&
        ( dialog_caps & BX_GUI_DLG_FLOPPY)) {
        // instead of just toggling the status, bring up a dialog asking what disk
        // image you want to switch to.
        ret = SIM->ask_param(BXPN_FLOPPYB);
    }
    else {
        SIM->get_param_enum(BXPN_FLOPPYB_STATUS)->set(! floppyB_status);
    }
    if (ret > 0) {
        SIM->update_runtime_options();
    }
    */
}

void cdrom1_handler(void)
{
    int ret = 1;
    /*
    bx_param_c* cdrom = SIM->get_first_cdrom();

    if (cdrom == NULL)
        return;  // no cdrom found
    if ( dialog_caps & BX_GUI_DLG_CDROM) {
        // instead of just toggling the status, bring up a dialog asking what disk
        // image you want to switch to.
        // This code handles the first cdrom only. The cdrom drives #2, #3 and
        // #4 are handled in the runtime configuaration.
        ret = SIM->ask_param(cdrom);
    }
    else {
        SIM->get_param_enum("status", cdrom)->set(! cdrom1_status);
    }
    if (ret > 0) {
        SIM->update_runtime_options();
    }
    */
}

void reset_handler(void)
{
    //BX_INFO(("system RESET callback"));
    //bx_pc_system.Reset(BX_RESET_HARDWARE);
}

void power_handler(void)
{
#ifdef BX_GUI_CONFIRM_QUIT
    if (!SIM->ask_yes_no("Quit Bochs", "Are you sure ?", 0))
        return;
#endif
    // the user pressed power button, so there's no doubt they want bochs
    // to quit.  Change panics to fatal for the GUI and then do a panic.
    //bx_user_quit = 1;
    //BX_FATAL(("POWER button turned off."));
    // shouldn't reach this point, but if you do, QUIT!!!
    //fprintf(stderr, "Bochs is exiting because you pressed the power button.\n");
    exit(-1);
}

void make_text_snapshot(char** snapshot, DWORD* length)
{
    BYTE* raw_snap = NULL;
    char* clean_snap;
    unsigned line_addr, txt_addr, txHeight, txWidth;

   // DEV_vga_get_text_snapshot(&raw_snap, &txHeight, &txWidth);
    clean_snap = malloc(txHeight * (txWidth + 2) + 1);
    txt_addr = 0;
    for (unsigned i = 0; i < txHeight; i++) {
        line_addr = i * txWidth * 2;
        for (unsigned j = 0; j < (txWidth * 2); j += 2) {
            if (!raw_snap[line_addr + j])
                raw_snap[line_addr + j] = 0x20;
            clean_snap[txt_addr++] = raw_snap[line_addr + j];
        }
        while ((txt_addr > 0) && (clean_snap[txt_addr - 1] == ' ')) txt_addr--;
#ifdef WIN32
        clean_snap[txt_addr++] = 13;
#endif
        clean_snap[txt_addr++] = 10;
    }
    clean_snap[txt_addr] = 0;
    *snapshot = clean_snap;
    *length = txt_addr;
}

DWORD set_snapshot_mode(BOOL mode)
{
    unsigned pixel_bytes, bufsize;

     snapshot_mode = mode;
    if (mode) {
        pixel_bytes = (( guest_bpp + 1) >> 3);
        bufsize =  guest_xres *  guest_yres * pixel_bytes;
         snapshot_buffer = malloc(bufsize);
        if ( snapshot_buffer != NULL) {
            memset( snapshot_buffer, 0, bufsize);
            //DEV_vga_refresh(1);
            return bufsize;
        }
    }
    else {
        if ( snapshot_buffer != NULL) {
             free(snapshot_buffer);
             snapshot_buffer = NULL;
            //DEV_vga_redraw_area(0, 0,  guest_xres,  guest_yres);
        }
    }
    return 0;
}

// create a text snapshot and copy to the system clipboard.  On guis that
// we haven't figured out how to support yet, dump to a file instead.
void copy_handler(void)
{
    DWORD len;
    char* text_snapshot;
    /*
    if ( guest_textmode) {
        make_text_snapshot(&text_snapshot, &len);
        if (! set_clipboard_text(text_snapshot, len)) {
            // platform specific code failed, use portable code instead
            FILE* fp = fopen("copy.txt", "w");
            if (fp != NULL) {
                fwrite(text_snapshot, 1, len, fp);
                fclose(fp);
            }
        }
        delete[] text_snapshot;
    }
    */

}

#define BX_SNAPSHOT_TXT 0
#define BX_SNAPSHOT_BMP 1

// create a text snapshot and dump it to a file
void snapshot_handler(void)
{
    /*
    int fd, i, j, pitch;
    BYTE* snapshot_ptr = NULL;
    BYTE* row_buffer, * pixel_ptr, * row_ptr;
    BYTE bmp_header[54], iBits, b1, b2;
    DWORD ilen, len, rlen;
    char filename[256], msg[80], * ext;
    BYTE snap_fmt;

    if ( guest_textmode) {
        strcpy(filename, "snapshot.txt");
        snap_fmt = BX_SNAPSHOT_TXT;
    }
    else {
        strcpy(filename, "snapshot.bmp");
        snap_fmt = BX_SNAPSHOT_BMP;
    }
    if (! fullscreen_mode &&
        ( dialog_caps & BX_GUI_DLG_SNAPSHOT)) {
        int ret = SIM->ask_filename(filename, sizeof(filename),
            "Save snapshot as...", filename,
            bx_param_string_c::SAVE_FILE_DIALOG);
        if (ret < 0) { // cancelled
            return;
        }
        ext = strrchr(filename, '.');
        if (ext == NULL) {
            SIM->message_box("ERROR", "Unknown snapshot file format");
            return;
        }
        else {
            ext++;
            if ( guest_textmode && !strcmp(ext, "txt")) {
                snap_fmt = BX_SNAPSHOT_TXT;
            }
            else if (!strcmp(ext, "bmp")) {
                snap_fmt = BX_SNAPSHOT_BMP;
            }
            else {
                sprintf(msg, "Unsupported snapshot file format '%s'", ext);
                SIM->message_box("ERROR", msg);
                return;
            }
        }
    }
    if (snap_fmt == BX_SNAPSHOT_BMP) {
        fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC
#ifdef O_BINARY
            | O_BINARY
#endif
            , S_IRUSR | S_IWUSR
        );
        if (fd < 0) {
            SIM->message_box("ERROR", "snapshot button failed: cannot create BMP file");
            return;
        }
        ilen =  set_snapshot_mode(1);
        if (ilen > 0) {
            BX_INFO(("GFX snapshot: %u x %u x %u bpp (%u bytes)",  guest_xres,
                 guest_yres,  guest_bpp, ilen));
        }
        else {
            close(fd);
            SIM->message_box("ERROR", "snapshot button failed: cannot allocate memory");
            return;
        }
        iBits = ( guest_bpp == 8) ? 8 : 24;
        rlen = ( guest_xres * (iBits >> 3) + 3) & ~3;
        len = rlen *  guest_yres + 54;
        if ( guest_bpp == 8) {
            len += (256 * 4);
        }
        memset(bmp_header, 0, 54);
        bmp_header[0] = 0x42;
        bmp_header[1] = 0x4d;
        bmp_header[2] = len & 0xff;
        bmp_header[3] = (len >> 8) & 0xff;
        bmp_header[4] = (len >> 16) & 0xff;
        bmp_header[5] = (len >> 24) & 0xff;
        bmp_header[10] = 54;
        if ( guest_bpp == 8) {
            bmp_header[11] = 4;
        }
        bmp_header[14] = 40;
        bmp_header[18] =  guest_xres & 0xff;
        bmp_header[19] = ( guest_xres >> 8) & 0xff;
        bmp_header[22] =  guest_yres & 0xff;
        bmp_header[23] = ( guest_yres >> 8) & 0xff;
        bmp_header[26] = 1;
        bmp_header[28] = iBits;
        write(fd, bmp_header, 54);
        if ( guest_bpp == 8) {
            write(fd,  palette, 256 * 4);
        }
        pitch =  guest_xres * (( guest_bpp + 1) >> 3);
        row_buffer = new BYTE[rlen];
        row_ptr =  snapshot_buffer + (( guest_yres - 1) * pitch);
        for (i =  guest_yres; i > 0; i--) {
            memset(row_buffer, 0, rlen);
            if (( guest_bpp == 8) || ( guest_bpp == 24)) {
                memcpy(row_buffer, row_ptr, pitch);
            }
            else if (( guest_bpp == 15) || ( guest_bpp == 16)) {
                pixel_ptr = row_ptr;
                for (j = 0; j < (int)( guest_xres * 3); j += 3) {
                    b1 = *(pixel_ptr++);
                    b2 = *(pixel_ptr++);
                    *(row_buffer + j) = (b1 << 3);
                    if ( guest_bpp == 15) {
                        *(row_buffer + j + 1) = ((b1 & 0xe0) >> 2) | (b2 << 6);
                        *(row_buffer + j + 2) = (b2 & 0x7c) << 1;
                    }
                    else {
                        *(row_buffer + j + 1) = ((b1 & 0xe0) >> 3) | (b2 << 5);
                        *(row_buffer + j + 2) = (b2 & 0xf8);
                    }
                }
            }
            else if ( guest_bpp == 32) {
                pixel_ptr = row_ptr;
                for (j = 0; j < (int)( guest_xres * 3); j += 3) {
                    *(row_buffer + j) = *(pixel_ptr++);
                    *(row_buffer + j + 1) = *(pixel_ptr++);
                    *(row_buffer + j + 2) = *(pixel_ptr++);
                    pixel_ptr++;
                }
            }
            write(fd, row_buffer, rlen);
            row_ptr -= pitch;
        }
        delete[] row_buffer;
        close(fd);
         set_snapshot_mode(0);
    }
    else {
        make_text_snapshot((char**)&snapshot_ptr, &len);
        FILE* fp = fopen(filename, "wb");
        if (fp != NULL) {
            fwrite(snapshot_ptr, 1, len, fp);
            fclose(fp);
        }
        else {
            SIM->message_box("ERROR", "snapshot button failed: cannot create text file");
        }
        delete[] snapshot_ptr;
    }
    */
}

// Read ASCII chars from the system clipboard and paste them into bochs.
// Note that paste cannot work with the key mapping tables loaded.
void paste_handler(void)
{
    /*
    DWORD nbytes;
    BYTE* bytes;
    if (!bx_keymap.isKeymapLoaded()) {
        BX_ERROR(("keyboard_mapping disabled, so paste cannot work"));
        return;
    }
    if (! get_clipboard_text(&bytes, &nbytes)) {
        BX_ERROR(("paste not implemented on this platform"));
        return;
    }
    BX_INFO(("pasting %d bytes", nbytes));
    DEV_kbd_paste_bytes(bytes, nbytes);
    */
}

void config_handler(void)
{
    /*
    if ( dialog_caps & BX_GUI_DLG_RUNTIME) {
        SIM->configuration_interface(NULL, CI_RUNTIME_CONFIG);
    }
    */
}

void toggle_mouse_enable(void)
{
    //int old = SIM->get_param_BOOL(BXPN_MOUSE_ENABLED)->get();
    //BX_DEBUG(("toggle mouse_enabled, now %d", !old));
    //SIM->get_param_BOOL(BXPN_MOUSE_ENABLED)->set(!old);
}

BOOL mouse_toggle_check(DWORD key, BOOL pressed)
{
    DWORD newstate;
    BOOL toggle = 0;

    //if (console_running())
      //  return 0;
    newstate = toggle_keystate;
    if (pressed) {
        newstate |= key;
        if (newstate == toggle_keystate) return 0;
        switch (toggle_method) {
        case BX_MOUSE_TOGGLE_CTRL_MB:
            toggle = (newstate & BX_GUI_MT_CTRL_MB) == BX_GUI_MT_CTRL_MB;
            if (!toggle) {
                toggle = (newstate & BX_GUI_MT_CTRL_LRB) == BX_GUI_MT_CTRL_LRB;
            }
            break;
        case BX_MOUSE_TOGGLE_CTRL_F10:
            toggle = (newstate & BX_GUI_MT_CTRL_F10) == BX_GUI_MT_CTRL_F10;
            break;
        case BX_MOUSE_TOGGLE_CTRL_ALT:
            toggle = (newstate & BX_GUI_MT_CTRL_ALT) == BX_GUI_MT_CTRL_ALT;
            break;
        case BX_MOUSE_TOGGLE_F12:
            toggle = (newstate == BX_GUI_MT_F12);
            break;
        }
        toggle_keystate = newstate;
    }
    else {
        toggle_keystate &= ~key;
    }
    return toggle;
}

const char* get_toggle_info(void)
{
    return mouse_toggle_text;
}

BYTE get_modifier_keys(void)
{
    if ((keymodstate & BX_MOD_KEY_CAPS) > 0) {
        return ((keymodstate & ~BX_MOD_KEY_CAPS) | BX_MOD_KEY_SHIFT);
    }
    else {
        return keymodstate;
    }
}

BYTE set_modifier_keys(BYTE modifier, BOOL pressed)
{
    BYTE newstate = keymodstate, changestate = 0;

    if (modifier == BX_MOD_KEY_CAPS) {
        if (pressed) {
            newstate ^= modifier;
        }
    }
    else {
        if (pressed) {
            newstate |= modifier;
        }
        else {
            newstate &= ~modifier;
        }
    }
    changestate = keymodstate ^ newstate;
    keymodstate = newstate;
    return changestate;
}

BOOL parse_user_shortcut(const char* val)
{
    char* ptr, shortcut_tmp[512];
    DWORD symbol, new_shortcut[4];
    int i, len = 0;

    user_shortcut_error = 0;
    if ((strlen(val) == 0) || !strcmp(val, "none")) {
        user_shortcut_len = 0;
        return 1;
    }
    else {
        len = 0;
        strcpy_s(shortcut_tmp, 512, val);
        ptr = strtok_s(shortcut_tmp, "-", NULL);
        while (ptr) {
            symbol = get_user_key(ptr);
            if (symbol == BX_KEY_UNKNOWN) {
                //BX_ERROR(("Unknown key symbol '%s' ignored", ptr));
                user_shortcut_error = 1;
                return 0;
            }
            if (len < 3) {
                new_shortcut[len++] = symbol;
                ptr = strtok_s(NULL, "-", NULL);
            }
            else {
                //BX_ERROR(("Ignoring extra key symbol '%s'", ptr));
                break;
            }
        }
        for (i = 0; i < len; i++) {
            user_shortcut[i] = new_shortcut[i];
        }
        user_shortcut_len = len;
        return 1;
    }
}

void userbutton_handler(void)
{
    int i, ret = 1;

    if (! fullscreen_mode &&
        ( dialog_caps & BX_GUI_DLG_USER)) {
        //ret = SIM->ask_param(BXPN_USER_SHORTCUT);
    }
    if ( user_shortcut_error) {
        //SIM->message_box("ERROR", "Ignoring invalid user shortcut");
        return;
    }
    if ((ret > 0) && ( user_shortcut_len > 0)) {
        i = 0;
        while (i <  user_shortcut_len) {
            //DEV_kbd_gen_scancode( user_shortcut[i++]);
        }
        i--;
        while (i >= 0) {
            //DEV_kbd_gen_scancode( user_shortcut[i--] | BX_KEY_RELEASED);
        }
    }
}

void save_restore_handler(void)
{
    int ret;
    char sr_path[256];

    /*
    if ( dialog_caps & BX_GUI_DLG_SAVE_RESTORE) {
         set_display_mode(DISP_MODE_CONFIG);
        sr_path[0] = 0;
        ret = SIM->ask_filename(sr_path, sizeof(sr_path),
            "Save Bochs state to folder...", "none",
            bx_param_string_c::SELECT_FOLDER_DLG);
        if ((ret >= 0) && (strcmp(sr_path, "none"))) {
            if (SIM->save_state(sr_path)) {
                if (!SIM->ask_yes_no("WARNING",
                    "The state of cpu, memory, devices and hard drive images is saved now.\n"
                    "It is possible to continue, but when using the restore function in a\n"
                    "new Bochs session, all changes after this checkpoint will be lost.\n\n"
                    "Do you want to continue?", 0)) {
                    power_handler();
                }
            }
            else {
                SIM->message_box("ERROR", "Failed to save state");
            }
        }
         set_display_mode(DISP_MODE_SIM);
    }
    */
}

void marklog_handler(void)
{
}

void headerbar_click(int x)
{
    int xorigin;

    for (unsigned i = 0; i < bx_headerbar_entries; i++) {
        if (bx_headerbar_entry[i].alignment == BX_GRAVITY_LEFT)
            xorigin = bx_headerbar_entry[i].xorigin;
        else
            xorigin = guest_xres - bx_headerbar_entry[i].xorigin;
        if ((x >= xorigin) && (x < (xorigin + bx_headerbar_entry[i].xdim))) {
            //if (console_running() && (i != power_hbar_id))
              //  return;
            bx_headerbar_entry[i].f();
            return;
        }
    }
}

void mouse_enabled_changed(BOOL val)
{
    // This is only called when SIM->get_init_done is 1.  Note that VAL
    // is the new value of mouse_enabled, which may not match the old
    // value which is still in SIM->get_param_BOOL(BXPN_MOUSE_ENABLED)->get().
    //BX_DEBUG(("replacing the mouse bitmaps"));
    if (val)
         replace_bitmap( mouse_hbar_id,  mouse_bmap_id);
    else
         replace_bitmap( mouse_hbar_id,  nomouse_bmap_id);
    // give the GUI a chance to respond to the event.  Most guis will hide
    // the native mouse cursor and do something to trap the mouse inside the
    // bochs VGA display window.
     mouse_enabled_changed_specific(val);
}

void init_signal_handlers()
{

}

void set_text_charmap(BYTE* fbuffer)
{
    memcpy(&vga_charmap, fbuffer, 0x2000);
    for (unsigned i = 0; i < 256; i++) 
        char_changed[i] = 1;
     charmap_updated = 1;
}

void set_text_charbyte(WORD address, BYTE data)
{
     vga_charmap[address] = data;
     char_changed[address >> 5] = 1;
     charmap_updated = 1;
}

void beep_on(float frequency)
{
}

void beep_off()
{
}

int register_statusitem(const char* text, BOOL auto_off)
{
    unsigned id = statusitem_count;

    for (unsigned i = 0; i < statusitem_count; i++) {
        if (!statusitem[i].in_use) {
            id = i;
            break;
        }
    }
    if (id == statusitem_count) {
        if (statusitem_count == BX_MAX_STATUSITEMS) {
            return -1;
        }
        else {
            statusitem_count++;
        }
    }
    statusitem[id].in_use = 1;
    strncpy_s(statusitem[id].text, 8, text, 8);
    statusitem[id].text[7] = 0;
    statusitem[id].auto_off = auto_off;
    statusitem[id].counter = 0;
    statusitem[id].active = 0;
    statusitem[id].mode = 0;
    statusbar_setitem_specific(id, 0, 0);
    return id;
}

void unregister_statusitem(int id)
{
    if ((id >= 0) && (id < (int)statusitem_count)) {
        strcpy_s(statusitem[id].text, 8, "      ");
        statusbar_setitem_specific(id, 0, 0);
        if (id == (int)(statusitem_count - 1)) {
            statusitem_count--;
        }
        else {
            statusitem[id].in_use = 0;
        }
    }
}

void statusbar_setitem(int element, BOOL active, BOOL w)
{
    if (element < 0) {
        for (unsigned i = 0; i < statusitem_count; i++) {
            statusbar_setitem_specific(i, 0, 0);
        }
    }
    else if ((unsigned)element < statusitem_count) {
        if ((active != statusitem[element].active) ||
            (w != statusitem[element].mode)) {
            statusbar_setitem_specific(element, active, w);
            statusitem[element].active = active;
            statusitem[element].mode = w;
        }
        if (active && statusitem[element].auto_off) {
            statusitem[element].counter = 5;
        }
    }
}


void led_timer()
{
    for (unsigned i = 0; i < statusitem_count; i++) {
        if (statusitem[i].auto_off) {
            if (statusitem[i].counter > 0) {
                if (!(--statusitem[i].counter)) {
                    statusbar_setitem(i, 0, FALSE);
                }
            }
        }
    }
}

void led_timer_handler(void* this_ptr)
{

    led_timer();
}



BOOL palette_change_common(BYTE index, BYTE red, BYTE green, BYTE blue)
{
     palette[index].red = red;
     palette[index].green = green;
     palette[index].blue = blue;
    return palette_change(index, red, green, blue);
}

BYTE get_mouse_headerbar_id()
{
    return  mouse_hbar_id;
}



// new graphics API (compatibility code)

bx_svga_tileinfo_t* graphics_tile_info(bx_svga_tileinfo_t* info)
{
     host_pitch =  host_xres * (( host_bpp + 1) >> 3);

    info->bpp =  host_bpp;
    info->pitch =  host_pitch;
    switch (info->bpp) {
    case 15:
        info->red_shift = 15;
        info->green_shift = 10;
        info->blue_shift = 5;
        info->red_mask = 0x7c00;
        info->green_mask = 0x03e0;
        info->blue_mask = 0x001f;
        break;
    case 16:
        info->red_shift = 16;
        info->green_shift = 11;
        info->blue_shift = 5;
        info->red_mask = 0xf800;
        info->green_mask = 0x07e0;
        info->blue_mask = 0x001f;
        break;
    case 24:
    case 32:
        info->red_shift = 24;
        info->green_shift = 16;
        info->blue_shift = 8;
        info->red_mask = 0xff0000;
        info->green_mask = 0x00ff00;
        info->blue_mask = 0x0000ff;
        break;
    }
    info->is_indexed = ( host_bpp == 8);
#ifdef BX_LITTLE_ENDIAN
    info->is_little_endian = 1;
#else
    info->is_little_endian = 0;
#endif

    return info;
}

BYTE* graphics_tile_get(unsigned x0, unsigned y0,
    unsigned* w, unsigned* h)
{
    if (x0 +  x_tilesize >  host_xres) {
        *w =  host_xres - x0;
    }
    else {
        *w =  x_tilesize;
    }

    if (y0 +  y_tilesize >  host_yres) {
        *h =  host_yres - y0;
    }
    else {
        *h =  y_tilesize;
    }

    return (BYTE*)framebuffer + y0 *  host_pitch +
        x0 * (( host_bpp + 1) >> 3);
}

void graphics_tile_update_in_place(unsigned x0, unsigned y0,
    unsigned w, unsigned h)
{
    BYTE* tile;
    BYTE* tile_ptr, * fb_ptr;
    WORD xc, yc, fb_pitch, tile_pitch;
    BYTE r, diffx, diffy;

    tile = malloc(x_tilesize *  y_tilesize * 4);
    diffx = (x0 %  x_tilesize);
    diffy = (y0 %  y_tilesize);
    if (diffx > 0) {
        x0 -= diffx;
        w += diffx;
    }
    if (diffy > 0) {
        y0 -= diffy;
        h += diffy;
    }
    fb_pitch =  host_pitch;
    tile_pitch =  x_tilesize * (( host_bpp + 1) >> 3);
    for (yc = y0; yc < (y0 + h); yc +=  y_tilesize) {
        for (xc = x0; xc < (x0 + w); xc +=  x_tilesize) {
            fb_ptr =  framebuffer + (yc * fb_pitch + xc * (( host_bpp + 1) >> 3));
            tile_ptr = &tile[0];
            for (r = 0; r < h; r++) {
                memcpy(tile_ptr, fb_ptr, tile_pitch);
                fb_ptr += fb_pitch;
                tile_ptr += tile_pitch;
            }
             graphics_tile_update(tile, xc, yc);
        }
    }
    free(tile);
}

void draw_char_common(BYTE ch, BYTE fc, BYTE bc, WORD xc,
    WORD yc, BYTE fw, BYTE fh, BYTE fx,
    BYTE fy, BOOL gfxcharw9, BYTE cs, BYTE ce,
    BOOL curs)
{
    BYTE* buf, * font_ptr, fontpixels;
    WORD font_row, mask;
    BOOL dwidth;

    buf =  snapshot_buffer + yc *  guest_xres + xc;
    dwidth = ( guest_fwidth > 9);
    font_ptr = &vga_charmap[(ch << 5) + fy];
    do {
        font_row = *font_ptr++;
        if (gfxcharw9) {
            font_row = (font_row << 1) | (font_row & 0x01);
        }
        else {
            font_row <<= 1;
        }
        if (fx > 0) {
            font_row <<= fx;
        }
        fontpixels = fw;
        if (curs && (fy >= cs) && (fy <= ce))
            mask = 0x100;
        else
            mask = 0x00;
        do {
            if ((font_row & 0x100) == mask)
                *buf = bc;
            else
                *buf = fc;
            buf++;
            if (!dwidth || (fontpixels & 1)) font_row <<= 1;
        } while (--fontpixels);
        buf += ( guest_xres - fw);
        fy++;
    } while (--fh);
}

void text_update_common(BYTE* old_text, BYTE* new_text,
    WORD cursor_address,
    bx_vga_tminfo_t* arg_tm_info)
{
    WORD curs, cursor_x, cursor_y, xc, yc, rows, hchars, text_cols;
    WORD offset, loffset;
    BYTE cfheight, cfwidth, cfrow, cfcol, fgcolor, bgcolor;
    BYTE split_textrow, split_fontrows, x, y;
    BYTE* new_line, * old_line, * text_base;
    BOOL cursor_visible, gfxcharw9, split_screen;
    BOOL forceUpdate = 0, blink_mode = 0, blink_state = 0;

    if ( snapshot_mode ||  new_text_api) {
        cursor_visible = ((arg_tm_info->cs_start <= arg_tm_info->cs_end) &&
            (arg_tm_info->cs_start <  guest_fheight));
        if ( snapshot_mode && ( snapshot_buffer != NULL)) {
            forceUpdate = 1;
        }
        else if ( new_text_api) {
            blink_mode = (arg_tm_info->blink_flags & BX_TEXT_BLINK_MODE) > 0;
            blink_state = (arg_tm_info->blink_flags & BX_TEXT_BLINK_STATE) > 0;
            if (blink_mode) {
                if (arg_tm_info->blink_flags & BX_TEXT_BLINK_TOGGLE)
                    forceUpdate = 1;
                if (!blink_state) cursor_visible = 0;
            }
            if ( charmap_updated) {
                 set_font(arg_tm_info->line_graphics);
                 charmap_updated = 0;
                forceUpdate = 1;
            }
            if ((arg_tm_info->h_panning !=  tm_info.h_panning) ||
                (arg_tm_info->v_panning !=  tm_info.v_panning) ||
                (arg_tm_info->line_compare !=  tm_info.line_compare)) {
                 tm_info.h_panning = arg_tm_info->h_panning;
                 tm_info.v_panning = arg_tm_info->v_panning;
                 tm_info.line_compare = arg_tm_info->line_compare;
                forceUpdate = 1;
            }
            // invalidate character at previous and new cursor location
            if (cursor_address !=  cursor_address) {
                old_text[ cursor_address] = ~new_text[ cursor_address];
                 cursor_address = cursor_address;
            }
            if (cursor_address < 0xffff) {
                old_text[cursor_address] = ~new_text[cursor_address];
            }
        }
        rows =  guest_yres /  guest_fheight;
        if (arg_tm_info->v_panning > 0) rows++;
        text_cols =  guest_xres /  guest_fwidth;
        if (cursor_visible) {
            curs = cursor_address;
        }
        else {
            curs = 0xffff;
        }
        if (arg_tm_info->line_compare < 0x3ff) {
            split_textrow = (arg_tm_info->line_compare + arg_tm_info->v_panning) /  guest_fheight;
            split_fontrows = ((arg_tm_info->line_compare + arg_tm_info->v_panning) %  guest_fheight) + 1;
        }
        else {
            split_textrow = 0xff;
            split_fontrows = 0;
        }
        y = 0;
        yc = 0;
        split_screen = 0;
        loffset = arg_tm_info->start_address;
        text_base = new_text;
        new_text += arg_tm_info->start_address;
        old_text += arg_tm_info->start_address;
        do {
            hchars = text_cols;
            if (arg_tm_info->h_panning > 0)
                hchars++;
            cfheight =  guest_fheight;
            cfrow = 0;
            if (split_screen) {
                if (rows == 1) {
                    cfheight = (guest_yres - arg_tm_info->line_compare - 1) %  guest_fheight;
                    if (cfheight == 0) cfheight =  guest_fheight;
                }
            }
            else if (arg_tm_info->v_panning > 0) {
                if (y == 0) {
                    cfheight -= arg_tm_info->v_panning;
                    cfrow = arg_tm_info->v_panning;
                }
                else if (rows == 1) {
                    cfheight = arg_tm_info->v_panning;
                }
            }
            if (y == split_textrow) {
                cfheight = split_fontrows - cfrow;
            }
            new_line = new_text;
            old_line = old_text;
            offset = loffset;
            x = 0;
            xc = 0;
            do {
                cfwidth =  guest_fwidth;
                cfcol = 0;
                if (arg_tm_info->h_panning > 0) {
                    if (x == 0) {
                        cfcol = arg_tm_info->h_panning;
                        cfwidth -= arg_tm_info->h_panning;
                    }
                    else if (hchars == 1) {
                        cfwidth = arg_tm_info->h_panning;
                    }
                }
                // check if char needs to be updated
                if (forceUpdate || (new_text[0] != old_text[0]) ||
                    (new_text[1] != old_text[1])) {
                    fgcolor = arg_tm_info->actl_palette[new_text[1] & 0x0f];
                    if (blink_mode) {
                        bgcolor = arg_tm_info->actl_palette[(new_text[1] >> 4) & 0x07];
                        if (!blink_state && (new_text[1] & 0x80))
                            fgcolor = bgcolor;
                    }
                    else {
                        bgcolor = arg_tm_info->actl_palette[(new_text[1] >> 4) & 0x0F];
                    }
                    gfxcharw9 = ((arg_tm_info->line_graphics) && ((new_text[0] & 0xE0) == 0xC0));
                    if ( snapshot_mode) {
                         draw_char_common(new_text[0], fgcolor, bgcolor, xc, yc,
                            cfwidth, cfheight, cfcol, cfrow,
                            gfxcharw9, arg_tm_info->cs_start,
                             arg_tm_info->cs_end, (offset == curs));
                    }
                    else {
                         draw_char(new_text[0], fgcolor, bgcolor, xc, yc,
                            cfwidth, cfheight, cfcol, cfrow,
                            gfxcharw9, arg_tm_info->cs_start,
                             arg_tm_info->cs_end, (offset == curs));
                    }
                }
                new_text += 2;
                old_text += 2;
                offset += 2;
                x++;
                xc += cfwidth;
            } while (--hchars);
            if (y == split_textrow) {
                new_text = text_base;
                forceUpdate = 1;
                loffset = 0;
                rows = ((guest_yres - arg_tm_info->line_compare +  guest_fheight - 2) /  guest_fheight) + 1;
                if (arg_tm_info->split_hpanning) arg_tm_info->h_panning = 0;
                split_screen = 1;
            }
            else {
                new_text = new_line + arg_tm_info->line_offset;
                old_text = old_line + arg_tm_info->line_offset;
                loffset += arg_tm_info->line_offset;
            }
            y++;
            yc += cfheight;
        } while (--rows);
    }
    else {
        // workarounds for existing text_update() API
        if (cursor_address >= arg_tm_info->start_address) {
            cursor_x = ((cursor_address - arg_tm_info->start_address) % arg_tm_info->line_offset) / 2;
            cursor_y = ((cursor_address - arg_tm_info->start_address) / arg_tm_info->line_offset);
        }
        else {
            cursor_x = 0xffff;
            cursor_y = 0xffff;
        }
        if ((arg_tm_info->blink_flags & BX_TEXT_BLINK_STATE) == 0) {
            arg_tm_info->cs_start |= 0x20;
        }
        new_text += arg_tm_info->start_address;
        old_text += arg_tm_info->start_address;
        text_update(old_text, new_text, cursor_x, cursor_y, arg_tm_info);
    }
}

void graphics_tile_update_common(BYTE* tile, unsigned x, unsigned y)
{
    unsigned i, pitch, pixel_bytes, nbytes, tilebytes;
    BYTE* src, * dst;

    if ( snapshot_mode) {
        if ( snapshot_buffer != NULL) {
            pixel_bytes = (( guest_bpp + 1) >> 3);
            tilebytes =  x_tilesize * pixel_bytes;
            if ((x +  x_tilesize) <=  guest_xres) {
                nbytes = tilebytes;
            }
            else {
                nbytes = ( guest_xres - x) * pixel_bytes;
            }
            pitch =  guest_xres * pixel_bytes;
            src = tile;
            dst =  snapshot_buffer + (y * pitch) + x;
            for (i = 0; i < y_tilesize; i++) {
                memcpy(dst, src, nbytes);
                src += tilebytes;
                dst += pitch;
                if (++y >=  guest_yres) break;
            }
        }
    }
    else {
        graphics_tile_update(tile, x, y);
    }
}

BYTE* get_snapshot_buffer(void) 
{ 
    return snapshot_buffer;
}

bx_svga_tileinfo_t* graphics_tile_info_common(bx_svga_tileinfo_t* info)
{
    if (!info) {
        info = malloc(sizeof(bx_svga_tileinfo_t));
        if (!info) {
            return NULL;
        }
    }
    info->snapshot_mode =  snapshot_mode;
    if ( snapshot_mode) {
        info->pitch =  guest_xres * (( guest_bpp + 1) >> 3);
    }
    else {
        return graphics_tile_info(info);
    }

    return info;
}

#if BX_USE_GUI_CONSOLE

#define BX_CONSOLE_BUFSIZE 4000

void console_init(void)
{
    int i;

    console.screen = new BYTE[BX_CONSOLE_BUFSIZE];
    console.oldscreen = new BYTE[BX_CONSOLE_BUFSIZE];
    for (i = 0; i < BX_CONSOLE_BUFSIZE; i += 2) {
        console.screen[i] = 0x20;
        console.screen[i + 1] = 0x07;
    }
    memset(console.oldscreen, 0xff, BX_CONSOLE_BUFSIZE);
    console.saved_xres = guest_xres;
    console.saved_yres = guest_yres;
    console.saved_bpp = guest_bpp;
    console.saved_fwidth = guest_fwidth;
    console.saved_fheight = guest_fheight;
    memcpy(console.saved_charmap,  vga_charmap, 0x2000);
    for (i = 0; i < 256; i++) {
        memcpy(& vga_charmap[0] + i * 32, &sdl_font8x16[i], 16);
         char_changed[i] = 1;
    }
     charmap_updated = 1;
    console.cursor_x = 0;
    console.cursor_y = 0;
    console.cursor_addr = 0;
    memset(&console.tminfo, 0, sizeof(bx_vga_tminfo_t));
    console.tminfo.line_offset = 160;
    console.tminfo.line_compare = 1023;
    console.tminfo.cs_start = 0x2e;
    console.tminfo.cs_end = 0x0f;
    console.tminfo.actl_palette[7] = 0x07;
    memcpy(console.saved_palette, palette, 32);
    palette_change_common(0x00, 0x00, 0x00, 0x00);
    palette_change_common(0x07, 0xa8, 0xa8, 0xa8);
    dimension_update(720, 400, 16, 9, 8);
    console.n_keys = 0;
    console.running = 1;
}

void console_cleanup(void)
{
    delete[] console.screen;
    delete[] console.oldscreen;
    palette_change_common(0x00, console.saved_palette[2], console.saved_palette[1],
        console.saved_palette[0]);
    palette_change_common(0x07, console.saved_palette[30], console.saved_palette[29],
        console.saved_palette[28]);
    unsigned fheight = (console.saved_fheight);
    unsigned fwidth = (console.saved_fwidth);
    set_text_charmap(console.saved_charmap);
    dimension_update(console.saved_xres, console.saved_yres, fheight, fwidth,
        console.saved_bpp);
    DEV_vga_refresh(1);
    console.running = 0;
}

void console_refresh(BOOL force)
{
    if (force) memset(console.oldscreen, 0xff, BX_CONSOLE_BUFSIZE);
    if ( new_text_api) {
        text_update_common(console.oldscreen, console.screen, console.cursor_addr,
            &console.tminfo);
    }
    else {
        text_update(console.oldscreen, console.screen, console.cursor_x,
            console.cursor_y, &console.tminfo);
    }
    flush();
    memcpy(console.oldscreen, console.screen, BX_CONSOLE_BUFSIZE);
}

void console_key_enq(BYTE key)
{
    if (console.n_keys < 16) {
        console.keys[console.n_keys++] = key;
    }
}

int bx_printf(const char* s)
{
    unsigned offset;

    if (!console.running) {
        console_init();
    }
    for (unsigned i = 0; i < strlen(s); i++) {
        offset = console.cursor_y * 160 + console.cursor_x * 2;
        if ((s[i] != 0x08) && (s[i] != 0x0a)) {
            console.screen[offset] = s[i];
            console.screen[offset + 1] = 0x07;
            console.cursor_x++;
        }
        if ((s[i] == 0x0a) || (console.cursor_x == 80)) {
            console.cursor_x = 0;
            console.cursor_y++;
        }
        if (s[i] == 0x08) {
            if (offset > 0) {
                console.screen[offset - 2] = 0x20;
                console.screen[offset - 1] = 0x07;
                if (console.cursor_x > 0) {
                    console.cursor_x--;
                }
                else {
                    console.cursor_x = 79;
                    console.cursor_y--;
                }
            }
        }
        if (console.cursor_y == 25) {
            memmove(console.screen, console.screen + 160, BX_CONSOLE_BUFSIZE - 160);
            console.cursor_y--;
            offset = console.cursor_y * 160 + console.cursor_x * 2;
            for (int j = 0; j < 160; j += 2) {
                console.screen[offset + j] = 0x20;
                console.screen[offset + j + 1] = 0x07;
            }
        }
    }
    console.cursor_addr = console.cursor_y * 160 + console.cursor_x * 2;
    console_refresh(0);
    return strlen(s);
}

char* bx_gets(char* s, int size)
{
    char keystr[2];
    int pos = 0, done = 0;
    int cs_counter = 1, cs_visible = 0;

    set_console_edit_mode(1);
    keystr[1] = 0;
    do {
        handle_events();
        while (console.n_keys > 0) {
            if ((console.keys[0] >= 0x20) && (pos < (size - 1))) {
                s[pos++] = console.keys[0];
                keystr[0] = console.keys[0];
                bx_printf(keystr);
            }
            else if (console.keys[0] == 0x0d) {
                s[pos] = 0x00;
                keystr[0] = 0x0a;
                bx_printf(keystr);
                done = 1;
            }
            else if ((console.keys[0] == 0x08) && (pos > 0)) {
                pos--;
                keystr[0] = 0x08;
                bx_printf(keystr);
            }
            memmove(&console.keys[0], &console.keys[1], 15);
            console.n_keys--;
        }
#if BX_HAVE_USLEEP
        usleep(25000);
#else
        msleep(25);
#endif
        if (--cs_counter == 0) {
            cs_counter = 10;
            cs_visible ^= 1;
            if (cs_visible) {
                console.tminfo.cs_start &= ~0x20;
            }
            else {
                console.tminfo.cs_start |= 0x20;
            }
            console_refresh(0);
        }
    } while (!done);
    console.tminfo.cs_start |= 0x20;
    set_console_edit_mode(0);
    return s;
}
#endif

void set_command_mode(BOOL active)
{
    if (command_mode.present) {
        command_mode.active = active;
    }
}

