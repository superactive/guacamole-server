/*
 * Copyright (C) 2013 Glyptodon LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config.h"

#include "client.h"
#include "convert.h"

#include <iconv.h>
#include <stdlib.h>
#include <syslog.h>
#include <time.h>

#include <cairo/cairo.h>
#include <rfb/rfbclient.h>
#include <guacamole/socket.h>
#include <guacamole/protocol.h>
#include <guacamole/client.h>

/* Define cairo_format_stride_for_width() if missing */
#ifndef HAVE_CAIRO_FORMAT_STRIDE_FOR_WIDTH
#define cairo_format_stride_for_width(format, width) (width*4)
#endif

void guac_vnc_cursor(rfbClient* client, int x, int y, int w, int h, int bpp) {

    guac_client* gc = rfbClientGetClientData(client, __GUAC_CLIENT);
    guac_socket* socket = gc->socket;
    vnc_guac_client_data* guac_client_data = (vnc_guac_client_data*) gc->data;
    const guac_layer* cursor_layer = guac_client_data->cursor;

    /* Cairo image buffer */
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, w);
    unsigned char* buffer = malloc(h*stride);
    unsigned char* buffer_row_current = buffer;
    cairo_surface_t* surface;

    /* VNC image buffer */
    unsigned int fb_stride = bpp * w;
    unsigned char* fb_row_current = client->rcSource;
    unsigned char* fb_mask = client->rcMask;

    int dx, dy;

    /* Copy image data from VNC client to RGBA buffer */
    for (dy = 0; dy<h; dy++) {

        unsigned int*  buffer_current;
        unsigned char* fb_current;
        
        /* Get current buffer row, advance to next */
        buffer_current      = (unsigned int*) buffer_row_current;
        buffer_row_current += stride;

        /* Get current framebuffer row, advance to next */
        fb_current      = fb_row_current;
        fb_row_current += fb_stride;

        for (dx = 0; dx<w; dx++) {

            unsigned char alpha, red, green, blue;
            unsigned int v;

            /* Read current pixel value */
            switch (bpp) {
                case 4:
                    v = *((unsigned int*)   fb_current);
                    break;

                case 2:
                    v = *((unsigned short*) fb_current);
                    break;

                default:
                    v = *((unsigned char*)  fb_current);
            }

            /* Translate mask to alpha */
            if (*(fb_mask++)) alpha = 0xFF;
            else              alpha = 0x00;

            /* Translate value to RGB */
            red   = (v >> client->format.redShift)   * 0x100 / (client->format.redMax  + 1);
            green = (v >> client->format.greenShift) * 0x100 / (client->format.greenMax+ 1);
            blue  = (v >> client->format.blueShift)  * 0x100 / (client->format.blueMax + 1);

            /* Output ARGB */
            if (guac_client_data->swap_red_blue)
                *(buffer_current++) = (alpha << 24) | (blue << 16) | (green << 8) | red;
            else
                *(buffer_current++) = (alpha << 24) | (red  << 16) | (green << 8) | blue;

            /* Next VNC pixel */
            fb_current += bpp;

        }
    }

    /* Send cursor data*/
    surface = cairo_image_surface_create_for_data(buffer, CAIRO_FORMAT_ARGB32, w, h, stride);
    
    guac_protocol_send_png(socket,
            GUAC_COMP_SRC, cursor_layer, 0, 0, surface);
    
    /* Update cursor */
    guac_protocol_send_cursor(socket, x, y, cursor_layer, 0, 0, w, h);
    
    /* Free surface */
    cairo_surface_destroy(surface);
    free(buffer);

    /* libvncclient does not free rcMask as it does rcSource */
    free(client->rcMask);
}

void guac_vnc_update(rfbClient* client, int x, int y, int w, int h) {

    guac_client* gc = rfbClientGetClientData(client, __GUAC_CLIENT);
    vnc_guac_client_data* guac_client_data = (vnc_guac_client_data*) gc->data;
    guac_socket* socket = gc->socket;

    int dx, dy;

    /* Cairo image buffer */
    int stride;
    unsigned char* buffer;
    unsigned char* buffer_row_current;
    cairo_surface_t* surface;

    /* VNC framebuffer */
    unsigned int bpp;
    unsigned int fb_stride;
    unsigned char* fb_row_current;

    /* Ignore extra update if already handled by copyrect */
    if (guac_client_data->copy_rect_used) {
        guac_client_data->copy_rect_used = 0;
        return;
    }

    /* Init Cairo buffer */
    stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, w);
    buffer = malloc(h*stride);
    buffer_row_current = buffer;

    bpp = client->format.bitsPerPixel/8;
    fb_stride = bpp * client->width;
    fb_row_current = client->frameBuffer + (y * fb_stride) + (x * bpp);

    /* Copy image data from VNC client to PNG */
    for (dy = y; dy<y+h; dy++) {

        unsigned int*  buffer_current;
        unsigned char* fb_current;
        
        /* Get current buffer row, advance to next */
        buffer_current      = (unsigned int*) buffer_row_current;
        buffer_row_current += stride;

        /* Get current framebuffer row, advance to next */
        fb_current      = fb_row_current;
        fb_row_current += fb_stride;

        for (dx = x; dx<x+w; dx++) {

            unsigned char red, green, blue;
            unsigned int v;

            switch (bpp) {
                case 4:
                    v = *((unsigned int*)   fb_current);
                    break;

                case 2:
                    v = *((unsigned short*) fb_current);
                    break;

                default:
                    v = *((unsigned char*)  fb_current);
            }

            /* Translate value to RGB */
            red   = (v >> client->format.redShift)   * 0x100 / (client->format.redMax  + 1);
            green = (v >> client->format.greenShift) * 0x100 / (client->format.greenMax+ 1);
            blue  = (v >> client->format.blueShift)  * 0x100 / (client->format.blueMax + 1);

            /* Output RGB */
            if (guac_client_data->swap_red_blue)
                *(buffer_current++) = (blue << 16) | (green << 8) | red;
            else
                *(buffer_current++) = (red  << 16) | (green << 8) | blue;

            fb_current += bpp;

        }
    }

    /* For now, only use default layer */
    surface = cairo_image_surface_create_for_data(buffer, CAIRO_FORMAT_RGB24, w, h, stride);

    guac_protocol_send_png(socket, GUAC_COMP_OVER, GUAC_DEFAULT_LAYER, x, y, surface);

    /* Free surface */
    cairo_surface_destroy(surface);
    free(buffer);

}

void guac_vnc_copyrect(rfbClient* client, int src_x, int src_y, int w, int h, int dest_x, int dest_y) {

    guac_client* gc = rfbClientGetClientData(client, __GUAC_CLIENT);
    guac_socket* socket = gc->socket;

    /* For now, only use default layer */
    guac_protocol_send_copy(socket,
                            GUAC_DEFAULT_LAYER, src_x,  src_y, w, h,
            GUAC_COMP_OVER, GUAC_DEFAULT_LAYER, dest_x, dest_y);

    ((vnc_guac_client_data*) gc->data)->copy_rect_used = 1;

}

char* guac_vnc_get_password(rfbClient* client) {
    guac_client* gc = rfbClientGetClientData(client, __GUAC_CLIENT);
    return ((vnc_guac_client_data*) gc->data)->password;
}

void guac_vnc_set_pixel_format(rfbClient* client, int color_depth) {
    switch(color_depth) {
        case 8:
            client->format.depth        = 8;
            client->format.bitsPerPixel = 8;
            client->format.blueShift    = 6;
            client->format.redShift     = 0;
            client->format.greenShift   = 3;
            client->format.blueMax      = 3;
            client->format.redMax       = 7;
            client->format.greenMax     = 7;
            break;

        case 16:
            client->format.depth        = 16;
            client->format.bitsPerPixel = 16;
            client->format.blueShift    = 0;
            client->format.redShift     = 11;
            client->format.greenShift   = 5;
            client->format.blueMax      = 0x1f;
            client->format.redMax       = 0x1f;
            client->format.greenMax     = 0x3f;
            break;

        case 24:
        case 32:
        default:
            client->format.depth        = 24;
            client->format.bitsPerPixel = 32;
            client->format.blueShift    = 0;
            client->format.redShift     = 16;
            client->format.greenShift   = 8;
            client->format.blueMax      = 0xff;
            client->format.redMax       = 0xff;
            client->format.greenMax     = 0xff;
    }
}

rfbBool guac_vnc_malloc_framebuffer(rfbClient* rfb_client) {

    guac_client* gc = rfbClientGetClientData(rfb_client, __GUAC_CLIENT);
    vnc_guac_client_data* guac_client_data = (vnc_guac_client_data*) gc->data;

    /* Send new size */
    guac_protocol_send_size(gc->socket,
            GUAC_DEFAULT_LAYER, rfb_client->width, rfb_client->height);

    /* Use original, wrapped proc */
    return guac_client_data->rfb_MallocFrameBuffer(rfb_client);
}

void guac_vnc_cut_text(rfbClient* client, const char* text, int textlen) {

    guac_client* gc = rfbClientGetClientData(client, __GUAC_CLIENT);
    guac_socket* socket = gc->socket;

    /* Convert ASCII character data to UTF-8 */
    char* utf8_text = convert("ISO_8859-1", "UTF-8", text);

    guac_protocol_send_clipboard(socket, utf8_text);

    free(utf8_text);

}

void guac_vnc_client_log_info(const char* format, ...) {

    va_list args;
    va_start(args, format);

    vsyslog(LOG_INFO, format, args);

    va_end(args);

}

void guac_vnc_client_log_error(const char* format, ...) {

    va_list args;
    va_start(args, format);

    vsyslog(LOG_ERR, format, args);

    va_end(args);

}

