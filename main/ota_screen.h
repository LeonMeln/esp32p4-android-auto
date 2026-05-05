#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t ota_screen_init(void);

void ota_screen_show(const char *subtitle);

void ota_screen_set_progress(uint32_t bytes_done, uint32_t bytes_total);

void ota_screen_set_status(const char *line);

void ota_screen_hide(void);

/* Replace OTA progress UI with a two-line idle status (used after OTA
 * completes / is skipped to show "Waiting for Android Auto" + IP).
 * The strings are cached so ota_screen_refresh_idle() can re-apply
 * them after the AA video sink scribbled over the panel. */
void ota_screen_show_idle(const char *line1, const char *line2);

/* Re-apply the last `show_idle` strings and force LVGL to repaint the
 * active screen. Call after AA disconnects so the stale last video
 * frame in the panel framebuffer is replaced by the idle text. */
void ota_screen_refresh_idle(void);

/* Returns the lv_display_t* the BSP gave us at init time, or NULL if the
 * display was disabled (CONFIG_C6_OTA_DISPLAY_PROGRESS=n). The video sink
 * needs this to drive the LVGL adapter into dummy-draw mode. */
struct _lv_display_t;
struct _lv_display_t *ota_screen_get_display(void);
