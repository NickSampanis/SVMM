#include "vgacore.h"
#include "gui.h"
#include "win32gui.h"
#include "timer.h"
#include "pci.h"


static const WORD charmap_offset[8] = {
  0x0000, 0x4000, 0x8000, 0xc000,
  0x2000, 0x6000, 0xa000, 0xe000
};

static const BYTE ccdat[16][4] = {
  { 0x00, 0x00, 0x00, 0x00 },
  { 0xff, 0x00, 0x00, 0x00 },
  { 0x00, 0xff, 0x00, 0x00 },
  { 0xff, 0xff, 0x00, 0x00 },
  { 0x00, 0x00, 0xff, 0x00 },
  { 0xff, 0x00, 0xff, 0x00 },
  { 0x00, 0xff, 0xff, 0x00 },
  { 0xff, 0xff, 0xff, 0x00 },
  { 0x00, 0x00, 0x00, 0xff },
  { 0xff, 0x00, 0x00, 0xff },
  { 0x00, 0xff, 0x00, 0xff },
  { 0xff, 0xff, 0x00, 0xff },
  { 0x00, 0x00, 0xff, 0xff },
  { 0xff, 0x00, 0xff, 0xff },
  { 0x00, 0xff, 0xff, 0xff },
  { 0xff, 0xff, 0xff, 0xff },
};

struct VgaCore VgaCore;

#define SET_TILE_UPDATED(xtile, ytile, Value)                          \
  do {                                                                        \
    if (((xtile) < VgaCore.num_x_tiles) && ((ytile) < VgaCore.num_y_tiles))   \
      VgaCore.vga_tile_updated[(xtile)+(ytile)* VgaCore.num_x_tiles] = Value; \
  } while (0)

// Only reference the array if the tile numbers are within the bounds
// of the array.  If out of bounds, return 0.
#define GET_TILE_UPDATED(xtile,ytile)                        \
  ((((xtile) < VgaCore.num_x_tiles) && ((ytile) < VgaCore.num_y_tiles))? \
     VgaCore.vga_tile_updated[(xtile)+(ytile) * VgaCore.num_x_tiles]      \
     : 0)


BYTE get_vga_pixel(WORD x, WORD y, WORD saddr, WORD lc, BOOL bs, BYTE * *plane)
{
    BYTE attribute, bit_no, palette_reg_val, DAC_regno;
    DWORD byte_offset;

    if (VgaCore.x_dotclockdiv2) x >>= 1;
    bit_no = 7 - (x % 8);
    if (y > lc) {
        byte_offset = x / 8 +
            ((y - lc - 1) * VgaCore.line_offset);
    }
    else {
        byte_offset = saddr + x / 8 +
            (y * VgaCore.line_offset);
    }
    attribute =
        (((plane[0][byte_offset] >> bit_no) & 0x01) << 0) |
        (((plane[1][byte_offset] >> bit_no) & 0x01) << 1) |
        (((plane[2][byte_offset] >> bit_no) & 0x01) << 2) |
        (((plane[3][byte_offset] >> bit_no) & 0x01) << 3);

    attribute &= VgaCore.attribute_ctrl.color_plane_enable;
    // undocumented feature ???: colors 0..7 high intensity, colors 8..15 blinking
    if (VgaCore.attribute_ctrl.mode_ctrl.blink_intensity) {
        if (bs) {
            attribute |= 0x08;
        }
        else {
            attribute ^= 0x08;
        }
    }
    palette_reg_val = VgaCore.attribute_ctrl.palette_reg[attribute];
    if (VgaCore.attribute_ctrl.mode_ctrl.internal_palette_size) {
        // use 4 lower bits from palette register
        // use 4 higher bits from color select register
        // 16 banks of 16-color registers
        DAC_regno = (palette_reg_val & 0x0f) |
            (VgaCore.attribute_ctrl.color_select << 4);
    }
    else {
        // use 6 lower bits from palette register
        // use 2 higher bits from color select register
        // 4 banks of 64-color registers
        DAC_regno = (palette_reg_val & 0x3f) |
            ((VgaCore.attribute_ctrl.color_select & 0x0c) << 4);
    }
    DAC_regno &= VgaCore.pel.mask;
    return DAC_regno;
}

BYTE VgaCoreGetVgaPixel(WORD x, WORD y, WORD saddr, WORD lc, BOOL bs, BYTE **plane)
{
    BYTE attribute, bit_no, palette_reg_val, DAC_regno;
    DWORD byte_offset;

    if (VgaCore.x_dotclockdiv2) x >>= 1;
    bit_no = 7 - (x % 8);
    if (y > lc) {
        byte_offset = x / 8 +
            ((y - lc - 1) * VgaCore.line_offset);
    }
    else {
        byte_offset = saddr + x / 8 +
            (y * VgaCore.line_offset);
    }
    attribute =
        (((plane[0][byte_offset] >> bit_no) & 0x01) << 0) |
        (((plane[1][byte_offset] >> bit_no) & 0x01) << 1) |
        (((plane[2][byte_offset] >> bit_no) & 0x01) << 2) |
        (((plane[3][byte_offset] >> bit_no) & 0x01) << 3);

    attribute &= VgaCore.attribute_ctrl.color_plane_enable;
    // undocumented feature ???: colors 0..7 high intensity, colors 8..15 blinking
    if (VgaCore.attribute_ctrl.mode_ctrl.blink_intensity) {
        if (bs) {
            attribute |= 0x08;
        }
        else {
            attribute ^= 0x08;
        }
    }
    palette_reg_val = VgaCore.attribute_ctrl.palette_reg[attribute];
    if (VgaCore.attribute_ctrl.mode_ctrl.internal_palette_size) {
        // use 4 lower bits from palette register
        // use 4 higher bits from color select register
        // 16 banks of 16-color registers
        DAC_regno = (palette_reg_val & 0x0f) |
            (VgaCore.attribute_ctrl.color_select << 4);
    }
    else {
        // use 6 lower bits from palette register
        // use 2 higher bits from color select register
        // 4 banks of 64-color registers
        DAC_regno = (palette_reg_val & 0x3f) |
            ((VgaCore.attribute_ctrl.color_select & 0x0c) << 4);
    }
    DAC_regno &= VgaCore.pel.mask;
    return DAC_regno;
}

void VgaCoreDetermineScreenDimensions(unsigned* piHeight, unsigned* piWidth)
{
    {
        int h, v;
#define AI VgaCore.CRTC.reg

        h = (AI[1] + 1) * 8;
        v = (AI[18] | ((AI[7] & 0x02) << 7) | ((AI[7] & 0x40) << 3)) + 1;
#undef AI

        if (VgaCore.x_dotclockdiv2)
            h <<= 1;
        if (VgaCore.ext_y_dblsize)
            v <<= 1;
        *piWidth = h;
        *piHeight = v;
    }
}

BOOL VgaCoreSkipUpdate(void)
{
    ULONG64 display_usec;

    /* handle clear screen request from the sequencer */
    if (VgaCore.sequencer.clear_screen) {
        clear_screen();
        VgaCore.sequencer.clear_screen = 0;
    }

    /* skip screen update when vga/video is disabled or the sequencer is in reset mode */
    if (!VgaCore.vga_enabled || !VgaCore.attribute_ctrl.video_enabled
        || (VgaCore.attribute_ctrl.mode_ctrl.graphics_alpha != VgaCore.graphics_ctrl.graphics_alpha)
        || !VgaCore.sequencer.reset2 || !VgaCore.sequencer.reset1
        || (VgaCore.sequencer.reg1 & 0x20))
        return 1;

    /* skip screen update if the vertical retrace is in progress */
    /*
    display_usec = bx_virt_timer.time_usec(VgaCore.vsync_realtime) % VgaCore.vtotal_usec;
    if ((display_usec > VgaCore.vrstart_usec) &&
        (display_usec < VgaCore.vrend_usec)) {
        return 1;
    }
    */
    return 0;
}

