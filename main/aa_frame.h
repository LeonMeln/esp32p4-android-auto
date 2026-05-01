#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

/* Channel ids — matches f1x/aasdk/Messenger/ChannelId enum order. */
typedef enum {
    AA_CHANNEL_CONTROL      = 0,
    AA_CHANNEL_INPUT        = 1,
    AA_CHANNEL_SENSOR       = 2,
    AA_CHANNEL_VIDEO        = 3,
    AA_CHANNEL_MEDIA_AUDIO  = 4,
    AA_CHANNEL_SPEECH_AUDIO = 5,
    AA_CHANNEL_SYSTEM_AUDIO = 6,
    AA_CHANNEL_AV_INPUT     = 7,
    AA_CHANNEL_BLUETOOTH    = 8,
} aa_channel_id_t;

/* Control-channel message ids — see aasdk_proto/ControlMessageIdsEnum.proto. */
#define AA_MSG_VERSION_REQUEST   0x0001
#define AA_MSG_VERSION_RESPONSE  0x0002
#define AA_MSG_SSL_HANDSHAKE     0x0003
#define AA_MSG_AUTH_COMPLETE     0x0004

/* Frame flag bits (byte 1 of the header). aasdk encodes:
 *   frame_type | encryption_type | message_type
 * with FrameType::FIRST=1, LAST=2, BULK=3,
 * EncryptionType::ENCRYPTED=8, MessageType::CONTROL=4. */
#define AA_FRAME_FLAG_FIRST    0x01
#define AA_FRAME_FLAG_LAST     0x02
#define AA_FRAME_FLAG_BULK     (AA_FRAME_FLAG_FIRST | AA_FRAME_FLAG_LAST)
#define AA_FRAME_FLAG_CONTROL  0x04
#define AA_FRAME_FLAG_ENCRYPT  0x08

/* Encode-and-send a single BULK plaintext frame whose payload is
 * [msg_id BE16][body]. Suitable for control-channel handshake messages. */
esp_err_t aa_frame_send_plain(int sock, aa_channel_id_t channel,
                              uint16_t msg_id,
                              const uint8_t *body, size_t body_len);

/* Send a frame whose payload is already-formed (e.g. an encrypted blob).
 * Caller picks the flag byte: BULK | ENCRYPT | CONTROL etc. */
esp_err_t aa_frame_send_raw(int sock, aa_channel_id_t channel,
                            uint8_t flags,
                            const uint8_t *payload, size_t payload_len);

/* Read one frame from the socket. Returns the channel id, flag byte,
 * and payload (allocated into out_payload up to out_capacity bytes).
 * out_payload_len is set to the actual size.
 *
 * Handles:
 *   - BULK and LAST frames (2-byte size field)
 *   - FIRST frames (4-byte size field — encoded payload size + total size)
 *   - MIDDLE frames (2-byte size field)
 *
 * Returns ESP_ERR_NO_MEM if the frame doesn't fit in out_capacity. */
esp_err_t aa_frame_recv(int sock,
                        aa_channel_id_t *out_channel,
                        uint8_t *out_flags,
                        uint8_t *out_payload, size_t out_capacity,
                        size_t *out_payload_len);
