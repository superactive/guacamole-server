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


#ifndef __GUAC_RDPSND_MESSAGES_H
#define __GUAC_RDPSND_MESSAGES_H

#include "config.h"

#include "rdpsnd_service.h"

#include <guacamole/audio.h>

/*
 * PDU Message Types
 */

/**
 * Close PDU
 */
#define SNDC_CLOSE         1

/**
 * WaveInfo PDU. This PDU is sent just before wave data is sent.
 */
#define SNDC_WAVE          2

/**
 * Wave Confirm PDU. This PDU is sent in response to the WaveInfo PDU,
 * confirming it has been received and played.
 */
#define SNDC_WAVECONFIRM   5

/**
 * Training PDU. This PDU is sent by the server occasionally and must be
 * responded to with another training PDU, similar to Guac's sync message.
 */
#define SNDC_TRAINING      6

/**
 * Server Audio Formats and Version PDU. This PDU is sent by the server to
 * advertise to the client which audio formats are supported.
 */
#define SNDC_FORMATS       7

/**
 * Quality Mode PDU. This PDU must be sent by the client to select an audio
 * quality mode if the server is at least version 6.
 */
#define SNDC_QUALITYMODE   12

/*
 * Quality Modes
 */

/**
 * Dynamic Quality. The server will choose the audio quality based on its
 * perception of latency.
 */
#define DYNAMIC_QUALITY    0x0000

/**
 * Medium Quality. The server prioritizes bandwidth over quality.
 */
#define MEDIUM_QUALITY     0x0001

/**
 * High Quality. The server prioritizes quality over bandwidth.
 */
#define HIGH_QUALITY       0x0002

/*
 * Capabilities
 */
#define TSSNDCAPS_ALIVE  1

/*
 * Sound Formats
 */
#define WAVE_FORMAT_PCM  1

/**
 * The header common to all RDPSND PDUs.
 */
typedef struct guac_rdpsnd_pdu_header {

    /**
     * The type of message represented by this PDU (SNDC_WAVE, etc.)
     */
    int message_type;

    /**
     * The size of the remainder of the message.
     */
    int body_size;

} guac_rdpsnd_pdu_header;

/**
 * Handler for the SNDC_FORMATS (Server Audio Formats and Version) PDU.
 */
void guac_rdpsnd_formats_handler(guac_rdpsndPlugin* rdpsnd,
        guac_audio_stream* audio, wStream* input_stream,
        guac_rdpsnd_pdu_header* header);

/**
 * Handler for the SNDC_TRAINING (Training) PDU.
 */
void guac_rdpsnd_training_handler(guac_rdpsndPlugin* rdpsnd,
        guac_audio_stream* audio, wStream* input_stream,
        guac_rdpsnd_pdu_header* header);

/**
 * Handler for the SNDC_WAVE (WaveInfo) PDU.
 */
void guac_rdpsnd_wave_info_handler(guac_rdpsndPlugin* rdpsnd,
        guac_audio_stream* audio, wStream* input_stream,
        guac_rdpsnd_pdu_header* header);

/**
 * Handler for the SNDWAV (Wave) PDU which follows any WaveInfo PDU.
 */
void guac_rdpsnd_wave_handler(guac_rdpsndPlugin* rdpsnd,
        guac_audio_stream* audio, wStream* input_stream,
        guac_rdpsnd_pdu_header* header);

/**
 * Handler for the SNDC_CLOSE (Close) PDU.
 */
void guac_rdpsnd_close_handler(guac_rdpsndPlugin* rdpsnd,
        guac_audio_stream* audio, wStream* input_stream,
        guac_rdpsnd_pdu_header* header);

#endif