VOID VgaCoreUpdate(VOID)
{
    unsigned iHeight, iWidth;
    static unsigned cs_counter = 1;
    static BOOL cs_visible = 0;
    BOOL cs_toggle = 0;
    BYTE color;
    WORD x, y, start_addr;
    BYTE attribute, palette_reg_val, DAC_regno;
    WORD line_compare;
    BYTE* plane[4];
    unsigned bit_no, r, c;
    unsigned long byte_offset;
    unsigned xc, yc, xti, yti;

    if ((VgaCore.vga_mem_updated == 0) && (cs_counter > 0))
        return;
    if (cs_counter == 0) {
        cs_counter = VgaCore.blink_counter;
        if ((!VgaCore.graphics_ctrl.graphics_alpha) ||
            (VgaCore.attribute_ctrl.mode_ctrl.blink_intensity)) {
            cs_toggle = 1;
            cs_visible = !cs_visible;
        }
        else {
            if (VgaCore.vga_mem_updated == 0)
                return;
            cs_toggle = 0;
            cs_visible = 0;
        }
    }

    // fields that effect the way video memory is serialized into screen output:
    // GRAPHICS CONTROLLER:
    //   VgaCore.graphics_ctrl.shift_reg:
    //     0: output data in standard VGA format or CGA-compatible 640x200 2 color
    //        graphics mode (mode 6)
    //     1: output data in CGA-compatible 320x200 4 color graphics mode
    //        (modes 4 & 5)
    //     2: output data 8 bits at a time from the 4 bit planes
    //        (mode 13 and variants like modeX)

    // if (VgaCore.vga_mem_updated==0 || VgaCore.attribute_ctrl.video_enabled == 0)

    if (VgaCore.graphics_ctrl.graphics_alpha) {
        // Graphics mode
       

        start_addr = (VgaCore.CRTC.reg[0x0c] << 8) | VgaCore.CRTC.reg[0x0d];

        VgaCoreDetermineScreenDimensions(&iHeight, &iWidth);
        if ((iWidth != VgaCore.last_xres) || (iHeight != VgaCore.last_yres) ||
            (VgaCore.last_bpp > 8))
        {
            //dimension_update(iWidth, iHeight);
            VgaCore.last_xres = iWidth;
            VgaCore.last_yres = iHeight;
            VgaCore.last_bpp = 8;
        }

        if (VgaCoreSkipUpdate())
            return;

        switch (VgaCore.graphics_ctrl.shift_reg) {
        case 0: // interleaved shift
            

            if ((VgaCore.CRTC.reg[0x17] & 1) == 0) { // CGA 640x200x2

                for (yc = 0, yti = 0; yc < iHeight; yc += Y_TILESIZE, yti++) {
                    for (xc = 0, xti = 0; xc < iWidth; xc += X_TILESIZE, xti++) {
                        if (GET_TILE_UPDATED(xti, yti)) {
                            for (r = 0; r < Y_TILESIZE; r++) {
                                y = yc + r;
                                if (VgaCore.y_doublescan) y >>= 1;
                                for (c = 0; c < X_TILESIZE; c++) {

                                    x = xc + c;
                                    /* 0 or 0x2000 */
                                    byte_offset = start_addr + ((y & 1) << 13);
                                    /* to the start of the line */
                                    byte_offset += (320 / 4) * (y / 2);
                                    /* to the byte start */
                                    byte_offset += (x / 8);

                                    bit_no = 7 - (x % 8);
                                    palette_reg_val = (((VgaCore.memory[byte_offset]) >> bit_no) & 1);
                                    DAC_regno = VgaCore.attribute_ctrl.palette_reg[palette_reg_val];
                                    VgaCore.tile[r * X_TILESIZE + c] = DAC_regno;
                                }
                            }
                            SET_TILE_UPDATED(xti, yti, 0);
                            graphics_tile_update_common(VgaCore.tile, xc, yc);
                        }
                    }
                }
            }
            else { // output data in serial fashion with each display plane
                  // output on its associated serial output.  Standard EGA/VGA format

                plane[0] = &VgaCore.memory[0 << VgaCore.plane_shift];
                plane[1] = &VgaCore.memory[1 << VgaCore.plane_shift];
                plane[2] = &VgaCore.memory[2 << VgaCore.plane_shift];
                plane[3] = &VgaCore.memory[3 << VgaCore.plane_shift];
                line_compare = VgaCore.line_compare;
                if (VgaCore.y_doublescan)
                    line_compare >>= 1;

                for (yc = 0, yti = 0; yc < iHeight; yc += Y_TILESIZE, yti++) {
                    for (xc = 0, xti = 0; xc < iWidth; xc += X_TILESIZE, xti++) {
                        if (cs_toggle || GET_TILE_UPDATED(xti, yti)) {
                            for (r = 0; r < Y_TILESIZE; r++) {
                                y = yc + r;
                                if (VgaCore.y_doublescan) y >>= 1;
                                for (c = 0; c < X_TILESIZE; c++) {
                                    x = xc + c;
                                    VgaCore.tile[r * X_TILESIZE + c] =
                                        VgaCoreGetVgaPixel(x, y, start_addr, line_compare, cs_visible, plane);
                                }
                            }
                            SET_TILE_UPDATED(xti, yti, 0);
                            graphics_tile_update_common(VgaCore.tile, xc, yc);
                        }
                    }
                }
            }
            break; // case 0

        case 1: // output the data in a CGA-compatible 320x200 4 color graphics
                // mode.  (planar shift, modes 4 & 5)

          /* CGA 320x200x4 start */

            for (yc = 0, yti = 0; yc < iHeight; yc += Y_TILESIZE, yti++) {
                for (xc = 0, xti = 0; xc < iWidth; xc += X_TILESIZE, xti++) {
                    if (GET_TILE_UPDATED(xti, yti)) {
                        for (r = 0; r < Y_TILESIZE; r++) {
                            y = yc + r;
                            if (VgaCore.y_doublescan) y >>= 1;
                            for (c = 0; c < X_TILESIZE; c++) {

                                x = xc + c;
                                if (VgaCore.x_dotclockdiv2) x >>= 1;
                                /* 0 or 0x2000 */
                                byte_offset = start_addr + ((y & 1) << 13);
                                /* to the start of the line */
                                byte_offset += (320 / 4) * (y / 2);
                                /* to the byte start */
                                byte_offset += (x / 4);

                                attribute = 6 - 2 * (x % 4);
                                palette_reg_val = (VgaCore.memory[byte_offset]) >> attribute;
                                palette_reg_val &= 3;
                                DAC_regno = VgaCore.attribute_ctrl.palette_reg[palette_reg_val];
                                VgaCore.tile[r * X_TILESIZE + c] = DAC_regno;
                            }
                        }
                        SET_TILE_UPDATED(xti, yti, 0);
                        graphics_tile_update_common(VgaCore.tile, xc, yc);
                    }
                }
            }
            /* CGA 320x200x4 end */

            break; // case 1

        case 2: // output the data eight bits at a time from the 4 bit plane
                // (format for VGA mode 13 hex)
        case 3: // FIXME: is this really the same ???

            if (VgaCore.CRTC.reg[0x14] & 0x40) { // DW set: doubleword mode
                unsigned long pixely, pixelx, plane;


                for (yc = 0, yti = 0; yc < iHeight; yc += Y_TILESIZE, yti++) {
                    for (xc = 0, xti = 0; xc < iWidth; xc += X_TILESIZE, xti++) {
                        if (GET_TILE_UPDATED(xti, yti)) {
                            for (r = 0; r < Y_TILESIZE; r++) {
                                pixely = yc + r;
                                if (VgaCore.y_doublescan) pixely >>= 1;
                                for (c = 0; c < X_TILESIZE; c++) {
                                    pixelx = (xc + c) >> 1;
                                    plane = (pixelx % 4);
                                    byte_offset = start_addr + (plane * 65536) +
                                        (pixely * VgaCore.line_offset) + (pixelx & ~0x03);
                                    color = VgaCore.memory[byte_offset];
                                    VgaCore.tile[r * X_TILESIZE + c] = color;
                                }
                            }
                            SET_TILE_UPDATED(xti, yti, 0);
                            graphics_tile_update_common(VgaCore.tile, xc, yc);
                        }
                    }
                }
            }
            else if (VgaCore.CRTC.reg[0x17] & 0x40) { // B/W set: byte mode, modeX
                unsigned long pixely, pixelx, plane;

                for (yc = 0, yti = 0; yc < iHeight; yc += Y_TILESIZE, yti++) {
                    for (xc = 0, xti = 0; xc < iWidth; xc += X_TILESIZE, xti++) {
                        if (GET_TILE_UPDATED(xti, yti)) {
                            for (r = 0; r < Y_TILESIZE; r++) {
                                pixely = yc + r;
                                if (VgaCore.y_doublescan) pixely >>= 1;
                                for (c = 0; c < X_TILESIZE; c++) {
                                    pixelx = (xc + c) >> 1;
                                    plane = (pixelx % 4);
                                    byte_offset = (plane * 65536) +
                                        (pixely * VgaCore.line_offset)
                                        + (pixelx >> 2);
                                    color = VgaCore.memory[start_addr + byte_offset];
                                    VgaCore.tile[r * X_TILESIZE + c] = color;
                                }
                            }
                            SET_TILE_UPDATED(xti, yti, 0);
                            graphics_tile_update_common(VgaCore.tile, xc, yc);
                        }
                    }
                }
            }
            else { // word mode
                unsigned long pixely, pixelx, plane;

                for (yc = 0, yti = 0; yc < iHeight; yc += Y_TILESIZE, yti++) {
                    for (xc = 0, xti = 0; xc < iWidth; xc += X_TILESIZE, xti++) {
                        if (GET_TILE_UPDATED(xti, yti)) {
                            for (r = 0; r < Y_TILESIZE; r++) {
                                pixely = yc + r;
                                if (VgaCore.y_doublescan) pixely >>= 1;
                                for (c = 0; c < X_TILESIZE; c++) {
                                    pixelx = (xc + c) >> 1;
                                    plane = (pixelx % 4);
                                    byte_offset = (plane * 65536) +
                                        (pixely * VgaCore.line_offset)
                                        + ((pixelx >> 1) & ~0x01);
                                    color = VgaCore.memory[start_addr + byte_offset];
                                    VgaCore.tile[r * X_TILESIZE + c] = color;
                                }
                            }
                            SET_TILE_UPDATED(xti, yti, 0);
                            graphics_tile_update_common(VgaCore.tile, xc, yc);
                        }
                    }
                }
            }
            break; // case 2
        }

        VgaCore.vga_mem_updated = 0;
        return;
    }
    else {
        // text mode
        WORD cursor_address;
        bx_vga_tminfo_t tm_info;
        unsigned VDE, cols, rows, cWidth;
        BYTE MSL;

        tm_info.start_address = 2 * ((VgaCore.CRTC.reg[12] << 8) +
            VgaCore.CRTC.reg[13]);
      
        tm_info.cs_start = VgaCore.CRTC.reg[0x0a] & 0x3f;
        tm_info.cs_end = VgaCore.CRTC.reg[0x0b] & 0x1f;
        tm_info.line_offset = VgaCore.line_offset;
        tm_info.line_compare = VgaCore.line_compare;
        tm_info.h_panning = VgaCore.attribute_ctrl.horiz_pel_panning & 0x0f;
        tm_info.v_panning = VgaCore.CRTC.reg[0x08] & 0x1f;
        tm_info.line_graphics = VgaCore.attribute_ctrl.mode_ctrl.enable_line_graphics;
        tm_info.split_hpanning = VgaCore.attribute_ctrl.mode_ctrl.pixel_panning_compat;
        tm_info.blink_flags = 0;
        if (VgaCore.attribute_ctrl.mode_ctrl.blink_intensity) {
            tm_info.blink_flags |= BX_TEXT_BLINK_MODE;
            if (cs_toggle)
                tm_info.blink_flags |= BX_TEXT_BLINK_TOGGLE;
            if (cs_visible)
                tm_info.blink_flags |= BX_TEXT_BLINK_STATE;
        }
        if ((VgaCore.sequencer.reg1 & 0x01) == 0) {
            if (tm_info.h_panning >= 8)
                tm_info.h_panning = 0;
            else
                tm_info.h_panning++;
        }
        else {
            tm_info.h_panning &= 0x07;
        }
        for (int index = 0; index < 16; index++) {
            tm_info.actl_palette[index] =
                VgaCore.attribute_ctrl.palette_reg[index] & VgaCore.pel.mask;
        }

        // Vertical Display End: find out how many lines are displayed
        VDE = VgaCore.vertical_display_end;
        // Maximum Scan Line: height of character cell
        MSL = VgaCore.CRTC.reg[0x09] & 0x1f;
        cols = VgaCore.CRTC.reg[1] + 1;
        // workaround for update() calls before VGABIOS init
        if ((cols == 1) || (MSL == 0)) {
            cols = 80;
            MSL = 15;
        }
        if ((MSL == 1) && (VDE == 399)) {
            // emulated CGA graphics mode 160x100x16 colors
            MSL = 3;
        }
        rows = (VDE + 1) / (MSL + 1);
        if ((rows * tm_info.line_offset) > (1 << 17)) {
            return;
        }
        cWidth = ((VgaCore.sequencer.reg1 & 0x01) == 1) ? 8 : 9;
        if (VgaCore.x_dotclockdiv2) cWidth <<= 1;
        iWidth = cWidth * cols;
        iHeight = VDE + 1;
        if ((iWidth != VgaCore.last_xres) || (iHeight != VgaCore.last_yres) ||
            (cWidth != VgaCore.last_fw) || ((MSL + 1) != VgaCore.last_fh) ||
            (VgaCore.last_bpp > 8))
        {
            dimension_update(iWidth, iHeight, MSL + 1, cWidth, 0);
            VgaCore.last_xres = iWidth;
            VgaCore.last_yres = iHeight;
            VgaCore.last_fw = cWidth;
            VgaCore.last_fh = MSL + 1;
            VgaCore.last_bpp = 8;
        }
        if (VgaCoreSkipUpdate())
            return;

        // pass old text snapshot & new VGA memory contents
        cursor_address = 2 * ((VgaCore.CRTC.reg[0x0e] << 8) +
            VgaCore.CRTC.reg[0x0f]);
        if ((cursor_address < tm_info.start_address) ||
            (cursor_address > (tm_info.start_address + tm_info.line_offset * rows))) {
            cursor_address = 0xffff;
        }
        text_update_common(VgaCore.text_snapshot,
            VgaCore.memory,
            cursor_address, &tm_info);
        if (VgaCore.vga_mem_updated) {
            // screen updated, copy new VGA memory contents into text snapshot
            memcpy(VgaCore.text_snapshot, VgaCore.memory,
                tm_info.line_offset * rows + tm_info.start_address);
            VgaCore.vga_mem_updated = 0;
        }
    }

}

