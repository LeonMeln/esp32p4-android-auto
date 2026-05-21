#include "ota_screen.h"

#include <stdio.h>

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "ota_screen";

#if !CONFIG_C6_OTA_DISPLAY_PROGRESS

esp_err_t ota_screen_init(void)
{
    ESP_LOGI(TAG, "display progress disabled — OTA will be log-only");
    return ESP_OK;
}
void ota_screen_set_title(const char *title) { (void)title; }
void ota_screen_show(const char *subtitle) { (void)subtitle; }
void ota_screen_set_progress(uint32_t done, uint32_t total) { (void)done; (void)total; }
void ota_screen_set_status(const char *line) { (void)line; }
void ota_screen_set_status_error(const char *line) { (void)line; }
void ota_screen_hide(void) { }

#else

static lv_obj_t *s_root;
static lv_obj_t *s_title;
static lv_obj_t *s_subtitle;
static lv_obj_t *s_status;
static lv_obj_t *s_bar;
static lv_obj_t *s_pct;
static lv_obj_t *s_prev_screen;   /* what lv_scr_act() was when we showed */
static bool      s_initialized;

#define BAR_COLOR_OK     0x3aa3ff
#define BAR_COLOR_ERROR  0xff5050
#define STATUS_COLOR_OK  0x808080
#define STATUS_COLOR_ERR 0xff5050

esp_err_t ota_screen_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }
    if (bsp_display_lock(1000) != ESP_OK) {
        ESP_LOGE(TAG, "lvgl lock timeout");
        return ESP_FAIL;
    }

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
    lv_obj_set_style_bg_color(s_bar, lv_color_hex(BAR_COLOR_OK), LV_PART_INDICATOR);

    s_pct = lv_label_create(s_root);
    lv_label_set_text(s_pct, "0%");
    lv_obj_set_style_text_color(s_pct, lv_color_white(), 0);
    lv_obj_set_style_pad_top(s_pct, 12, 0);

    s_status = lv_label_create(s_root);
    lv_label_set_text(s_status, "");
    lv_obj_set_style_text_color(s_status, lv_color_hex(STATUS_COLOR_OK), 0);

    bsp_display_unlock();
    s_initialized = true;
    return ESP_OK;
}

void ota_screen_set_title(const char *title)
{
    if (!s_initialized || !title || !*title) {
        return;
    }
    if (bsp_display_lock(200) == ESP_OK) {
        lv_label_set_text(s_title, title);
        bsp_display_unlock();
    }
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
        /* Reset visual state to OK colours every time we re-show — a
         * previous failure run may have left status red and bar red. */
        lv_obj_set_style_text_color(s_status, lv_color_hex(STATUS_COLOR_OK), 0);
        lv_obj_set_style_bg_color(s_bar, lv_color_hex(BAR_COLOR_OK), LV_PART_INDICATOR);
        lv_label_set_text(s_status, "");
        lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
        lv_label_set_text(s_pct, "0%");
        lv_obj_clear_flag(s_root, LV_OBJ_FLAG_HIDDEN);
        /* The overlay was built as a child of whichever screen was active at
         * init() time (typically the AA idle screen). If callers ran after
         * ui_mode_init switched to the VESC dashboard, the overlay would be
         * sitting on an off-screen page — silently invisible. Swap to the
         * overlay's own parent screen here, restore on hide. */
        lv_obj_t *parent_scr = lv_obj_get_parent(s_root);
        lv_obj_t *cur = lv_scr_act();
        if (parent_scr && parent_scr != cur) {
            s_prev_screen = cur;
            ESP_LOGW(TAG, "show: lv_scr_load(parent=%p)", parent_scr);
            lv_scr_load(parent_scr);
        }
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

void ota_screen_set_status_error(const char *line)
{
    if (!s_initialized) {
        return;
    }
    if (bsp_display_lock(200) == ESP_OK) {
        lv_obj_set_style_text_color(s_status, lv_color_hex(STATUS_COLOR_ERR), 0);
        lv_obj_set_style_bg_color(s_bar, lv_color_hex(BAR_COLOR_ERROR), LV_PART_INDICATOR);
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
        if (s_prev_screen) {
            ESP_LOGW(TAG, "hide: lv_scr_load(prev=%p)", s_prev_screen);
            lv_scr_load(s_prev_screen);
            s_prev_screen = NULL;
        }
        bsp_display_unlock();
    }
}

#endif /* CONFIG_C6_OTA_DISPLAY_PROGRESS */
