#include "vga.h"
#include "vgacore.h"
#include "gui.h"
#include "pci.h"

struct VGA Vga;
extern struct VgaCore VgaCore;

#define SET_TILE_UPDATED(xtile, ytile, Value)                          \
  do {                                                                        \
    if (((xtile) < VgaCore.num_x_tiles) && ((ytile) < VgaCore.num_y_tiles))   \
      VgaCore.vga_tile_updated[(xtile)+(ytile)* VgaCore.num_x_tiles] = Value; \
  } while (0)

#define MAKE_COLOUR(red, red_shiftfrom, red_shiftto, red_mask, \
                    green, green_shiftfrom, green_shiftto, green_mask, \
                    blue, blue_shiftfrom, blue_shiftto, blue_mask) \
( \
 ((((red_shiftto) > (red_shiftfrom)) ? \
  (red) << ((red_shiftto) - (red_shiftfrom)) : \
  (red) >> ((red_shiftfrom) - (red_shiftto))) & \
  (red_mask)) | \
 ((((green_shiftto) > (green_shiftfrom)) ? \
  (green) << ((green_shiftto) - (green_shiftfrom)) : \
  (green) >> ((green_shiftfrom) - (green_shiftto))) & \
  (green_mask)) | \
 ((((blue_shiftto) > (blue_shiftfrom)) ? \
  (blue) << ((blue_shiftto) - (blue_shiftfrom)) : \
  (blue) >> ((blue_shiftfrom) - (blue_shiftto))) & \
  (blue_mask)) \
)

// Only reference the array if the tile numbers are within the bounds
// of the array.  If out of bounds, return 0.
#define GET_TILE_UPDATED(xtile,ytile)                        \
  ((((xtile) < VgaCore.num_x_tiles) && ((ytile) < VgaCore.num_y_tiles))? \
     VgaCore.vga_tile_updated[(xtile)+(ytile) * VgaCore.num_x_tiles]      \
     : 0)



VOID VgaVbePortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length)
{

}

ULONG VgaVbePortIoReadHandler(ULONG64 Address, ULONG Length)
{
    
}



VOID VgaPortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length)
{
    if (Length == 2) {
        VgaPortIoWriteHandler(Address, Value & 0xff, 1);
        VgaPortIoWriteHandler(Address + 1, (Value >> 8) & 0xff, 1);

        return;
    }

    if ((Address >= 0x03b0) && (Address <= 0x03bf) &&
        (VgaCore.misc_output.color_emulation))
        return;
    if ((Address >= 0x03d0) && (Address <= 0x03df) &&
        (VgaCore.misc_output.color_emulation == 0))
        return;

    switch (Address) {
    case 0x03b5: /* CRTC Registers (monochrome emulation modes) */
    case 0x03d5: /* CRTC Registers (color emulation modes) */
        if (VgaCore.CRTC.address > 0x18) {
            return;
        }
        if (Value != VgaCore.CRTC.reg[VgaCore.CRTC.address]) {
            switch (VgaCore.CRTC.address) {
            case 0x13:
            case 0x14:
            case 0x17:
                if (!Vga.vbe.enabled || (Vga.vbe.bpp == VBE_DISPI_BPP_4)) {
                    // Line offset change
                    VgaCorePortIoWriteHandler(Address, Value, Length);
                }
                else {
                    VgaCore.CRTC.reg[VgaCore.CRTC.address] = Value;
                }
                break;
            default:
                VgaCorePortIoWriteHandler(Address, Value, Length);
            }
        }
        break;
    default:
        VgaCorePortIoWriteHandler(Address, Value, Length);
    }

}

ULONG VgaReadPciConfHandler(PCI* Pci, ULONG64 Address, ULONG Length)
{

}

VOID VgaWritePciConfHandler(PCI* Pci, ULONG64 Address, ULONG Value, ULONG Length)
{
    unsigned write_addr;
    BYTE new_value;

    if ((Address >= 0x14) && (Address < 0x30))
        return;

    for (unsigned i = 0; i < Length; i++) {
        write_addr = Address + i;
        new_value = (BYTE)(Value & 0xff);
        switch (write_addr) {
            case 0x04: // disallowing write to command
            case 0x06: // disallowing write to status lo-byte (is that expected?)
                break;
            default:
                Pci->conf[write_addr] = new_value;
        }
        Value >>= 8;
    }
}