ULONG _VgaCoreMMIOReadHandler(ULONG64 Address, ULONG Length)
{
    DWORD offset;
    BYTE *plane0, *plane1, *plane2, *plane3;

    switch (VgaCore.graphics_ctrl.memory_mapping) {
    case 1: // 0xA0000 .. 0xAFFFF
        if (Address > 0xAFFFF) return 0xff;
        offset = Address & 0xFFFF;
        break;
    case 2: // 0xB0000 .. 0xB7FFF
        if ((Address < 0xB0000) || (Address > 0xB7FFF)) return 0xff;
        offset = Address & 0x7FFF;
        break;
    case 3: // 0xB8000 .. 0xBFFFF
        if (Address < 0xB8000) return 0xff;
        offset = Address & 0x7FFF;
        break;
    default: // 0xA0000 .. 0xBFFFF
        offset = Address & 0x1FFFF;
    }

    if (VgaCore.sequencer.chain_four) {
        // Mode 13h: 320 x 200 256 color mode: chained pixel representation
        return VgaCore.memory[(offset & ~0x03) + (offset % 4) * 65536];
    }

    plane0 = &VgaCore.memory[(0 << VgaCore.plane_shift) + VgaCore.ext_offset];
    plane1 = &VgaCore.memory[(1 << VgaCore.plane_shift) + VgaCore.ext_offset];
    plane2 = &VgaCore.memory[(2 << VgaCore.plane_shift) + VgaCore.ext_offset];
    plane3 = &VgaCore.memory[(3 << VgaCore.plane_shift) + VgaCore.ext_offset];

    /* Address between 0xA0000 and 0xAFFFF */
    switch (VgaCore.graphics_ctrl.read_mode) {
    case 0: /* read mode 0 */
        VgaCore.graphics_ctrl.latch[0] = plane0[offset];
        VgaCore.graphics_ctrl.latch[1] = plane1[offset];
        VgaCore.graphics_ctrl.latch[2] = plane2[offset];
        VgaCore.graphics_ctrl.latch[3] = plane3[offset];
        return(VgaCore.graphics_ctrl.latch[VgaCore.graphics_ctrl.read_map_select]);

    case 1: /* read mode 1 */
    {
        BYTE color_compare, color_dont_care;
        BYTE latch0, latch1, latch2, latch3, retval;

        color_compare = VgaCore.graphics_ctrl.color_compare & 0x0f;
        color_dont_care = VgaCore.graphics_ctrl.color_dont_care & 0x0f;
        latch0 = VgaCore.graphics_ctrl.latch[0] = plane0[offset];
        latch1 = VgaCore.graphics_ctrl.latch[1] = plane1[offset];
        latch2 = VgaCore.graphics_ctrl.latch[2] = plane2[offset];
        latch3 = VgaCore.graphics_ctrl.latch[3] = plane3[offset];

        latch0 ^= ccdat[color_compare][0];
        latch1 ^= ccdat[color_compare][1];
        latch2 ^= ccdat[color_compare][2];
        latch3 ^= ccdat[color_compare][3];

        latch0 &= ccdat[color_dont_care][0];
        latch1 &= ccdat[color_dont_care][1];
        latch2 &= ccdat[color_dont_care][2];
        latch3 &= ccdat[color_dont_care][3];

        retval = ~(latch0 | latch1 | latch2 | latch3);

        return retval;
    }
    default:
        return 0;
    }
}

VOID VgaCoreMMIOReadHandler(ULONG Address, BYTE* Data, ULONG Length)
{
    ULONG i;

    for (i = 0; i < Length; i++) {
        Data[i] = _VgaCoreMMIOReadHandler(Address + i, 1);

    }
}

void determine_screen_dimensions(unsigned* piHeight, unsigned* piWidth)
{
    int h, v;
#define AI VgaCore.CRTC.reg

    h = (AI[1] + 1) * 8;
    v = (AI[18] | ((AI[7] & 0x02) << 7) | ((AI[7] & 0x40) << 3)) + 1;
#undef AI

    if (VgaCore.x_dotclockdiv2) h <<= 1;
    if (VgaCore.ext_y_dblsize) v <<= 1;
    *piWidth = h;
    *piHeight = v;
}


