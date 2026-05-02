#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

/* H.264 decode pipe for the AAP video channel.
 *
 * Owns a ring buffer + decoder task. AAP-side calls h264_pipe_push() with the
 * raw NAL bytes from each AVMediaWith{Timestamp}Indication; the decoder task
 * pulls them out, runs esp_h264_dec_process, and (currently) just logs the
 * decoded frame metadata. PPA + display sink will land in the next stage —
 * this header stays the same; only the internal task gets extended. */

esp_err_t h264_pipe_init(void);

/* Copy one AAP video payload (NAL units, Annex B framing) into the ring
 * buffer for async decoding. Drops on overflow rather than blocking — we
 * must not stall the AAP receive loop or phone will hit ack timeouts. */
void h264_pipe_push(const uint8_t *data, size_t len);