void VgaUpdate(void)
{
    unsigned iHeight, iWidth;

    if (Vga.vbe.enabled) {
        /* no screen update necessary */
        if ((VgaCore.vga_mem_updated == 0) && VgaCore.graphics_ctrl.graphics_alpha)
            return;

        /* skip screen update when vga/video is disabled or the sequencer is in reset mode */
        if (!VgaCore.vga_enabled || !VgaCore.attribute_ctrl.video_enabled
            || !VgaCore.sequencer.reset2 || !VgaCore.sequencer.reset1
            || (VgaCore.sequencer.reg1 & 0x20))
            return;

        /* skip screen update if the vertical retrace is in progress
           (using 72 Hz vertical frequency) */
        /*
        if ((bx_virt_timer.time_usec(BX_VGA_THIS vsync_realtime) % 13888) < 70)
            return;
        */
        if (Vga.vbe.bpp != VBE_DISPI_BPP_4) {
            // specific VBE code display update code
            unsigned pitch;
            unsigned xc, yc, xti, yti;
            unsigned r, c, w, h;
            int i;
            unsigned long red, green, blue, colour;
            BYTE* vid_ptr, * vid_ptr2;
            BYTE* tile_ptr, * tile_ptr2;
            bx_svga_tileinfo_t info;
            BYTE dac_size = Vga.vbe.dac_8bit ? 8 : 6;

            iWidth = Vga.vbe.xres;
            iHeight = Vga.vbe.yres;
            pitch = VgaCore.line_offset;
            BYTE* disp_ptr = &VgaCore.memory[Vga.vbe.virtual_start];

            if (graphics_tile_info_common(&info)) {
                if (info.snapshot_mode) {
                    vid_ptr = disp_ptr;
                    tile_ptr = get_snapshot_buffer();
                    if (tile_ptr != NULL) {
                        for (yc = 0; yc < iHeight; yc++) {
                            memcpy(tile_ptr, vid_ptr, info.pitch);
                            vid_ptr += pitch;
                            tile_ptr += info.pitch;
                        }
                    }
                }
                else if (info.is_indexed) {
                    switch (Vga.vbe.bpp) {
                    case 4:
                    case 15:
                    case 16:
                    case 24:
                    case 32:
                        break;
                    case 8:
                        for (yc = 0, yti = 0; yc < iHeight; yc += Y_TILESIZE, yti++) {
                            for (xc = 0, xti = 0; xc < iWidth; xc += X_TILESIZE, xti++) {
                                if (GET_TILE_UPDATED(xti, yti)) {
                                    vid_ptr = disp_ptr + (yc * pitch + xc);
                                    tile_ptr = graphics_tile_get(xc, yc, &w, &h);
                                    for (r = 0; r < h; r++) {
                                        vid_ptr2 = vid_ptr;
                                        tile_ptr2 = tile_ptr;
                                        for (c = 0; c < w; c++) {
                                            colour = 0;
                                            for (i = 0; i < (int)Vga.vbe.bpp; i += 8) {
                                                colour |= *(vid_ptr2++) << i;
                                            }
                                            if (info.is_little_endian) {
                                                for (i = 0; i < info.bpp; i += 8) {
                                                    *(tile_ptr2++) = (BYTE)(colour >> i);
                                                }
                                            }
                                            else {
                                                for (i = info.bpp - 8; i > -8; i -= 8) {
                                                    *(tile_ptr2++) = (BYTE)(colour >> i);
                                                }
                                            }
                                        }
                                        vid_ptr += pitch;
                                        tile_ptr += info.pitch;
                                    }
                                    graphics_tile_update_in_place(xc, yc, w, h);
                                    SET_TILE_UPDATED(xti, yti, 0);
                                }
                            }
                        }
                        break;
                    }
                }
                else {
                    switch (Vga.vbe.bpp) {
                    case 4:
                        break;
                    case 8:
                        for (yc = 0, yti = 0; yc < iHeight; yc += Y_TILESIZE, yti++) {
                            for (xc = 0, xti = 0; xc < iWidth; xc += X_TILESIZE, xti++) {
                                if (GET_TILE_UPDATED(xti, yti)) {
                                    vid_ptr = disp_ptr + (yc * pitch + xc);
                                    tile_ptr = graphics_tile_get(xc, yc, &w, &h);
                                    for (r = 0; r < h; r++) {
                                        vid_ptr2 = vid_ptr;
                                        tile_ptr2 = tile_ptr;
                                        for (c = 0; c < w; c++) {
                                            colour = *(vid_ptr2++);
                                            colour = MAKE_COLOUR(
                                                VgaCore.pel.data[colour].red, dac_size, info.red_shift, info.red_mask,
                                                VgaCore.pel.data[colour].green, dac_size, info.green_shift, info.green_mask,
                                                VgaCore.pel.data[colour].blue, dac_size, info.blue_shift, info.blue_mask);
                                            if (info.is_little_endian) {
                                                for (i = 0; i < info.bpp; i += 8) {
                                                    *(tile_ptr2++) = (BYTE)(colour >> i);
                                                }
                                            }
                                            else {
                                                for (i = info.bpp - 8; i > -8; i -= 8) {
                                                    *(tile_ptr2++) = (BYTE)(colour >> i);
                                                }
                                            }
                                        }
                                        vid_ptr += pitch;
                                        tile_ptr += info.pitch;
                                    }
                                    graphics_tile_update_in_place(xc, yc, w, h);
                                    SET_TILE_UPDATED(xti, yti, 0);
                                }
                            }
                        }
                        break;
                    case 15:
                        for (yc = 0, yti = 0; yc < iHeight; yc += Y_TILESIZE, yti++) {
                            for (xc = 0, xti = 0; xc < iWidth; xc += X_TILESIZE, xti++) {
                                if (GET_TILE_UPDATED(xti, yti)) {
                                    vid_ptr = disp_ptr + (yc * pitch + (xc << 1));
                                    tile_ptr = graphics_tile_get(xc, yc, &w, &h);
                                    for (r = 0; r < h; r++) {
                                        vid_ptr2 = vid_ptr;
                                        tile_ptr2 = tile_ptr;
                                        for (c = 0; c < w; c++) {
                                            colour = *(vid_ptr2++);
                                            colour |= *(vid_ptr2++) << 8;
                                            colour = MAKE_COLOUR(
                                                colour & 0x001f, 5, info.blue_shift, info.blue_mask,
                                                colour & 0x03e0, 10, info.green_shift, info.green_mask,
                                                colour & 0x7c00, 15, info.red_shift, info.red_mask);
                                            if (info.is_little_endian) {
                                                for (i = 0; i < info.bpp; i += 8) {
                                                    *(tile_ptr2++) = (BYTE)(colour >> i);
                                                }
                                            }
                                            else {
                                                for (i = info.bpp - 8; i > -8; i -= 8) {
                                                    *(tile_ptr2++) = (BYTE)(colour >> i);
                                                }
                                            }
                                        }
                                        vid_ptr += pitch;
                                        tile_ptr += info.pitch;
                                    }
                                    graphics_tile_update_in_place(xc, yc, w, h);
                                    SET_TILE_UPDATED( xti, yti, 0);
                                }
                            }
                        }
                        break;
                    case 16:
                        for (yc = 0, yti = 0; yc < iHeight; yc += Y_TILESIZE, yti++) {
                            for (xc = 0, xti = 0; xc < iWidth; xc += X_TILESIZE, xti++) {
                                if (GET_TILE_UPDATED(xti, yti)) {
                                    vid_ptr = disp_ptr + (yc * pitch + (xc << 1));
                                    tile_ptr = graphics_tile_get(xc, yc, &w, &h);
                                    for (r = 0; r < h; r++) {
                                        vid_ptr2 = vid_ptr;
                                        tile_ptr2 = tile_ptr;
                                        for (c = 0; c < w; c++) {
                                            colour = *(vid_ptr2++);
                                            colour |= *(vid_ptr2++) << 8;
                                            colour = MAKE_COLOUR(
                                                colour & 0x001f, 5, info.blue_shift, info.blue_mask,
                                                colour & 0x07e0, 11, info.green_shift, info.green_mask,
                                                colour & 0xf800, 16, info.red_shift, info.red_mask);
                                            if (info.is_little_endian) {
                                                for (i = 0; i < info.bpp; i += 8) {
                                                    *(tile_ptr2++) = (BYTE)(colour >> i);
                                                }
                                            }
                                            else {
                                                for (i = info.bpp - 8; i > -8; i -= 8) {
                                                    *(tile_ptr2++) = (BYTE)(colour >> i);
                                                }
                                            }
                                        }
                                        vid_ptr += pitch;
                                        tile_ptr += info.pitch;
                                    }
                                    graphics_tile_update_in_place(xc, yc, w, h);
                                    SET_TILE_UPDATED( xti, yti, 0);
                                }
                            }
                        }
                        break;
                    case 24:
                        for (yc = 0, yti = 0; yc < iHeight; yc += Y_TILESIZE, yti++) {
                            for (xc = 0, xti = 0; xc < iWidth; xc += X_TILESIZE, xti++) {
                                if (GET_TILE_UPDATED(xti, yti)) {
                                    vid_ptr = disp_ptr + (yc * pitch + 3 * xc);
                                    tile_ptr = graphics_tile_get(xc, yc, &w, &h);
                                    for (r = 0; r < h; r++) {
                                        vid_ptr2 = vid_ptr;
                                        tile_ptr2 = tile_ptr;
                                        for (c = 0; c < w; c++) {
                                            blue = *(vid_ptr2++);
                                            green = *(vid_ptr2++);
                                            red = *(vid_ptr2++);
                                            colour = MAKE_COLOUR(
                                                red, 8, info.red_shift, info.red_mask,
                                                green, 8, info.green_shift, info.green_mask,
                                                blue, 8, info.blue_shift, info.blue_mask);
                                            if (info.is_little_endian) {
                                                for (i = 0; i < info.bpp; i += 8) {
                                                    *(tile_ptr2++) = (BYTE)(colour >> i);
                                                }
                                            }
                                            else {
                                                for (i = info.bpp - 8; i > -8; i -= 8) {
                                                    *(tile_ptr2++) = (BYTE)(colour >> i);
                                                }
                                            }
                                        }
                                        vid_ptr += pitch;
                                        tile_ptr += info.pitch;
                                    }
                                    graphics_tile_update_in_place(xc, yc, w, h);
                                    SET_TILE_UPDATED( xti, yti, 0);
                                }
                            }
                        }
                        break;
                    case 32:
                        for (yc = 0, yti = 0; yc < iHeight; yc += Y_TILESIZE, yti++) {
                            for (xc = 0, xti = 0; xc < iWidth; xc += X_TILESIZE, xti++) {
                                if (GET_TILE_UPDATED(xti, yti)) {
                                    vid_ptr = disp_ptr + (yc * pitch + (xc << 2));
                                    tile_ptr = graphics_tile_get(xc, yc, &w, &h);
                                    for (r = 0; r < h; r++) {
                                        vid_ptr2 = vid_ptr;
                                        tile_ptr2 = tile_ptr;
                                        for (c = 0; c < w; c++) {
                                            blue = *(vid_ptr2++);
                                            green = *(vid_ptr2++);
                                            red = *(vid_ptr2++);
                                            vid_ptr2++;
                                            colour = MAKE_COLOUR(
                                                red, 8, info.red_shift, info.red_mask,
                                                green, 8, info.green_shift, info.green_mask,
                                                blue, 8, info.blue_shift, info.blue_mask);
                                            if (info.is_little_endian) {
                                                for (i = 0; i < info.bpp; i += 8) {
                                                    *(tile_ptr2++) = (BYTE)(colour >> i);
                                                }
                                            }
                                            else {
                                                for (i = info.bpp - 8; i > -8; i -= 8) {
                                                    *(tile_ptr2++) = (BYTE)(colour >> i);
                                                }
                                            }
                                        }
                                        vid_ptr += pitch;
                                        tile_ptr += info.pitch;
                                    }
                                    graphics_tile_update_in_place(xc, yc, w, h);
                                    SET_TILE_UPDATED( xti, yti, 0);
                                }
                            }
                        }
                        break;
                    }
                }
                VgaCore.last_xres = iWidth;
                VgaCore.last_yres = iHeight;
                VgaCore.vga_mem_updated = 0;
            }
           
        }
        else {
            unsigned r, c, x, y;
            unsigned xc, yc, xti, yti;
            BYTE* plane[4];

            determine_screen_dimensions(&iHeight, &iWidth);
            if ((iWidth != VgaCore.last_xres) || (iHeight != VgaCore.last_yres) ||
                (VgaCore.last_bpp > 8)) {
                //dimension_update(iWidth, iHeight);
                VgaCore.last_xres = iWidth;
                VgaCore.last_yres = iHeight;
                VgaCore.last_bpp = 8;
            }

            plane[0] = &VgaCore.memory[0 << VBE_DISPI_4BPP_PLANE_SHIFT];
            plane[1] = &VgaCore.memory[1 << VBE_DISPI_4BPP_PLANE_SHIFT];
            plane[2] = &VgaCore.memory[2 << VBE_DISPI_4BPP_PLANE_SHIFT];
            plane[3] = &VgaCore.memory[3 << VBE_DISPI_4BPP_PLANE_SHIFT];

            for (yc = 0, yti = 0; yc < iHeight; yc += Y_TILESIZE, yti++) {
                for (xc = 0, xti = 0; xc < iWidth; xc += X_TILESIZE, xti++) {
                    if (GET_TILE_UPDATED(xti, yti)) {
                        for (r = 0; r < Y_TILESIZE; r++) {
                            y = yc + r;
                            if (VgaCore.y_doublescan) y >>= 1;
                            for (c = 0; c < X_TILESIZE; c++) {
                                x = xc + c;
                                VgaCore.tile[r * X_TILESIZE + c] =
                                    get_vga_pixel(x, y, Vga.vbe.virtual_start, 0xffff, 0, plane);
                            }
                        }
                        SET_TILE_UPDATED( xti, yti, 0);
                        graphics_tile_update_common(VgaCore.tile, xc, yc);
                    }
                }
            }
        }
    }
    else {
        VgaCoreUpdate();
    }
}


