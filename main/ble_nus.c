#include "ble_nus.h"

#include <string.h>

#include "esp_log.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_hs_mbuf.h"
#include "host/ble_uuid.h"

#include "vesc_can/comm_can.h"
#include "vesc_can/packet_parser.h"

static const char *TAG = "ble_nus";

/* NimBLE stores UUID-128 in little-endian byte order (least-significant
 * byte first). The on-the-wire UUID 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 * thus reverses to the byte sequence below; only byte[12] changes between
 * the service / RX / TX UUIDs (the trailing "01"/"02"/"03" of the prefix). */
#define NUS_UUID_TAIL_LE                                              \
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,                   \
    0x93, 0xF3, 0xA3, 0xB5, /* byte 12 below */ 0x00, 0x00, 0x40, 0x6E

static const ble_uuid128_t NUS_SVC_UUID = BLE_UUID128_INIT(
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E);

static const ble_uuid128_t NUS_RX_UUID = BLE_UUID128_INIT(
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E);

static const ble_uuid128_t NUS_TX_UUID = BLE_UUID128_INIT(
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E);

static uint16_t        s_tx_val_handle;
static uint16_t        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static packet_parser_t s_rx_parser;

static void rx_packet_complete(const uint8_t *payload, uint16_t len)
{
    if (len == 0) return;
    ESP_LOGI(TAG, "BLE→CAN cmd 0x%02X len=%u", payload[0], (unsigned)len);
    /* send=0 — VESC controller replies via CAN; comm_can's RX task wraps
     * the response into PROCESS_RX_BUFFER and the handler in main.c
     * fans it back to ble_nus_forward_response. */
    comm_can_send_buffer((uint8_t)CONFIG_VESC_CAN_TARGET_ID,
                         payload, len, 0);
}

static int nus_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)arg;
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
    }

    /* Flatten os_mbuf into a stack buffer one MTU's worth at a time. */
    uint8_t  buf[256];
    uint16_t out_len = 0;
    int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &out_len);
    if (rc != 0) {
        ESP_LOGW(TAG, "mbuf_to_flat rc=%d", rc);
        return BLE_ATT_ERR_UNLIKELY;
    }

    ESP_LOG_BUFFER_HEX_LEVEL(TAG, buf, out_len, ESP_LOG_DEBUG);

    for (uint16_t i = 0; i < out_len; i++) {
        packet_parser_process_byte(&s_rx_parser, buf[i], rx_packet_complete);
    }
    return 0;
}

static const struct ble_gatt_svc_def s_nus_svcs[] = {
    {
        .type            = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid            = &NUS_SVC_UUID.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid       = &NUS_RX_UUID.u,
                .access_cb  = nus_access_cb,
                .flags      = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid       = &NUS_TX_UUID.u,
                .access_cb  = nus_access_cb,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_tx_val_handle,
            },
            { 0 },
        },
    },
    { 0 },
};

const struct ble_gatt_svc_def *ble_nus_get_svcs(void)
{
    return s_nus_svcs;
}

void ble_nus_gatts_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    (void)arg;
    if (ctxt->op == BLE_GATT_REGISTER_OP_CHR) {
        char uuid_buf[BLE_UUID_STR_LEN];
        ble_uuid_to_str(ctxt->chr.chr_def->uuid, uuid_buf);
        ESP_LOGI(TAG, "registered char %s val_handle=%u",
                 uuid_buf, (unsigned)ctxt->chr.val_handle);
    }
}

void ble_nus_on_connect(uint16_t conn_handle)
{
    s_conn_handle = conn_handle;
    packet_parser_init(&s_rx_parser);
    ESP_LOGI(TAG, "peer connected, conn=%u", (unsigned)conn_handle);
}

void ble_nus_on_disconnect(void)
{
    ESP_LOGI(TAG, "peer disconnected");
    s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
}

void ble_nus_forward_response(const uint8_t *data, uint16_t len)
{
    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE || s_tx_val_handle == 0) {
        return;
    }
    if (len == 0) return;

    /* Frame the payload (start byte + len + crc + end). 600 covers the
     * 512-byte parser cap + framing. */
    uint8_t  framed[600];
    uint16_t framed_len = packet_build_frame(data, len, framed, sizeof(framed));
    if (framed_len == 0) return;

    /* Negotiated MTU minus the 3-byte ATT notify header is the per-PDU
     * payload limit. ble_att_mtu() returns 23 (BLE_ATT_MTU_DFLT) until
     * MTU exchange completes. */
    uint16_t mtu = ble_att_mtu(s_conn_handle);
    if (mtu < 23) mtu = 23;
    uint16_t chunk = (uint16_t)(mtu - 3);

    uint16_t off = 0;
    while (off < framed_len) {
        uint16_t this_chunk = (uint16_t)((framed_len - off > chunk)
                                             ? chunk : (framed_len - off));
        struct os_mbuf *txom = ble_hs_mbuf_from_flat(framed + off, this_chunk);
        if (!txom) {
            ESP_LOGW(TAG, "mbuf_from_flat OOM at off=%u", (unsigned)off);
            return;
        }
        int rc = ble_gatts_notify_custom(s_conn_handle, s_tx_val_handle, txom);
        if (rc != 0) {
            ESP_LOGW(TAG, "notify_custom rc=%d at off=%u", rc, (unsigned)off);
            /* On error mbuf is freed by NimBLE — bail. */
            return;
        }
        off += this_chunk;
    }
}
