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
#include "clipboard.h"
#include "guac_clipboard.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <iconv.h>

#include <guacamole/client.h>
#include <guacamole/timestamp.h>
#include <rfb/rfbclient.h>

#ifdef ENABLE_PULSE
#include "pulse.h"
#endif

int vnc_guac_client_handle_messages(guac_client* client) {

    rfbClient* rfb_client = ((vnc_guac_client_data*) client->data)->rfb_client;

    /* Initially wait for messages */
    int wait_result = WaitForMessage(rfb_client, 1000000);
    guac_timestamp frame_start = guac_timestamp_current();
    while (wait_result > 0) {

        guac_timestamp frame_end;
        int frame_remaining;

        /* Handle any message received */
        if (!HandleRFBServerMessage(rfb_client)) {
            guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_ERROR, "Error handling message from VNC server.");
            return 1;
        }

        /* Calculate time remaining in frame */
        frame_end = guac_timestamp_current();
        frame_remaining = frame_start + GUAC_VNC_FRAME_DURATION - frame_end;

        /* Wait again if frame remaining */
        if (frame_remaining > 0)
            wait_result = WaitForMessage(rfb_client,
                    GUAC_VNC_FRAME_TIMEOUT*1000);
        else
            break;

    }

    /* If an error occurs, log it and fail */
    if (wait_result < 0) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_ERROR, "Connection closed.");
        return 1;
    }

    return 0;

}

int vnc_guac_client_mouse_handler(guac_client* client, int x, int y, int mask) {

    rfbClient* rfb_client = ((vnc_guac_client_data*) client->data)->rfb_client;

    SendPointerEvent(rfb_client, x, y, mask);

    return 0;
}

int vnc_guac_client_key_handler(guac_client* client, int keysym, int pressed) {

    rfbClient* rfb_client = ((vnc_guac_client_data*) client->data)->rfb_client;

    SendKeyEvent(rfb_client, keysym, pressed);

    return 0;
}

int vnc_guac_client_clipboard_handler(guac_client* client, guac_stream* stream, char* mimetype) {
    return guac_vnc_clipboard_handler(client, stream, mimetype);
}

int vnc_guac_client_blob_handler(guac_client* client, guac_stream* stream, void* data, int length) {
    return guac_vnc_clipboard_blob_handler(client, stream, data, length);
}

int vnc_guac_client_end_handler(guac_client* client, guac_stream* stream) {
    return guac_vnc_clipboard_end_handler(client, stream);
}

int vnc_guac_client_free_handler(guac_client* client) {

    vnc_guac_client_data* guac_client_data = (vnc_guac_client_data*) client->data;
    rfbClient* rfb_client = guac_client_data->rfb_client;

#ifdef ENABLE_PULSE
    /* If audio enabled, stop streaming */
    if (guac_client_data->audio_enabled)
        guac_pa_stop_stream(client);
#endif

    /* Free encodings string, if used */
    if (guac_client_data->encodings != NULL)
        free(guac_client_data->encodings);

    /* Free clipboard */
    guac_common_clipboard_free(guac_client_data->clipboard);

    /* Free generic data struct */
    free(client->data);

    /* Free memory not free'd by libvncclient's rfbClientCleanup() */
    if (rfb_client->frameBuffer != NULL) free(rfb_client->frameBuffer);
    if (rfb_client->raw_buffer != NULL) free(rfb_client->raw_buffer);
    if (rfb_client->rcSource != NULL) free(rfb_client->rcSource);

    /* Free VNC rfbClientData linked list (not free'd by rfbClientCleanup()) */
    while (rfb_client->clientData != NULL) {
        rfbClientData* next = rfb_client->clientData->next;
        free(rfb_client->clientData);
        rfb_client->clientData = next;
    }

    /* Clean up VNC client*/
    rfbClientCleanup(rfb_client);

    return 0;
}

