#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* On boot, after bt_link_init has the agent up:
 *   1. Wait CONFIG_BT_AGENT_VERSION_TIMEOUT_MS for a BT-VER:<v> line.
 *   2. If v == CONFIG_BT_AGENT_FW_VERSION → ESP_OK, no action.
 *   3. Otherwise (mismatch, or no version line at all): tear down bt_link
 *      UART, force agent into ROM bootloader, flash the embedded blob,
 *      reset agent back to app, reattach bt_link UART.
 *
 * No-op if CONFIG_BT_AGENT_OTA_ENABLED is unset — returns ESP_OK without
 * touching anything. Safe to call unconditionally from main(). */
esp_err_t bt_agent_ota_check_and_update(void);

#ifdef __cplusplus
}
#endif
