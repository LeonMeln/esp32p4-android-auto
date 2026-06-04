/*
 * Host test for the config serializer. Compiles the generated tables + serdes
 * + buffer/crc from vesc_can on the host and verifies, for every baked table:
 *   1. the runtime crc32c signature equals the baked expected_signature
 *      (i.e. it will match the VESC firmware), and
 *   2. serializing the firmware defaults reproduces the byte stream the Python
 *      reference (tools/gen_vesc_config.py --self-test) produced — checked via
 *      the serialized size and the crc32c of the blob.
 *
 * Build & run:
 *   cc -std=c11 -I components/vesc_config/include -I components/vesc_can/include \
 *      tools/test/test_serdes.c components/vesc_config/vesc_config_serdes.c \
 *      components/vesc_config/generated/*.c \
 *      components/vesc_can/buffer.c components/vesc_can/crc.c -lm -o /tmp/test_serdes
 *   /tmp/test_serdes
 */
#include "vesc_config/vesc_config_types.h"
#include "vesc_config/vesc_config_serdes.h"
#include "vesc_can/crc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Expected crc32c of the default blob, from gen_vesc_config.py --self-test.
 * Keyed by (major,minor,kind). */
struct expect { uint8_t major, minor, kind; size_t size; uint32_t blob_crc; };
static const struct expect EXPECT[] = {
    { 6, 5, 0, 477, 0xB3E41F21 }, { 6, 5, 1, 278, 0x4E42B6D9 },
    { 6, 6, 0, 483, 0x4D343F5D }, { 6, 6, 1, 278, 0x4E42B6D9 },
    { 7, 0, 0, 488, 0x8807FED1 }, { 7, 0, 1, 284, 0x65F23401 },
};

static const struct expect *find_expect(const vc_table_t *t)
{
    for (size_t i = 0; i < sizeof EXPECT / sizeof EXPECT[0]; i++) {
        if (EXPECT[i].major == t->fw_major && EXPECT[i].minor == t->fw_minor &&
            EXPECT[i].kind == t->kind) {
            return &EXPECT[i];
        }
    }
    return NULL;
}

static int check_table(const vc_table_t *t, const char *label)
{
    int fails = 0;

    uint32_t sig = vesc_config_signature(t);
    if (sig != t->expected_signature) {
        printf("  FAIL %-16s signature 0x%08X != baked 0x%08X\n",
               label, sig, t->expected_signature);
        fails++;
    } else {
        printf("  ok   %-16s signature 0x%08X\n", label, sig);
    }

    vc_value_t *vals = calloc(t->param_count, sizeof(vc_value_t));
    uint8_t *buf = malloc(vesc_config_serialized_size(t) + 16);
    vesc_config_load_defaults(t, vals);
    size_t n = vesc_config_serialize(t, vals, buf, vesc_config_serialized_size(t) + 16);
    uint32_t blob_crc = crc32c(buf, (uint32_t)n);

    const struct expect *e = find_expect(t);
    if (!e) {
        printf("  WARN %-16s no expected entry\n", label);
    } else if (n != e->size || blob_crc != e->blob_crc) {
        printf("  FAIL %-16s defblob %zuB crc 0x%08X != expected %zuB 0x%08X\n",
               label, n, blob_crc, e->size, e->blob_crc);
        fails++;
    } else {
        printf("  ok   %-16s defblob %zuB crc 0x%08X\n", label, n, blob_crc);
    }

    /* Round-trip: deserialize the blob we just produced, re-serialize, compare. */
    vc_value_t *vals2 = calloc(t->param_count, sizeof(vc_value_t));
    if (!vesc_config_deserialize(t, buf, n, vals2)) {
        printf("  FAIL %-16s deserialize rejected own blob\n", label);
        fails++;
    } else {
        uint8_t *buf2 = malloc(n + 16);
        size_t n2 = vesc_config_serialize(t, vals2, buf2, n + 16);
        if (n2 != n || memcmp(buf, buf2, n) != 0) {
            printf("  FAIL %-16s round-trip mismatch\n", label);
            fails++;
        } else {
            printf("  ok   %-16s round-trip stable\n", label);
        }
        free(buf2);
    }

    free(vals2);
    free(vals);
    free(buf);
    return fails;
}

int main(void)
{
    int fails = 0;
    char label[32];
    for (size_t i = 0; i < g_vc_version_count; i++) {
        const vc_version_t *v = &g_vc_versions[i];
        snprintf(label, sizeof label, "%u.%02u mcconf", v->major, v->minor);
        fails += check_table(v->mcconf, label);
        snprintf(label, sizeof label, "%u.%02u appconf", v->major, v->minor);
        fails += check_table(v->appconf, label);
    }
    printf(fails ? "\nFAILED (%d)\n" : "\nALL PASS\n", fails);
    return fails ? 1 : 0;
}