VOID _VgaCoreMMIOWriteHandler(ULONG64 Address, ULONG Value, ULONG Length)
{
    DWORD offset;
    BYTE new_val[4] = { 0,0,0,0 };
    unsigned start_addr;
    BYTE* plane0, * plane1, * plane2, * plane3;
    unsigned x_tileno, x_tileno2, y_tileno, i;

    switch (VgaCore.graphics_ctrl.memory_mapping) {
    case 1: // 0xA0000 .. 0xAFFFF
        if ((Address < 0xA0000) || (Address > 0xAFFFF)) return;
        offset = (DWORD)Address - 0xA0000;
        break;
    case 2: // 0xB0000 .. 0xB7FFF
        if ((Address < 0xB0000) || (Address > 0xB7FFF)) return;
        offset = (DWORD)Address - 0xB0000;
        break;
    case 3: // 0xB8000 .. 0xBFFFF
        if ((Address < 0xB8000) || (Address > 0xBFFFF)) return;
        offset = (DWORD)Address - 0xB8000;
        break;
    default: // 0xA0000 .. 0xBFFFF
        if ((Address < 0xA0000) || (Address > 0xBFFFF)) return;
        offset = (DWORD)Address - 0xA0000;
    }

    start_addr = (VgaCore.CRTC.reg[0x0c] << 8) | VgaCore.CRTC.reg[0x0d];

    if (VgaCore.graphics_ctrl.graphics_alpha) {
        if (VgaCore.graphics_ctrl.memory_mapping == 3) { // 0xB8000 .. 0xBFFFF
            unsigned x_tileno, x_tileno2, y_tileno;

            /* CGA 320x200x4 / 640x200x2 start */
            VgaCore.memory[offset] = Value;
            offset -= start_addr;
            if (offset >= 0x2000) {
                y_tileno = offset - 0x2000;
                y_tileno /= (320 / 4);
                y_tileno <<= 1; //2 * y_tileno;
                y_tileno++;
                x_tileno = (offset - 0x2000) % (320 / 4);
                x_tileno <<= 2; //*= 4;
            }
            else {
                y_tileno = offset / (320 / 4);
                y_tileno <<= 1; //2 * y_tileno;
                x_tileno = offset % (320 / 4);
                x_tileno <<= 2; //*=4;
            }
            x_tileno2 = x_tileno;
            if (VgaCore.graphics_ctrl.shift_reg == 0) {
                x_tileno *= 2;
                x_tileno2 += 7;
            }
            else {
                x_tileno2 += 3;
            }
            if (VgaCore.x_dotclockdiv2) {
                x_tileno /= (X_TILESIZE / 2);
                x_tileno2 /= (X_TILESIZE / 2);
            }
            else {
                x_tileno /= X_TILESIZE;
                x_tileno2 /= X_TILESIZE;
            }
            if (VgaCore.y_doublescan) {
                y_tileno /= (Y_TILESIZE / 2);
            }
            else {
                y_tileno /= Y_TILESIZE;
            }
            VgaCore.vga_mem_updated = 1;
            SET_TILE_UPDATED(  x_tileno, y_tileno, 1);
            if (x_tileno2 != x_tileno) {
                SET_TILE_UPDATED(  x_tileno2, y_tileno, 1);
            }
            return;
            /* CGA 320x200x4 / 640x200x2 end */
        }
        /*
            else if (VgaCore.graphics_ctrl.memory_mapping != 1) {
              BX_PANIC(("mem_write: graphics: mapping = %u",
                       (unsigned) VgaCore.graphics_ctrl.memory_mapping));
              return;
            }
        */
        if (VgaCore.sequencer.chain_four) {
            unsigned x_tileno, y_tileno;

            // 320 x 200 256 color mode: chained pixel representation
            VgaCore.memory[(offset & ~0x03) + (offset % 4) * 65536] = Value;
            if (VgaCore.line_offset > 0) {
                offset -= start_addr;
                x_tileno = (offset % VgaCore.line_offset) / (X_TILESIZE / 2);
                if (VgaCore.y_doublescan) {
                    y_tileno = (offset / VgaCore.line_offset) / (Y_TILESIZE / 2);
                }
                else {
                    y_tileno = (offset / VgaCore.line_offset) / Y_TILESIZE;
                }
                VgaCore.vga_mem_updated = 1;
                SET_TILE_UPDATED(  x_tileno, y_tileno, 1);
            }
            return;
        }
    }

    /* Address between 0xA0000 and 0xAFFFF */

    plane0 = &VgaCore.memory[(0 << VgaCore.plane_shift) + VgaCore.ext_offset];
    plane1 = &VgaCore.memory[(1 << VgaCore.plane_shift) + VgaCore.ext_offset];
    plane2 = &VgaCore.memory[(2 << VgaCore.plane_shift) + VgaCore.ext_offset];
    plane3 = &VgaCore.memory[(3 << VgaCore.plane_shift) + VgaCore.ext_offset];

    switch (VgaCore.graphics_ctrl.write_mode) {
        case 0: /* write mode 0 */
        {
            const BYTE bitmask = VgaCore.graphics_ctrl.bitmask;
            const BYTE set_reset = VgaCore.graphics_ctrl.set_reset;
            const BYTE enable_set_reset = VgaCore.graphics_ctrl.enable_set_reset;
            /* perform rotate on CPU data in case its needed */
            if (VgaCore.graphics_ctrl.data_rotate) {
                Value = (Value >> VgaCore.graphics_ctrl.data_rotate) |
                    (Value << (8 - VgaCore.graphics_ctrl.data_rotate));
            }
            new_val[0] = VgaCore.graphics_ctrl.latch[0] & ~bitmask;
            new_val[1] = VgaCore.graphics_ctrl.latch[1] & ~bitmask;
            new_val[2] = VgaCore.graphics_ctrl.latch[2] & ~bitmask;
            new_val[3] = VgaCore.graphics_ctrl.latch[3] & ~bitmask;
            switch (VgaCore.graphics_ctrl.raster_op) {
                case 0: // replace
                    new_val[0] |= ((enable_set_reset & 1)
                        ? ((set_reset & 1) ? bitmask : 0)
                        : (Value & bitmask));
                    new_val[1] |= ((enable_set_reset & 2)
                        ? ((set_reset & 2) ? bitmask : 0)
                        : (Value & bitmask));
                    new_val[2] |= ((enable_set_reset & 4)
                        ? ((set_reset & 4) ? bitmask : 0)
                        : (Value & bitmask));
                    new_val[3] |= ((enable_set_reset & 8)
                        ? ((set_reset & 8) ? bitmask : 0)
                        : (Value & bitmask));
                    break;
                case 1: // AND
                    new_val[0] |= ((enable_set_reset & 1)
                        ? ((set_reset & 1)
                            ? (VgaCore.graphics_ctrl.latch[0] & bitmask)
                            : 0)
                        : (Value & VgaCore.graphics_ctrl.latch[0]) & bitmask);
                    new_val[1] |= ((enable_set_reset & 2)
                        ? ((set_reset & 2)
                            ? (VgaCore.graphics_ctrl.latch[1] & bitmask)
                            : 0)
                        : (Value & VgaCore.graphics_ctrl.latch[1]) & bitmask);
                    new_val[2] |= ((enable_set_reset & 4)
                        ? ((set_reset & 4)
                            ? (VgaCore.graphics_ctrl.latch[2] & bitmask)
                            : 0)
                        : (Value & VgaCore.graphics_ctrl.latch[2]) & bitmask);
                    new_val[3] |= ((enable_set_reset & 8)
                        ? ((set_reset & 8)
                            ? (VgaCore.graphics_ctrl.latch[3] & bitmask)
                            : 0)
                        : (Value & VgaCore.graphics_ctrl.latch[3]) & bitmask);
                    break;
                case 2: // OR
                    new_val[0]
                        |= ((enable_set_reset & 1)
                            ? ((set_reset & 1)
                                ? bitmask
                                : (VgaCore.graphics_ctrl.latch[0] & bitmask))
                            : ((Value | VgaCore.graphics_ctrl.latch[0]) & bitmask));
                    new_val[1]
                        |= ((enable_set_reset & 2)
                            ? ((set_reset & 2)
                                ? bitmask
                                : (VgaCore.graphics_ctrl.latch[1] & bitmask))
                            : ((Value | VgaCore.graphics_ctrl.latch[1]) & bitmask));
                    new_val[2]
                        |= ((enable_set_reset & 4)
                            ? ((set_reset & 4)
                                ? bitmask
                                : (VgaCore.graphics_ctrl.latch[2] & bitmask))
                            : ((Value | VgaCore.graphics_ctrl.latch[2]) & bitmask));
                    new_val[3]
                        |= ((enable_set_reset & 8)
                            ? ((set_reset & 8)
                                ? bitmask
                                : (VgaCore.graphics_ctrl.latch[3] & bitmask))
                            : ((Value | VgaCore.graphics_ctrl.latch[3]) & bitmask));
                    break;
                case 3: // XOR
                    new_val[0]
                        |= ((enable_set_reset & 1)
                            ? ((set_reset & 1)
                                ? (~VgaCore.graphics_ctrl.latch[0] & bitmask)
                                : (VgaCore.graphics_ctrl.latch[0] & bitmask))
                            : (Value ^ VgaCore.graphics_ctrl.latch[0]) & bitmask);
                    new_val[1]
                        |= ((enable_set_reset & 2)
                            ? ((set_reset & 2)
                                ? (~VgaCore.graphics_ctrl.latch[1] & bitmask)
                                : (VgaCore.graphics_ctrl.latch[1] & bitmask))
                            : (Value ^ VgaCore.graphics_ctrl.latch[1]) & bitmask);
                    new_val[2]
                        |= ((enable_set_reset & 4)
                            ? ((set_reset & 4)
                                ? (~VgaCore.graphics_ctrl.latch[2] & bitmask)
                                : (VgaCore.graphics_ctrl.latch[2] & bitmask))
                            : (Value ^ VgaCore.graphics_ctrl.latch[2]) & bitmask);
                    new_val[3]
                        |= ((enable_set_reset & 8)
                            ? ((set_reset & 8)
                                ? (~VgaCore.graphics_ctrl.latch[3] & bitmask)
                                : (VgaCore.graphics_ctrl.latch[3] & bitmask))
                            : (Value ^ VgaCore.graphics_ctrl.latch[3]) & bitmask);
                    break;
                default:
                    break;
        }
            break;
        }
        case 1: /* write mode 1 */
            for (i = 0; i < 4; i++) {
                new_val[i] = VgaCore.graphics_ctrl.latch[i];
            }
            break;

        case 2: /* write mode 2 */
        {
            const BYTE bitmask = VgaCore.graphics_ctrl.bitmask;

            new_val[0] = VgaCore.graphics_ctrl.latch[0] & ~bitmask;
            new_val[1] = VgaCore.graphics_ctrl.latch[1] & ~bitmask;
            new_val[2] = VgaCore.graphics_ctrl.latch[2] & ~bitmask;
            new_val[3] = VgaCore.graphics_ctrl.latch[3] & ~bitmask;
            switch (VgaCore.graphics_ctrl.raster_op) {
                case 0: // write
                    new_val[0] |= (Value & 1) ? bitmask : 0;
                    new_val[1] |= (Value & 2) ? bitmask : 0;
                    new_val[2] |= (Value & 4) ? bitmask : 0;
                    new_val[3] |= (Value & 8) ? bitmask : 0;
                    break;
                case 1: // AND
                    new_val[0] |= (Value & 1)
                        ? (VgaCore.graphics_ctrl.latch[0] & bitmask)
                        : 0;
                    new_val[1] |= (Value & 2)
                        ? (VgaCore.graphics_ctrl.latch[1] & bitmask)
                        : 0;
                    new_val[2] |= (Value & 4)
                        ? (VgaCore.graphics_ctrl.latch[2] & bitmask)
                        : 0;
                    new_val[3] |= (Value & 8)
                        ? (VgaCore.graphics_ctrl.latch[3] & bitmask)
                        : 0;
                    break;
                case 2: // OR
                    new_val[0] |= (Value & 1)
                        ? bitmask
                        : (VgaCore.graphics_ctrl.latch[0] & bitmask);
                    new_val[1] |= (Value & 2)
                        ? bitmask
                        : (VgaCore.graphics_ctrl.latch[1] & bitmask);
                    new_val[2] |= (Value & 4)
                        ? bitmask
                        : (VgaCore.graphics_ctrl.latch[2] & bitmask);
                    new_val[3] |= (Value & 8)
                        ? bitmask
                        : (VgaCore.graphics_ctrl.latch[3] & bitmask);
                    break;
                case 3: // XOR
                    new_val[0] |= (Value & 1)
                        ? (~VgaCore.graphics_ctrl.latch[0] & bitmask)
                        : (VgaCore.graphics_ctrl.latch[0] & bitmask);
                    new_val[1] |= (Value & 2)
                        ? (~VgaCore.graphics_ctrl.latch[1] & bitmask)
                        : (VgaCore.graphics_ctrl.latch[1] & bitmask);
                    new_val[2] |= (Value & 4)
                        ? (~VgaCore.graphics_ctrl.latch[2] & bitmask)
                        : (VgaCore.graphics_ctrl.latch[2] & bitmask);
                    new_val[3] |= (Value & 8)
                        ? (~VgaCore.graphics_ctrl.latch[3] & bitmask)
                        : (VgaCore.graphics_ctrl.latch[3] & bitmask);
                    break;
            }
        }
        break;

        case 3: /* write mode 3 */
        {
            const BYTE bitmask = VgaCore.graphics_ctrl.bitmask & Value;
            const BYTE set_reset = VgaCore.graphics_ctrl.set_reset;

            /* perform rotate on CPU data */
            if (VgaCore.graphics_ctrl.data_rotate) {
                Value = (Value >> VgaCore.graphics_ctrl.data_rotate) |
                    (Value << (8 - VgaCore.graphics_ctrl.data_rotate));
            }
            new_val[0] = VgaCore.graphics_ctrl.latch[0] & ~bitmask;
            new_val[1] = VgaCore.graphics_ctrl.latch[1] & ~bitmask;
            new_val[2] = VgaCore.graphics_ctrl.latch[2] & ~bitmask;
            new_val[3] = VgaCore.graphics_ctrl.latch[3] & ~bitmask;

            Value &= bitmask;

            switch (VgaCore.graphics_ctrl.raster_op) {
                case 0: // write
                    new_val[0] |= (set_reset & 1) ? Value : 0;
                    new_val[1] |= (set_reset & 2) ? Value : 0;
                    new_val[2] |= (set_reset & 4) ? Value : 0;
                    new_val[3] |= (set_reset & 8) ? Value : 0;
                    break;
                case 1: // AND
                    new_val[0] |= ((set_reset & 1) ? Value : 0)
                        & VgaCore.graphics_ctrl.latch[0];
                    new_val[1] |= ((set_reset & 2) ? Value : 0)
                        & VgaCore.graphics_ctrl.latch[1];
                    new_val[2] |= ((set_reset & 4) ? Value : 0)
                        & VgaCore.graphics_ctrl.latch[2];
                    new_val[3] |= ((set_reset & 8) ? Value : 0)
                        & VgaCore.graphics_ctrl.latch[3];
                    break;
                case 2: // OR
                    new_val[0] |= ((set_reset & 1) ? Value : 0)
                        | VgaCore.graphics_ctrl.latch[0];
                    new_val[1] |= ((set_reset & 2) ? Value : 0)
                        | VgaCore.graphics_ctrl.latch[1];
                    new_val[2] |= ((set_reset & 4) ? Value : 0)
                        | VgaCore.graphics_ctrl.latch[2];
                    new_val[3] |= ((set_reset & 8) ? Value : 0)
                        | VgaCore.graphics_ctrl.latch[3];
                    break;
                case 3: // XOR
                    new_val[0] |= ((set_reset & 1) ? Value : 0)
                        ^ VgaCore.graphics_ctrl.latch[0];
                    new_val[1] |= ((set_reset & 2) ? Value : 0)
                        ^ VgaCore.graphics_ctrl.latch[1];
                    new_val[2] |= ((set_reset & 4) ? Value : 0)
                        ^ VgaCore.graphics_ctrl.latch[2];
                    new_val[3] |= ((set_reset & 8) ? Value : 0)
                        ^ VgaCore.graphics_ctrl.latch[3];
                    break;
            }
        }
        break;

        default:
            break;
    }

    if (VgaCore.sequencer.map_mask & 0x0f) {
        VgaCore.vga_mem_updated = 1;
        if (VgaCore.sequencer.map_mask & 0x01)
            plane0[offset] = new_val[0];
        if (VgaCore.sequencer.map_mask & 0x02)
            plane1[offset] = new_val[1];
        if (VgaCore.sequencer.map_mask & 0x04) {
            if ((offset & 0xe000) == VgaCore.charmap_address) {
                set_text_charbyte((offset & 0x1fff), new_val[2]);
            }
            plane2[offset] = new_val[2];
        }
        if (VgaCore.sequencer.map_mask & 0x08)
            plane3[offset] = new_val[3];

        

        if (VgaCore.graphics_ctrl.shift_reg == 2) {
            offset -= start_addr;
            x_tileno = (offset % VgaCore.line_offset) * 4 / (X_TILESIZE / 2);
            if (VgaCore.y_doublescan) {
                y_tileno = (offset / VgaCore.line_offset) / (Y_TILESIZE / 2);
            }
            else {
                y_tileno = (offset / VgaCore.line_offset) / Y_TILESIZE;
            }
            SET_TILE_UPDATED(  x_tileno, y_tileno, 1);
        }
        else {
            if (VgaCore.line_compare < VgaCore.vertical_display_end) {
                if (VgaCore.line_offset > 0) {
                    if (VgaCore.x_dotclockdiv2) {
                        x_tileno = (offset % VgaCore.line_offset) / (X_TILESIZE / 16);
                    }
                    else {
                        x_tileno = (offset % VgaCore.line_offset) / (X_TILESIZE / 8);
                    }
                    if (VgaCore.y_doublescan) {
                        y_tileno = ((offset / VgaCore.line_offset) * 2 + VgaCore.line_compare + 1) / Y_TILESIZE;
                    }
                    else {
                        y_tileno = ((offset / VgaCore.line_offset) + VgaCore.line_compare + 1) / Y_TILESIZE;
                    }
                    SET_TILE_UPDATED(  x_tileno, y_tileno, 1);
                }
            }
            if (offset >= start_addr) {
                offset -= start_addr;
                if (VgaCore.line_offset > 0) {
                    if (VgaCore.x_dotclockdiv2) {
                        x_tileno = (offset % VgaCore.line_offset) / (X_TILESIZE / 16);
                    }
                    else {
                        x_tileno = (offset % VgaCore.line_offset) / (X_TILESIZE / 8);
                    }
                    if (VgaCore.y_doublescan) {
                        y_tileno = (offset / VgaCore.line_offset) / (Y_TILESIZE / 2);
                    }
                    else {
                        y_tileno = (offset / VgaCore.line_offset) / Y_TILESIZE;
                    }
                    SET_TILE_UPDATED(  x_tileno, y_tileno, 1);
                }
            }
        }
    }
}

