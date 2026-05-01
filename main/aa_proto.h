#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Tiny hand-coded protobuf encoder. Covers what we need for AA control
 * messages (varint, length-delimited, embedded message). Avoids pulling in
 * nanopb + a build step for .proto files. The decoder is even smaller —
 * we just need to scan optional string fields out of incoming messages.
 *
 * All writers append to buf[*pos] and advance *pos. Caller is responsible
 * for ensuring capacity (we treat the buffer as scratch and crash early
 * via abort() if it overflows). */

void pb_w_varint(uint8_t *buf, size_t cap, size_t *pos, uint64_t v);
void pb_w_tag(uint8_t *buf, size_t cap, size_t *pos, uint32_t field, uint8_t wire);
void pb_w_uint32(uint8_t *buf, size_t cap, size_t *pos, uint32_t field, uint32_t v);
void pb_w_uint64(uint8_t *buf, size_t cap, size_t *pos, uint32_t field, uint64_t v);
void pb_w_bool(uint8_t *buf, size_t cap, size_t *pos, uint32_t field, bool v);
void pb_w_bytes(uint8_t *buf, size_t cap, size_t *pos, uint32_t field,
                const uint8_t *data, size_t len);
void pb_w_string(uint8_t *buf, size_t cap, size_t *pos, uint32_t field, const char *s);

/* Embedded sub-message: caller fills sub_buf with the encoded sub-message
 * body, then calls this to wrap it as field. */
void pb_w_submsg(uint8_t *buf, size_t cap, size_t *pos, uint32_t field,
                 const uint8_t *sub_buf, size_t sub_len);

/* Decoder: walk fields in [data..data+len). For each field, the callback gets
 * the field number, wire type, and a pointer to the value bytes (varint OR
 * length-delimited payload). Returns next read offset, or 0 on parse error. */
typedef struct {
    uint32_t field;
    uint8_t  wire;
    const uint8_t *p;       /* points at the value */
    size_t   len;           /* for wire=2: payload length; for wire=0: bytes consumed by varint */
    uint64_t varint;        /* decoded value for wire=0 */
} pb_field_t;

/* Returns true and fills *out if a field was parsed. Advances *pos.
 * Returns false at end-of-buffer or on parse error. */
bool pb_read_field(const uint8_t *data, size_t len, size_t *pos, pb_field_t *out);
