#include "aa_proto.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

static const char *TAG = "aa_pb";

static void check_cap(size_t cap, size_t need)
{
    if (need > cap) {
        ESP_LOGE(TAG, "buffer overflow: need %u, have %u", (unsigned)need, (unsigned)cap);
        abort();
    }
}

void pb_w_varint(uint8_t *buf, size_t cap, size_t *pos, uint64_t v)
{
    while (v > 0x7F) {
        check_cap(cap, *pos + 1);
        buf[(*pos)++] = (uint8_t)(v & 0x7F) | 0x80;
        v >>= 7;
    }
    check_cap(cap, *pos + 1);
    buf[(*pos)++] = (uint8_t)v;
}

void pb_w_tag(uint8_t *buf, size_t cap, size_t *pos, uint32_t field, uint8_t wire)
{
    pb_w_varint(buf, cap, pos, ((uint64_t)field << 3) | (wire & 0x7));
}

void pb_w_uint32(uint8_t *buf, size_t cap, size_t *pos, uint32_t field, uint32_t v)
{
    pb_w_tag(buf, cap, pos, field, 0);
    pb_w_varint(buf, cap, pos, v);
}

void pb_w_uint64(uint8_t *buf, size_t cap, size_t *pos, uint32_t field, uint64_t v)
{
    pb_w_tag(buf, cap, pos, field, 0);
    pb_w_varint(buf, cap, pos, v);
}

void pb_w_bool(uint8_t *buf, size_t cap, size_t *pos, uint32_t field, bool v)
{
    pb_w_uint32(buf, cap, pos, field, v ? 1 : 0);
}

void pb_w_bytes(uint8_t *buf, size_t cap, size_t *pos, uint32_t field,
                const uint8_t *data, size_t len)
{
    pb_w_tag(buf, cap, pos, field, 2);
    pb_w_varint(buf, cap, pos, len);
    check_cap(cap, *pos + len);
    if (len) memcpy(buf + *pos, data, len);
    *pos += len;
}

void pb_w_string(uint8_t *buf, size_t cap, size_t *pos, uint32_t field, const char *s)
{
    pb_w_bytes(buf, cap, pos, field, (const uint8_t *)s, strlen(s));
}

void pb_w_submsg(uint8_t *buf, size_t cap, size_t *pos, uint32_t field,
                 const uint8_t *sub_buf, size_t sub_len)
{
    pb_w_bytes(buf, cap, pos, field, sub_buf, sub_len);
}

static bool read_varint(const uint8_t *data, size_t len, size_t *pos, uint64_t *out)
{
    uint64_t v = 0;
    int shift = 0;
    while (*pos < len) {
        uint8_t b = data[(*pos)++];
        v |= ((uint64_t)(b & 0x7F)) << shift;
        if ((b & 0x80) == 0) {
            *out = v;
            return true;
        }
        shift += 7;
        if (shift > 63) return false;
    }
    return false;
}

bool pb_read_field(const uint8_t *data, size_t len, size_t *pos, pb_field_t *out)
{
    if (*pos >= len) return false;
    uint64_t tag;
    size_t start = *pos;
    if (!read_varint(data, len, pos, &tag)) return false;
    out->field = (uint32_t)(tag >> 3);
    out->wire  = (uint8_t)(tag & 0x7);

    if (out->wire == 0) {
        size_t v_start = *pos;
        if (!read_varint(data, len, pos, &out->varint)) return false;
        out->p   = data + v_start;
        out->len = *pos - v_start;
        return true;
    } else if (out->wire == 2) {
        uint64_t l;
        if (!read_varint(data, len, pos, &l)) return false;
        if (*pos + l > len) return false;
        out->p   = data + *pos;
        out->len = (size_t)l;
        *pos += (size_t)l;
        return true;
    } else if (out->wire == 5) { /* fixed32 */
        if (*pos + 4 > len) return false;
        out->p = data + *pos;
        out->len = 4;
        *pos += 4;
        return true;
    } else if (out->wire == 1) { /* fixed64 */
        if (*pos + 8 > len) return false;
        out->p = data + *pos;
        out->len = 8;
        *pos += 8;
        return true;
    }
    /* unsupported wire type */
    *pos = start;
    return false;
}