void redraw_area(unsigned x0, unsigned y0, unsigned width, unsigned height)
{
    unsigned xti, yti, xt0, xt1, yt0, yt1, xmax, ymax;

    VgaCore.vga_mem_updated = 1;

    if (VgaCore.graphics_ctrl.graphics_alpha) {
        // graphics mode
        xmax = VgaCore.last_xres;
        ymax = VgaCore.last_yres;
        xt0 = x0 / X_TILESIZE;
        yt0 = y0 / Y_TILESIZE;
        if (x0 < xmax) {
            xt1 = (x0 + width - 1) / X_TILESIZE;
        }
        else {
            xt1 = (xmax - 1) / X_TILESIZE;
        }
        if (y0 < ymax) {
            yt1 = (y0 + height - 1) / Y_TILESIZE;
        }
        else {
            yt1 = (ymax - 1) / Y_TILESIZE;
        }
        for (yti = yt0; yti <= yt1; yti++) {
            for (xti = xt0; xti <= xt1; xti++) {
                SET_TILE_UPDATED(xti, yti, 1);
            }
        }

    }
    else {
        // text mode
        memset(VgaCore.text_snapshot, 0,
            sizeof(VgaCore.text_snapshot));
    }
}


void vga_redraw_area(unsigned x0, unsigned y0, unsigned width,
    unsigned height)
{
    if (width == 0 || height == 0) {
        return;
    }

    redraw_area(x0, y0, width, height);
}

void get_crtc_params(bx_crtc_params_t* crtcp, DWORD* vclock)
{
    *vclock = VgaCore.vclk[VgaCore.misc_output.clock_select];
    if (VgaCore.x_dotclockdiv2) *vclock >>= 1;
    crtcp->htotal = VgaCore.CRTC.reg[0] + 5;
    crtcp->vtotal = VgaCore.CRTC.reg[6] +
        ((VgaCore.CRTC.reg[7] & 0x01) << 8) +
        ((VgaCore.CRTC.reg[7] & 0x20) << 4) + 2;
    crtcp->vrstart = VgaCore.CRTC.reg[16] +
        ((VgaCore.CRTC.reg[7] & 0x04) << 6) +
        ((VgaCore.CRTC.reg[7] & 0x80) << 2);
}


void calculate_retrace_timing()
{
    DWORD hbstart, hbend, vclock = 0, cwidth, hfreq, vfreq, vrend;
    bx_crtc_params_t crtcp;

    get_crtc_params(&crtcp, &vclock);
    if (vclock == 0) {
        return;
    }
   
    cwidth = ((VgaCore.sequencer.reg1 & 0x01) == 1) ? 8 : 9;
    hfreq = vclock / (crtcp.htotal * cwidth);
    VgaCore.htotal_usec = 1000000 / hfreq;
    hbstart = VgaCore.CRTC.reg[2];
    VgaCore.hbstart_usec = (1000000 * hbstart * cwidth) / vclock;
    hbend = (VgaCore.CRTC.reg[3] & 0x1f) + ((VgaCore.CRTC.reg[5] & 0x80) >> 2);
    hbend = hbstart + ((hbend - hbstart) & 0x3f);
    VgaCore.hbend_usec = (1000000 * hbend * cwidth) / vclock;
    vrend = ((VgaCore.CRTC.reg[17] & 0x0f) - crtcp.vrstart) & 0x0f;
    vrend += crtcp.vrstart;
    vfreq = hfreq / crtcp.vtotal;
    VgaCore.vtotal_usec = 1000000 / vfreq;
    VgaCore.vblank_usec = VgaCore.htotal_usec * VgaCore.vertical_display_end;
    VgaCore.vrstart_usec = VgaCore.htotal_usec * crtcp.vrstart;
    VgaCore.vrend_usec = VgaCore.htotal_usec * vrend;
}

