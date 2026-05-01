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
 * completes / is skipped to show "Waiting for Android Auto" + IP). */
void ota_screen_show_idle(const char *line1, const char *line2);
