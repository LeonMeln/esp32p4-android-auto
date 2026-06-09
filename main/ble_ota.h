#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* BLE OTA for the P4 main firmware.
 *
 * Lives on the NotifBridge GATT service (notif_bridge.c owns the chars and
 * routes writes here):
 *   OTA-CTRL CHR 7B4E4F00-...-0007  WRITE | NOTIFY  (phone↔P4 control)
 *   OTA-DATA CHR 7B4E4F00-...-0008  WRITE_NO_RSP     (phone→P4 firmware bytes)
 *
 * Wire protocol (mirror flutter-application/lib/firmware/ble_ota.dart):
 *
 *   CTRL write  (phone → P4):
 *     0x01 BEGIN : [op][u32 total_len LE][32B sha256]   start a transfer
 *     0x02 END   : [op]                                  finalise + reboot
 *     0x03 ABORT : [op]                                  cancel, free buffer
 *
 *   CTRL notify (P4 → phone), 5-byte frame [u8 status][u32 detail LE]:
 *     0x10 READY    detail=0            begin accepted — stream data now
 *     0x11 PROGRESS detail=bytes        liveness during receive / flash
 *     0x12 DONE     detail=0            committed, rebooting
 *     0x1F ERROR    detail=err_code     see OTA_ERR_* in ble_ota.c
 *
 *   DATA write  (phone → P4): raw firmware bytes, streamed sequentially
 *     after READY. The image is staged in PSRAM, then flushed to the spare
 *     OTA partition in one concentrated pass on END (same flow as ota_http.c),
 *     SHA-256-verified against the BEGIN digest, and booted.
 *
 * Backward compatible: firmware that omits these chars makes the app fall
 * back to the WiFi/HTTP path; the app probes for the chars at discovery. */

void ble_ota_init(void);

/* ---- notif_bridge wiring ---- */
void ble_ota_set_link(uint16_t conn_handle, uint16_t ctrl_val_handle);
void ble_ota_on_disconnect(void);

/* ---- routed from notif_bridge access_cb (NimBLE host task) ---- */
void ble_ota_ctrl_write(const uint8_t *data, uint16_t len);
void ble_ota_data_write(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif
