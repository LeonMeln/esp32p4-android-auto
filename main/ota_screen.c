#include "ota_screen.h"

#include <stdio.h>

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "ota_screen";

static lv_display_t *s_display;
static lv_obj_t *s_root;
static lv_obj_t *s_title;
static lv_obj_t *s_subtitle;
static lv_obj_t *s_status;
static lv_obj_t *s_bar;
static lv_obj_t *s_pct;
static bool s_initialized;

struct _lv_display_t *ota_screen_get_display(void)
{
    return (struct _lv_display_t *)s_display;
}

esp_err_t ota_screen_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

#if !CONFIG_C6_OTA_DISPLAY_PROGRESS
    ESP_LOGI(TAG, "display progress disabled — OTA will be log-only");
    return ESP_OK;
#endif

    /* Rotate 90° clockwise so the 800×480 panel reads landscape (480 high
     * × 800 wide on the user side). */
    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_90,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
    };
    s_display = bsp_display_start_with_config(&cfg);
    if (!s_display) {
        ESP_LOGE(TAG, "bsp_display_start failed");
        return ESP_FAIL;
    }

    /* Backlight off while the framebuffer is still uninitialised — that's
     * what causes the 1-2 s of white flash at boot. We turn it on once
     * the first frame has rendered our black background. */
    bsp_display_backlight_off();

    /* Generous timeout — LVGL task may still be holding the lock right
     * after esp_lv_adapter_start() returns. */
    if (bsp_display_lock(1000) != ESP_OK) {
        return ESP_FAIL;
    }

    /* Paint the LVGL active screen black so later widgets land on black,
     * not LVGL's default white. */
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

    s_root = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_root, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_pad_all(s_root, 24, 0);
    lv_obj_set_flex_flow(s_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);

    s_title = lv_label_create(s_root);
    lv_label_set_text(s_title, "Updating Wi-Fi co-processor");
    lv_obj_set_style_text_color(s_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_title, &lv_font_montserrat_32, 0);
    lv_obj_set_style_pad_bottom(s_title, 12, 0);

    s_subtitle = lv_label_create(s_root);
    lv_label_set_text(s_subtitle, "Don't power off");
    lv_obj_set_style_text_color(s_subtitle, lv_color_hex(0xa0a0a0), 0);
    lv_obj_set_style_pad_bottom(s_subtitle, 32, 0);

    s_bar = lv_bar_create(s_root);
    lv_obj_set_size(s_bar, LV_PCT(80), 20);
    lv_bar_set_range(s_bar, 0, 100);
    lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar, lv_color_hex(0x222222), 0);
    lv_obj_set_style_bg_color(s_bar, lv_color_hex(0x3aa3ff), LV_PART_INDICATOR);

    s_pct = lv_label_create(s_root);
    lv_label_set_text(s_pct, "0%");
    lv_obj_set_style_text_color(s_pct, lv_color_white(), 0);
    lv_obj_set_style_pad_top(s_pct, 12, 0);

    s_status = lv_label_create(s_root);
    lv_label_set_text(s_status, "");
    lv_obj_set_style_text_color(s_status, lv_color_hex(0x808080), 0);

    bsp_display_unlock();

    /* Give LVGL a couple of render cycles to push the black background +
     * UI to the panel before lighting the backlight — partial-rotate flush
     * can take more than one frame to settle. Otherwise we'd briefly
     * unmask whatever stale data was in the framebuffer. */
    vTaskDelay(pdMS_TO_TICKS(200));
    bsp_display_backlight_on();

    s_initialized = true;
    return ESP_OK;
}

void ota_screen_show(const char *subtitle)
{
    if (!s_initialized) {
        return;
    }
    if (bsp_display_lock(200) == ESP_OK) {
        if (subtitle) {
            lv_label_set_text(s_subtitle, subtitle);
        }
        lv_obj_clear_flag(s_root, LV_OBJ_FLAG_HIDDEN);
        bsp_display_unlock();
    }
}

void ota_screen_set_progress(uint32_t done, uint32_t total)
{
    if (!s_initialized || !total) {
        return;
    }
    int pct = (int)((uint64_t)done * 100 / total);
    if (pct > 100) pct = 100;
    if (bsp_display_lock(200) == ESP_OK) {
        lv_bar_set_value(s_bar, pct, LV_ANIM_OFF);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", pct);
        lv_label_set_text(s_pct, buf);
        bsp_display_unlock();
    }
}

void ota_screen_set_status(const char *line)
{
    if (!s_initialized) {
        return;
    }
    if (bsp_display_lock(200) == ESP_OK) {
        lv_label_set_text(s_status, line ? line : "");
        bsp_display_unlock();
    }
}

void ota_screen_hide(void)
{
    if (!s_initialized) {
        return;
    }
    if (bsp_display_lock(200) == ESP_OK) {
        lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
        bsp_display_unlock();
    }
}

void ota_screen_show_idle(const char *line1, const char *line2)
{
    if (!s_initialized) {
        return;
    }
    if (bsp_display_lock(200) == ESP_OK) {
        lv_label_set_text(s_title, line1 ? line1 : "");
        lv_label_set_text(s_subtitle, line2 ? line2 : "");
        /* Hide OTA-specific widgets (bar/percent/status); keep root visible. */
        lv_obj_add_flag(s_bar,    LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_pct,    LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_status, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_root, LV_OBJ_FLAG_HIDDEN);
        bsp_display_unlock();
    }
}