VOID VgaCorePortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length)
{
    BYTE charmap1, charmap2, prev_memory_mapping;
    BOOL prev_video_enabled, prev_line_graphics, prev_int_pal_size, prev_graphics_alpha;
    BOOL needs_update = 0, charmap_update = 0;


    if (Length == 2) {
        VgaCorePortIoWriteHandler(Address, Value & 0xff, 1);
        VgaCorePortIoWriteHandler(Address + 1, (Value >> 8) & 0xff, 1);
        return;
    }

    if ((Address >= 0x03b0) && (Address <= 0x03bf) &&
        (VgaCore.misc_output.color_emulation))
        return;
    if ((Address >= 0x03d0) && (Address <= 0x03df) &&
        (VgaCore.misc_output.color_emulation == 0))
        return;

    switch (Address) {
    case 0x03ba: /* Feature Control (monochrome emulation modes) */

        break;

    case 0x03c0: /* Attribute Controller */
        if (VgaCore.attribute_ctrl.flip_flop == 0) { /* Address mode */
            prev_video_enabled = VgaCore.attribute_ctrl.video_enabled;
            VgaCore.attribute_ctrl.video_enabled = (Value >> 5) & 0x01;

            if (VgaCore.attribute_ctrl.video_enabled == 0)
                clear_screen();
            else if (!prev_video_enabled) {

                needs_update = 1;
            }
            Value &= 0x1f; /* Address = bits 0..4 */
            VgaCore.attribute_ctrl.address = Value;
            switch (Value) {
            case 0x00: case 0x01: case 0x02: case 0x03:
            case 0x04: case 0x05: case 0x06: case 0x07:
            case 0x08: case 0x09: case 0x0a: case 0x0b:
            case 0x0c: case 0x0d: case 0x0e: case 0x0f:
                break;

            default:
                break;
            }
        }
        else { /* data-write mode */
            switch (VgaCore.attribute_ctrl.address) {
            case 0x00: case 0x01: case 0x02: case 0x03:
            case 0x04: case 0x05: case 0x06: case 0x07:
            case 0x08: case 0x09: case 0x0a: case 0x0b:
            case 0x0c: case 0x0d: case 0x0e: case 0x0f:
                if (Value != VgaCore.attribute_ctrl.palette_reg[VgaCore.attribute_ctrl.address]) {
                    VgaCore.attribute_ctrl.palette_reg[VgaCore.attribute_ctrl.address] =
                        Value;
                    needs_update = 1;
                }
                break;
            case 0x10: // mode control register
                prev_line_graphics = VgaCore.attribute_ctrl.mode_ctrl.enable_line_graphics;
                prev_int_pal_size = VgaCore.attribute_ctrl.mode_ctrl.internal_palette_size;
                VgaCore.attribute_ctrl.mode_ctrl.graphics_alpha =
                    (Value >> 0) & 0x01;
                VgaCore.attribute_ctrl.mode_ctrl.display_type =
                    (Value >> 1) & 0x01;
                VgaCore.attribute_ctrl.mode_ctrl.enable_line_graphics =
                    (Value >> 2) & 0x01;
                VgaCore.attribute_ctrl.mode_ctrl.blink_intensity =
                    (Value >> 3) & 0x01;
                VgaCore.attribute_ctrl.mode_ctrl.pixel_panning_compat =
                    (Value >> 5) & 0x01;
                VgaCore.attribute_ctrl.mode_ctrl.pixel_clock_select =
                    (Value >> 6) & 0x01;
                VgaCore.attribute_ctrl.mode_ctrl.internal_palette_size =
                    (Value >> 7) & 0x01;
                if (((Value >> 2) & 0x01) != prev_line_graphics) {
                    charmap_update = 1;
                }
                if (((Value >> 7) & 0x01) != prev_int_pal_size) {
                    needs_update = 1;
                }

                break;
            case 0x11: // Overscan Color Register
                VgaCore.attribute_ctrl.overscan_color = (Value & 0x3f);

                break;
            case 0x12: // Color Plane Enable Register
                VgaCore.attribute_ctrl.color_plane_enable = (Value & 0x0f);
                needs_update = 1;

                break;
            case 0x13: // Horizontal Pixel Panning Register
                VgaCore.attribute_ctrl.horiz_pel_panning = (Value & 0x0f);
                needs_update = 1;

                break;
            case 0x14: // Color Select Register
                VgaCore.attribute_ctrl.color_select = (Value & 0x0f);
                needs_update = 1;

                break;
            default:
                break;
            }
        }
        VgaCore.attribute_ctrl.flip_flop = !VgaCore.attribute_ctrl.flip_flop;
        break;

    case 0x03c2: // Miscellaneous Output Register
        VgaCore.misc_output.color_emulation = (Value >> 0) & 0x01;
        VgaCore.misc_output.enable_ram = (Value >> 1) & 0x01;
        VgaCore.misc_output.clock_select = (Value >> 2) & 0x03;
        VgaCore.misc_output.select_high_bank = (Value >> 5) & 0x01;
        VgaCore.misc_output.horiz_sync_pol = (Value >> 6) & 0x01;
        VgaCore.misc_output.vert_sync_pol = (Value >> 7) & 0x01;

        calculate_retrace_timing();
        break;

    case 0x03c3: // VGA enable
      // bit0: enables VGA display if set
        VgaCore.vga_enabled = Value & 0x01;

        break;

    case 0x03c4: /* Sequencer Index Register */

        VgaCore.sequencer.index = Value;
        break;

    case 0x03c5: /* Sequencer Registers 00..04 */
        switch (VgaCore.sequencer.index) {
        case 0: /* sequencer: reset */

            if (VgaCore.sequencer.reset1 && ((Value & 0x01) == 0)) {
                VgaCore.sequencer.char_map_select = 0;
                VgaCore.charmap_address = 0;
                charmap_update = 1;
            }
            VgaCore.sequencer.reset1 = (Value >> 0) & 0x01;
            VgaCore.sequencer.reset2 = (Value >> 1) & 0x01;
            break;
        case 1: /* sequencer: clocking mode */
            if ((Value ^ VgaCore.sequencer.reg1) & 0x29) {
                VgaCore.x_dotclockdiv2 = ((Value & 0x08) > 0);
                VgaCore.sequencer.clear_screen = ((Value & 0x20) > 0);
                calculate_retrace_timing();
                needs_update = 1;
            }
            VgaCore.sequencer.reg1 = Value & 0x3d;
            break;
        case 2: /* sequencer: map mask register */
            VgaCore.sequencer.map_mask = (Value & 0x0f);
            break;
        case 3: /* sequencer: character map select register */
            VgaCore.sequencer.char_map_select = Value & 0x3f;
            charmap1 = Value & 0x13;
            if (charmap1 > 3) 
                charmap1 = (charmap1 & 3) + 4;
            charmap2 = (Value & 0x2C) >> 2;
            if (charmap2 > 3)
                charmap2 = (charmap2 & 3) + 4;
            if (VgaCore.CRTC.reg[0x09] > 0) {
                VgaCore.charmap_address = charmap_offset[charmap1];
                charmap_update = 1;
            }
            break;
        case 4: /* sequencer: memory mode register */
            VgaCore.sequencer.extended_mem = (Value >> 1) & 0x01;
            VgaCore.sequencer.odd_even = (Value >> 2) & 0x01;
            VgaCore.sequencer.chain_four = (Value >> 3) & 0x01;
            break;
        default:
            break;
        }
        break;

    case 0x03c6: /* PEL mask */
        if (Value != VgaCore.pel.mask) {
            VgaCore.pel.mask = Value;
            needs_update = 1;
        }
        break;

    case 0x03c7: // PEL Address, read mode
        VgaCore.pel.read_data_register = Value;
        VgaCore.pel.read_data_cycle = 0;
        VgaCore.pel.dac_state = 0x03;
        break;

    case 0x03c8: /* PEL Address write mode */
        VgaCore.pel.write_data_register = Value;
        VgaCore.pel.write_data_cycle = 0;
        VgaCore.pel.dac_state = 0x00;
        break;

    case 0x03c9: /* PEL Data Register, colors 00..FF */
        switch (VgaCore.pel.write_data_cycle) {
        case 0:
            VgaCore.pel.data[VgaCore.pel.write_data_register].red = Value;
            break;
        case 1:
            VgaCore.pel.data[VgaCore.pel.write_data_register].green = Value;
            break;
        case 2:
            VgaCore.pel.data[VgaCore.pel.write_data_register].blue = Value;

            needs_update |= palette_change_common(VgaCore.pel.write_data_register,
                VgaCore.pel.data[VgaCore.pel.write_data_register].red << VgaCore.dac_shift,
                VgaCore.pel.data[VgaCore.pel.write_data_register].green << VgaCore.dac_shift,
                VgaCore.pel.data[VgaCore.pel.write_data_register].blue << VgaCore.dac_shift);
            break;
        }

        VgaCore.pel.write_data_cycle++;
        if (VgaCore.pel.write_data_cycle >= 3) {
            //BX_INFO(("VgaCore.pel.data[%u] {r=%u, g=%u, b=%u}",
            //  (unsigned) VgaCore.pel.write_data_register,
            //  (unsigned) VgaCore.pel.data[VgaCore.pel.write_data_register].red,
            //  (unsigned) VgaCore.pel.data[VgaCore.pel.write_data_register].green,
            //  (unsigned) VgaCore.pel.data[VgaCore.pel.write_data_register].blue);
            VgaCore.pel.write_data_cycle = 0;
            VgaCore.pel.write_data_register++;
        }
        break;

    case 0x03ca: /* Graphics 2 Position (EGA) */
      // ignore, EGA only???
        break;

    case 0x03cc: /* Graphics 1 Position (EGA) */
      // ignore, EGA only???
        break;

    case 0x03cd: /* ??? */
        break;

    case 0x03ce: /* Graphics Controller Index Register */
        VgaCore.graphics_ctrl.index = Value;
        break;

    case 0x03cf: /* Graphics Controller Registers 00..08 */
        switch (VgaCore.graphics_ctrl.index) {
        case 0: /* Set/Reset */
            VgaCore.graphics_ctrl.set_reset = Value & 0x0f;
            break;
        case 1: /* Enable Set/Reset */
            VgaCore.graphics_ctrl.enable_set_reset = Value & 0x0f;
            break;
        case 2: /* Color Compare */
            VgaCore.graphics_ctrl.color_compare = Value & 0x0f;
            break;
        case 3: /* Data Rotate */
            VgaCore.graphics_ctrl.data_rotate = Value & 0x07;
            VgaCore.graphics_ctrl.raster_op = (Value >> 3) & 0x03;
            break;
        case 4: /* Read Map Select */
            VgaCore.graphics_ctrl.read_map_select = Value & 0x03;

            break;
        case 5: /* Mode */
            VgaCore.graphics_ctrl.write_mode = Value & 0x03;
            VgaCore.graphics_ctrl.read_mode = (Value >> 3) & 0x01;
            VgaCore.graphics_ctrl.odd_even = (Value >> 4) & 0x01;
            VgaCore.graphics_ctrl.shift_reg = (Value >> 5) & 0x03;


            break;
        case 6: /* Miscellaneous */
            prev_graphics_alpha = VgaCore.graphics_ctrl.graphics_alpha;
            //        prev_chain_odd_even = VgaCore.graphics_ctrl.chain_odd_even;
            prev_memory_mapping = VgaCore.graphics_ctrl.memory_mapping;

            VgaCore.graphics_ctrl.graphics_alpha = Value & 0x01;
            VgaCore.graphics_ctrl.chain_odd_even = (Value >> 1) & 0x01;
            VgaCore.graphics_ctrl.memory_mapping = (Value >> 2) & 0x03;
            if (prev_memory_mapping != VgaCore.graphics_ctrl.memory_mapping)
                needs_update = 1;
            if (prev_graphics_alpha != VgaCore.graphics_ctrl.graphics_alpha) {
                needs_update = 1;
                VgaCore.last_yres = 0;
            }
            break;
        case 7: /* Color Don't Care */
            VgaCore.graphics_ctrl.color_dont_care = Value & 0x0f;
            break;
        case 8: /* Bit Mask */
            VgaCore.graphics_ctrl.bitmask = Value;
            break;
        default:
            break;
        }
        break;

    case 0x03b4: /* CRTC Index Register (monochrome emulation modes) */
    case 0x03d4: /* CRTC Index Register (color emulation modes) */
        VgaCore.CRTC.address = Value & 0x3f;

        break;

    case 0x03b5: /* CRTC Registers (monochrome emulation modes) */
    case 0x03d5: /* CRTC Registers (color emulation modes) */
        if (VgaCore.CRTC.address > 0x18) {
            return;
        }
        if (VgaCore.CRTC.write_protect && (VgaCore.CRTC.address < 0x08)) {
            if (VgaCore.CRTC.address == 0x07) {
                VgaCore.CRTC.reg[VgaCore.CRTC.address] &= ~0x10;
                VgaCore.CRTC.reg[VgaCore.CRTC.address] |= (Value & 0x10);
                VgaCore.line_compare &= 0x2ff;
                if (VgaCore.CRTC.reg[0x07] & 0x10) VgaCore.line_compare |= 0x100;
                needs_update = 1;
                break;
            }
            else {
                return;
            }
        }
        if (Value != VgaCore.CRTC.reg[VgaCore.CRTC.address]) {
            VgaCore.CRTC.reg[VgaCore.CRTC.address] = Value;
            switch (VgaCore.CRTC.address) {
            case 0x00:
            case 0x02:
            case 0x03:
            case 0x05:
            case 0x06:
            case 0x10:
                calculate_retrace_timing();
                break;
            case 0x07:
                VgaCore.vertical_display_end &= 0xff;
                if (VgaCore.CRTC.reg[0x07] & 0x02) VgaCore.vertical_display_end |= 0x100;
                if (VgaCore.CRTC.reg[0x07] & 0x40) VgaCore.vertical_display_end |= 0x200;
                VgaCore.line_compare &= 0x2ff;
                if (VgaCore.CRTC.reg[0x07] & 0x10) VgaCore.line_compare |= 0x100;
                calculate_retrace_timing();
                needs_update = 1;
                break;
            case 0x08:
                // Vertical pel panning change
                needs_update = 1;
                break;
            case 0x09:
                VgaCore.y_doublescan = ((Value & 0x9f) > 0);
                VgaCore.line_compare &= 0x1ff;
                if (VgaCore.CRTC.reg[0x09] & 0x40) VgaCore.line_compare |= 0x200;
                charmap_update = 1;
                needs_update = 1;
                break;
            case 0x0A:
            case 0x0B:
            case 0x0E:
            case 0x0F:
                // Cursor size / location change
                VgaCore.vga_mem_updated = 1;
                break;
            case 0x0C:
            case 0x0D:
                // Start Address change
                if (VgaCore.graphics_ctrl.graphics_alpha) {
                    needs_update = 1;
                }
                else {
                    VgaCore.vga_mem_updated = 1;
                }
                break;
            case 0x11:
                VgaCore.CRTC.write_protect = ((VgaCore.CRTC.reg[0x11] & 0x80) > 0);
                calculate_retrace_timing();
                break;
            case 0x12:
                VgaCore.vertical_display_end &= 0x300;
                VgaCore.vertical_display_end |= VgaCore.CRTC.reg[0x12];
                calculate_retrace_timing();
                break;
            case 0x13:
            case 0x14:
            case 0x17:
                // Line offset change
                VgaCore.line_offset = VgaCore.CRTC.reg[0x13] << 1;
                if (VgaCore.CRTC.reg[0x14] & 0x40) VgaCore.line_offset <<= 2;
                else if ((VgaCore.CRTC.reg[0x17] & 0x40) == 0) VgaCore.line_offset <<= 1;
                needs_update = 1;
                break;
            case 0x18:
                VgaCore.line_compare &= 0x300;
                VgaCore.line_compare |= VgaCore.CRTC.reg[0x18];
                needs_update = 1;
                break;
            }
        }
        break;

    case 0x03da: /* Feature Control (color emulation modes) */
        break;

    case 0x03c1: /* */
    default:
        break;
    }

    if (charmap_update) {
        set_text_charmap(
            &VgaCore.memory[0x20000 + VgaCore.charmap_address]);
        VgaCore.vga_mem_updated = 1;
    }
    if (needs_update) {
        // Mark all video as updated so the changes will go through
        vga_redraw_area(0, 0, VgaCore.last_xres, VgaCore.last_yres);
    }
}

