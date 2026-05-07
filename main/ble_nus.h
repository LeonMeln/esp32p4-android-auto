#pragma once

#include <stdint.h>

#include "host/ble_gatt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Nordic UART Service UUIDs (VESC Tool / VESC Express convention).
 *   Service: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 *   RX     : 6E400002-...   (phone → head unit, write)
 *   TX     : 6E400003-...   (head unit → phone, notify) */
#define NUS_UUID_PREFIX_BE_BYTE 0x6E

/* Returns the GATT service definitions for ble_gatts_count_cfg /
 * ble_gatts_add_svcs. Pointer-to-array of ble_gatt_svc_def with a {0}
 * terminator. Stable for the lifetime of the program. */
const struct ble_gatt_svc_def *ble_nus_get_svcs(void);

/* GATT register callback — wires the TX value handle from the registration
 * event to internal state so we can notify on it later. Pass to
 * ble_hs_cfg.gatts_register_cb in ble_host. */
void ble_nus_gatts_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);

/* Tracking of the active connection — call from ble_host's GAP event hook
 * on connect / disconnect so notifications target the right peer. */
void ble_nus_on_connect(uint16_t conn_handle);
void ble_nus_on_disconnect(void);

/* Push a reassembled VESC payload back to the phone over the NUS TX
 * characteristic (frames it with packet_build_frame and chunks by MTU-3).
 * No-op if no peer is connected. Safe to call from comm_can's RX task
 * (NimBLE serializes through the host task internally). */
void ble_nus_forward_response(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif
