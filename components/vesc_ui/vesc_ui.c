#include "vesc_ui.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "gui_guider.h"
#include "events_init.h"
#include "custom.h"

/* Single global GUI Guider state — referenced as `extern lv_ui guider_ui`
 * by generated and custom code. Definition lives here. */
lv_ui guider_ui;

static bool s_inited;

void vesc_ui_init(void)
{
    if (s_inited) return;
    int64_t t0 = esp_timer_get_time();
    /* Same as setup_ui() in gui_guider.c minus the lv_scr_load(dashboard)
     * — caller (ui_mode) controls which screen is active. */
    init_scr_del_flag(&guider_ui);
    init_keyboard(&guider_ui);
    setup_scr_dashboard(&guider_ui);
    int64_t t1 = esp_timer_get_time();
    custom_init(&guider_ui);
    int64_t t2 = esp_timer_get_time();
    ESP_LOGI("vesc_ui", "init: setup=%lldms custom=%lldms TOTAL=%lldms",
             (t1 - t0) / 1000, (t2 - t1) / 1000, (t2 - t0) / 1000);
    s_inited = true;
}

lv_obj_t *vesc_ui_get_screen(void)
{
    return s_inited ? guider_ui.dashboard : NULL;
}