ULONG VgaCorePortIoReadHandler(ULONG64 Address, ULONG Length)
{
    ULONG64 display_usec, line_usec;
    WORD ret16;
    BYTE retval;


    if (Length == 2) {
        ret16 = VgaCorePortIoReadHandler(Address, 1);
        ret16 |= (VgaCorePortIoReadHandler(Address + 1, 1)) << 8;
        return (ret16);
    }

    if ((Address >= 0x03b0) && (Address <= 0x03bf) &&
        (VgaCore.misc_output.color_emulation)) {
        return (0xff);
    }
    if ((Address >= 0x03d0) && (Address <= 0x03df) &&
        (VgaCore.misc_output.color_emulation == 0)) {
        return (0xff);
    }

    switch (Address) {
    case 0x03ba: /* Input Status 1 (monochrome emulation modes) */
    case 0x03ca: /* Feature Control ??? */
    case 0x03da: /* Input Status 1 (color emulation modes) */
      // bit3: Vertical Retrace
      //       0 = display is in the display mode
      //       1 = display is in the vertical retrace mode
      // bit0: Display Enable
      //       0 = display is in the display mode
      //       1 = display is not in the display mode; either the
      //           horizontal or vertical blanking period is active

        retval = 0;
        /*
        display_usec = bx_virt_timer.time_usec(BX_VGA_THIS vsync_realtime) % VgaCore.vtotal_usec;
        if ((display_usec >= VgaCore.vrstart_usec) &&
            (display_usec <= VgaCore.vrend_usec)) {
            retval |= 0x08;
        }
        if (display_usec >= VgaCore.vblank_usec) {
            retval |= 0x01;
        }
        else {
            line_usec = display_usec % VgaCore.htotal_usec;
            if ((line_usec >= VgaCore.hbstart_usec) &&
                (line_usec <= VgaCore.hbend_usec)) {
                retval |= 0x01;
            }
        }
        */

        /* reading this port resets the flip-flop to address mode */
        VgaCore.attribute_ctrl.flip_flop = 0;
        return (retval);
        break;

    case 0x03c0: /* */
        if (VgaCore.attribute_ctrl.flip_flop == 0) {
            //BX_INFO(("io read: 0x3c0: flip_flop = 0"));
            retval =
                (VgaCore.attribute_ctrl.video_enabled << 5) |
                VgaCore.attribute_ctrl.address;
            return (retval);
        }
        else {
            return(0);
        }
        break;

    case 0x03c1: /* */
        switch (VgaCore.attribute_ctrl.address) {
        case 0x00: case 0x01: case 0x02: case 0x03:
        case 0x04: case 0x05: case 0x06: case 0x07:
        case 0x08: case 0x09: case 0x0a: case 0x0b:
        case 0x0c: case 0x0d: case 0x0e: case 0x0f:
            retval = VgaCore.attribute_ctrl.palette_reg[VgaCore.attribute_ctrl.address];
            return (retval);
            break;
        case 0x10: /* mode control register */
            retval =
                (VgaCore.attribute_ctrl.mode_ctrl.graphics_alpha << 0) |
                (VgaCore.attribute_ctrl.mode_ctrl.display_type << 1) |
                (VgaCore.attribute_ctrl.mode_ctrl.enable_line_graphics << 2) |
                (VgaCore.attribute_ctrl.mode_ctrl.blink_intensity << 3) |
                (VgaCore.attribute_ctrl.mode_ctrl.pixel_panning_compat << 5) |
                (VgaCore.attribute_ctrl.mode_ctrl.pixel_clock_select << 6) |
                (VgaCore.attribute_ctrl.mode_ctrl.internal_palette_size << 7);
            return (retval);
            break;
        case 0x11: /* overscan color register */
            return (VgaCore.attribute_ctrl.overscan_color);
            break;
        case 0x12: /* color plane enable */
            return (VgaCore.attribute_ctrl.color_plane_enable);
            break;
        case 0x13: /* horizontal PEL panning register */
            return (VgaCore.attribute_ctrl.horiz_pel_panning);
            break;
        case 0x14: /* color select register */
            return (VgaCore.attribute_ctrl.color_select);
            break;
        default:

            return (0);
        }
        break;

    case 0x03c2: /* Input Status 0 */
        return (0);
        break;

    case 0x03c3: /* VGA Enable Register */
        return (VgaCore.vga_enabled);
        break;

    case 0x03c4: /* Sequencer Index Register */
        return (VgaCore.sequencer.index);
        break;

    case 0x03c5: /* Sequencer Registers 00..04 */
        switch (VgaCore.sequencer.index) {
        case 0: /* sequencer: reset */
            return ((BYTE)VgaCore.sequencer.reset1 | (VgaCore.sequencer.reset2 << 1));
            break;
        case 1: /* sequencer: clocking mode */
            return (VgaCore.sequencer.reg1);
            break;
        case 2: /* sequencer: map mask register */
            return (VgaCore.sequencer.map_mask);
            break;
        case 3: /* sequencer: character map select register */
            return (VgaCore.sequencer.char_map_select);
            break;
        case 4: /* sequencer: memory mode register */
            retval =
                (VgaCore.sequencer.extended_mem << 1) |
                (VgaCore.sequencer.odd_even << 2) |
                (VgaCore.sequencer.chain_four << 3);
            return (retval);
            break;

        default:
            return (0);
        }
        break;

    case 0x03c6: /* PEL mask ??? */
        return (VgaCore.pel.mask);
        break;

    case 0x03c7: /* DAC state, read = 11b, write = 00b */
        return (VgaCore.pel.dac_state);
        break;

    case 0x03c8: /* PEL address write mode */
        return (VgaCore.pel.write_data_register);
        break;

    case 0x03c9: /* PEL Data Register, colors 00..FF */
        if (VgaCore.pel.dac_state == 0x03) {
            switch (VgaCore.pel.read_data_cycle) {
            case 0:
                retval = VgaCore.pel.data[VgaCore.pel.read_data_register].red;
                break;
            case 1:
                retval = VgaCore.pel.data[VgaCore.pel.read_data_register].green;
                break;
            case 2:
                retval = VgaCore.pel.data[VgaCore.pel.read_data_register].blue;
                break;
            default:
                retval = 0; // keep compiler happy
            }
            VgaCore.pel.read_data_cycle++;
            if (VgaCore.pel.read_data_cycle >= 3) {
                VgaCore.pel.read_data_cycle = 0;
                VgaCore.pel.read_data_register++;
            }
        }
        else {
            retval = 0x3f;
        }
        return (retval);
        break;

    case 0x03cc: /* Miscellaneous Output / Graphics 1 Position ??? */
        retval =
            ((VgaCore.misc_output.color_emulation & 0x01) << 0) |
            ((VgaCore.misc_output.enable_ram & 0x01) << 1) |
            ((VgaCore.misc_output.clock_select & 0x03) << 2) |
            ((VgaCore.misc_output.select_high_bank & 0x01) << 5) |
            ((VgaCore.misc_output.horiz_sync_pol & 0x01) << 6) |
            ((VgaCore.misc_output.vert_sync_pol & 0x01) << 7);
        return (retval);
        break;

    case 0x03ce: /* Graphics Controller Index Register */
        return (VgaCore.graphics_ctrl.index);
        break;

    case 0x03cd: /* ??? */
        return (0x00);
        break;

    case 0x03cf: /* Graphics Controller Registers 00..08 */
        switch (VgaCore.graphics_ctrl.index) {
        case 0: /* Set/Reset */
            return (VgaCore.graphics_ctrl.set_reset);
            break;
        case 1: /* Enable Set/Reset */
            return (VgaCore.graphics_ctrl.enable_set_reset);
            break;
        case 2: /* Color Compare */
            return (VgaCore.graphics_ctrl.color_compare);
            break;
        case 3: /* Data Rotate */
            retval =
                ((VgaCore.graphics_ctrl.raster_op & 0x03) << 3) |
                ((VgaCore.graphics_ctrl.data_rotate & 0x07) << 0);
            return (retval);
            break;
        case 4: /* Read Map Select */
            return (VgaCore.graphics_ctrl.read_map_select);
            break;
        case 5: /* Mode */
            retval =
                ((VgaCore.graphics_ctrl.shift_reg & 0x03) << 5) |
                ((VgaCore.graphics_ctrl.odd_even & 0x01) << 4) |
                ((VgaCore.graphics_ctrl.read_mode & 0x01) << 3) |
                ((VgaCore.graphics_ctrl.write_mode & 0x03) << 0);

            if (VgaCore.graphics_ctrl.odd_even ||
                VgaCore.graphics_ctrl.shift_reg)
            return (retval);
            break;
        case 6: /* Miscellaneous */
            retval =
                ((VgaCore.graphics_ctrl.memory_mapping & 0x03) << 2) |
                ((VgaCore.graphics_ctrl.odd_even & 0x01) << 1) |
                ((VgaCore.graphics_ctrl.graphics_alpha & 0x01) << 0);
            return (retval);
            break;
        case 7: /* Color Don't Care */
            return (VgaCore.graphics_ctrl.color_dont_care);
            break;
        case 8: /* Bit Mask */
            return (VgaCore.graphics_ctrl.bitmask);
            break;
        default:
            /* ??? */
            return (0);
        }
        break;

    case 0x03d4: /* CRTC Index Register (color emulation modes) */
        return (VgaCore.CRTC.address);
        break;

    case 0x03b5: /* CRTC Registers (monochrome emulation modes) */
    case 0x03d5: /* CRTC Registers (color emulation modes) */
        if (VgaCore.CRTC.address == 0x22) {
            return VgaCore.graphics_ctrl.latch[VgaCore.graphics_ctrl.read_map_select];
        }
        if (VgaCore.CRTC.address > 0x18) {
            return (0);
        }
        return (VgaCore.CRTC.reg[VgaCore.CRTC.address]);
        break;

    case 0x03db: /* Ignore this address (16-bit read from 0x03da) */
        return (0); /* keep compiler happy */
        break;

    case 0x03b4: /* CRTC Index Register (monochrome emulation modes) */
    case 0x03cb: /* not sure but OpenBSD reads it a lot */
    default:
        return (0); /* keep compiler happy */
    }

}

