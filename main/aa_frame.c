#include "aa_frame.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "lwip/sockets.h"

static const char *TAG = "aa_frame";

static int recv_exact(int sock, uint8_t *buf, size_t len)
{
    size_t got = 0;
    while (got < len) {
        int n = recv(sock, buf + got, len - got, 0);
        if (n == 0) {
            return 0;
        }
        if (n < 0) {
            ESP_LOGW(TAG, "recv errno %d", errno);
            return n;
        }
        got += n;
    }
    return got;
}

esp_err_t aa_frame_send_plain(int sock, aa_channel_id_t channel,
                              uint16_t msg_id,
                              const uint8_t *body, size_t body_len)
{
    /* Header(2) + size(2) + msg_id(2) + body */
    if (body_len > 0xFFFF - 2) {
        return ESP_ERR_INVALID_SIZE;
    }
    size_t payload_len = 2 + body_len;
    size_t total = 4 + payload_len;
    uint8_t hdr[4];
    hdr[0] = (uint8_t)channel;
    hdr[1] = AA_FRAME_FLAG_BULK;     /* PLAIN, SPECIFIC, BULK */
    hdr[2] = (uint8_t)(payload_len >> 8);
    hdr[3] = (uint8_t)(payload_len & 0xFF);

    /* Use a small stack buffer when possible to send the whole frame in one
     * write — keeps things simple and avoids head-of-line interleaving. */
    uint8_t stackbuf[512];
    uint8_t *frame = stackbuf;
    if (total > sizeof(stackbuf)) {
        frame = malloc(total);
        if (!frame) return ESP_ERR_NO_MEM;
    }
    memcpy(frame, hdr, 4);
    frame[4] = (uint8_t)(msg_id >> 8);
    frame[5] = (uint8_t)(msg_id & 0xFF);
    if (body_len) {
        memcpy(frame + 6, body, body_len);
    }
    int n = send(sock, frame, total, 0);
    if (frame != stackbuf) free(frame);
    if (n != (int)total) {
        ESP_LOGE(TAG, "send failed: %d errno %d", n, errno);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t aa_frame_recv(int sock,
                        aa_channel_id_t *out_channel,
                        uint8_t *out_flags,
                        uint8_t *out_payload, size_t out_capacity,
                        size_t *out_payload_len)
{
    uint8_t hdr[4];
    int n = recv_exact(sock, hdr, 4);
    if (n == 0) return ESP_ERR_INVALID_STATE;   /* peer closed */
    if (n < 0) return ESP_FAIL;

    aa_channel_id_t channel = (aa_channel_id_t)hdr[0];
    uint8_t flags = hdr[1];
    /* Reject FIRST/MIDDLE-only frames for now — handshake fits in one BULK. */
    if ((flags & AA_FRAME_FLAG_BULK) != AA_FRAME_FLAG_BULK) {
        ESP_LOGE(TAG, "multi-frame not supported (flags 0x%02x)", flags);
        return ESP_ERR_NOT_SUPPORTED;
    }
    size_t payload_len = ((size_t)hdr[2] << 8) | hdr[3];
    if (payload_len > out_capacity) {
        ESP_LOGE(TAG, "payload %u > capacity %u", (unsigned)payload_len, (unsigned)out_capacity);
        return ESP_ERR_NO_MEM;
    }
    if (payload_len) {
        n = recv_exact(sock, out_payload, payload_len);
        if (n == 0) return ESP_ERR_INVALID_STATE;
        if (n < 0) return ESP_FAIL;
    }
    if (out_channel) *out_channel = channel;
    if (out_flags) *out_flags = flags;
    if (out_payload_len) *out_payload_len = payload_len;
    return ESP_OK;
}
