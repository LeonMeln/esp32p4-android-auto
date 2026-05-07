#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shared firmware-update overlay. Builds a hidden full-screen container with
 * a title, subtitle, progress bar, percent label and status line. Sits on
 * lv_scr_act() — display_init() must have run first.
 *
 * Used by c6_ota (Wi-Fi co-processor update on boot) and bt_agent_ota
 * (BT agent reflash on version mismatch). Both call set_title before
 * show() to label which device is being updated.
 *
 * Idempotent. Compiled-out when CONFIG_C6_OTA_DISPLAY_PROGRESS=n. */
esp_err_t ota_screen_init(void);

/* Update the title label (top of the screen). Default is set in init.
 * Pass NULL or empty to leave unchanged. */
void ota_screen_set_title(const char *title);

void ota_screen_show(const char *subtitle);

void ota_screen_set_progress(uint32_t bytes_done, uint32_t bytes_total);

void ota_screen_set_status(const char *line);

/* Same as set_status but renders the line in a red error tint and resets
 * the progress bar tint to red as well — for failure end-states that
 * should stay on screen until the caller hides. */
void ota_screen_set_status_error(const char *line);

void ota_screen_hide(void);

#ifdef __cplusplus
}
#endif