VOID VgaCoreMMIOWriteHandler2(ULONG Address, BYTE* Data, ULONG Length)
{
    printf("writing %c\n", *Data);

}

VOID VgaCoreMMIOWriteHandler(ULONG Address, BYTE* Data, ULONG Length)
{
    ULONG i;

    for (i = 0; i < Length; i++) {
        _VgaCoreMMIOWriteHandler(Address + i, Data[i], 1);
    }
}

VOID VgaCoreTimerHandler()
{
	VgaCoreUpdate();
	flush();
}

VOID VgaCoreInitialize()
{
	int y, x, Address;

	memset(&VgaCore, '\0', sizeof(VgaCore));
	
    VgaCore.vga_enabled = 1;
	VgaCore.misc_output.color_emulation = 1;
	VgaCore.misc_output.enable_ram = 1;
	VgaCore.misc_output.horiz_sync_pol = 1;
	VgaCore.misc_output.vert_sync_pol = 1;

	VgaCore.attribute_ctrl.mode_ctrl.enable_line_graphics = 1;
	VgaCore.line_offset = 80;
	VgaCore.line_compare = 1023;
	VgaCore.vertical_display_end = 399;

	VgaCore.attribute_ctrl.video_enabled = 1;
	VgaCore.attribute_ctrl.color_plane_enable = 0x0f;
	VgaCore.pel.dac_state = 0x01;
	VgaCore.pel.mask = 0xff;
	VgaCore.graphics_ctrl.memory_mapping = 2; // monochrome text mode

	VgaCore.sequencer.reset1 = 1;
	VgaCore.sequencer.reset2 = 1;
	VgaCore.sequencer.extended_mem = 1; // display mem greater than 64K
	VgaCore.sequencer.odd_even = 1; // use sequential addressing mode

	VgaCore.plane_shift = 16;
	VgaCore.dac_shift = 2;
	VgaCore.last_bpp = 8;
	VgaCore.vclk[0] = 25175000;
	VgaCore.vclk[1] = 28322000;
	VgaCore.htotal_usec = 31;
	VgaCore.vtotal_usec = 14285;
	VgaCore.max_xres = 800;
	VgaCore.max_yres = 600;
	VgaCore.vga_override = 0;
	VgaCore.blink_counter = 1;
	VgaCore.memsize = 0x40000;
	VgaCore.memory = calloc(1, VgaCore.memsize);

    VgaInitialize();
	GuiInitialize(0, NULL, 0x320, 0x258, 0x10, 0x18);

	//RegisterMMIO
    RegisterMMIOHandler(0xa0000, VgaCoreMMIOWriteHandler, VgaCoreMMIOReadHandler);
    RegisterMMIOHandler(0xb0000, VgaCoreMMIOWriteHandler, VgaCoreMMIOReadHandler);
    //RegisterMMIOHandler(0x110000, VgaCoreMMIOWriteHandler2, VgaCoreMMIOReadHandler);

    //0x30d40 
	//TimerRegister(MSECONDS_TO_NS(20), VgaCoreTimerHandler, NULL);
    TimerRegister(TICK_PERIOD, VgaCoreTimerHandler, NULL);

	VgaCore.num_x_tiles = VgaCore.max_xres / X_TILESIZE +
		((VgaCore.max_xres % X_TILESIZE) > 0);
	VgaCore.num_y_tiles = VgaCore.max_yres / Y_TILESIZE +
		((VgaCore.max_yres % Y_TILESIZE) > 0);
	VgaCore.vga_tile_updated = calloc(sizeof(BOOL), VgaCore.num_x_tiles * VgaCore.num_y_tiles);
	

	for (y = 0; y < VgaCore.num_y_tiles; y++) {
		for (x = 0; x < VgaCore.num_x_tiles; x++) {
			if (y < VgaCore.num_y_tiles)
				VgaCore.vga_tile_updated[x + y * VgaCore.num_x_tiles] = FALSE;
		}
	}
}