VOID VgaInitialize()
{
    int fd, ret;
    BYTE checksum = 0;
    ULONG Address, devFunc;

	memset(&Vga, '\0', sizeof(Vga));

    Vga.ddc.DCKhost = 1;
    Vga.ddc.DDAhost = 1;
    Vga.ddc.DDAmon = 1;
    Vga.ddc.ddc_stage = DDC_STAGE_STOP;
    Vga.ddc.ddc_ack = 1;
    Vga.ddc.ddc_rw = 1;
    Vga.ddc.edid_index = 0;
    Vga.ddc.ddc_mode = 1;
    if (Vga.ddc.ddc_mode == BX_DDC_MODE_BUILTIN) {
        memcpy(Vga.ddc.edid_data, vesa_EDID, 128);
        Vga.ddc.edid_extblock = 0;
    }

    Vga.ddc.edid_data[127] = 0;
    for (int i = 0; i < 128; i++) {
        checksum += Vga.ddc.edid_data[i];
    }
    if (checksum != 0) {
        Vga.ddc.edid_data[127] = (BYTE)-checksum;
    }

    Vga.vbe_present = 0;
    Vga.vbe.enabled = 0;
    Vga.vbe.dac_8bit = 0;
    Vga.vbe.ddc_enabled = 0;
    Vga.vbe.base_address = 0x0000;

    for (Address = 0x03B4; Address <= 0x03B5; Address++) {
        RegisterPortIoHandler(Address, (WritePortIoHandlerCallback)VgaPortIoWriteHandler, (ReadPortIoHandlerCallback)VgaCorePortIoReadHandler);

    }

    for (Address = 0x03BA; Address <= 0x03BA; Address++) {
        RegisterPortIoHandler(Address, (WritePortIoHandlerCallback)VgaPortIoWriteHandler, (ReadPortIoHandlerCallback)VgaCorePortIoReadHandler);

    }

    for (Address = 0x03C0; Address <= 0x03CF; Address++) {
        RegisterPortIoHandler(Address, (WritePortIoHandlerCallback)VgaPortIoWriteHandler, (ReadPortIoHandlerCallback)VgaCorePortIoReadHandler);

    }

    for (Address = 0x03D4; Address <= 0x03D5; Address++) {
        RegisterPortIoHandler(Address, (WritePortIoHandlerCallback)VgaPortIoWriteHandler, (ReadPortIoHandlerCallback)VgaCorePortIoReadHandler);

    }
    for (Address = 0x03DA; Address <= 0x03DA; Address++) {
        RegisterPortIoHandler(Address, (WritePortIoHandlerCallback)VgaPortIoWriteHandler, (ReadPortIoHandlerCallback)VgaCorePortIoReadHandler);

    }
    devFunc = PCI_DEVFUNC_TO_ADDRESS(2, 0);
    RegisterPciHandler(devFunc, VgaWritePciConfHandler, VgaReadPciConfHandler);
    InitPciConfig(devFunc, 0x1234, 0x1111, 0x00, 0x030000, 0x00, 0);

    for (Address = 0x1CE; Address <= 0x1CF; Address++) {
        RegisterPortIoHandler(Address, (WritePortIoHandlerCallback)VgaVbePortIoWriteHandler, (ReadPortIoHandlerCallback)VgaVbePortIoReadHandler);

    }
}
