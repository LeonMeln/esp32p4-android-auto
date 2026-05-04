#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

/* H.264 decode pipe for the AAP video channel.
 *
 * Async with a 4-slot queue. Recv-loop calls h264_pipe_push() with NAL
 * bytes; the decoder task picks the queue up, decodes, presents on the
 * display, and only then calls the ack callback. Frames are never dropped —
 * if the queue is full, push blocks until the decoder makes room. That
 * occasionally back-pressures TCP, which gearhead resolves by entering
 * FRAMER_WRITER_SYNCHRONOUS_MODE (1-frame-1-ack pacing) — the regime that
 * matches our throughput anyway. */

typedef esp_err_t (*h264_pipe_ack_cb_t)(void *ctx);

esp_err_t h264_pipe_init(void);

/* Copy `data..data+len` into the decode queue and arrange for `ack_cb(ack_ctx)`
 * to be called from the decoder task right after the frame has been displayed.
 * Blocks if the queue is full — never drops. */
void h264_pipe_push(const uint8_t *data, size_t len,
                    h264_pipe_ack_cb_t ack_cb, void *ack_ctx);
