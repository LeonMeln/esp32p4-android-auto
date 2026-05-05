#include "vesc_ui_updater.h"

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui_mode.h"
#include "vesc_can/vesc_rt_data.h"

/* GUI Guider's custom.h is generated C++-friendly but pure C. The
 * widget update functions live in Super_VESC_Display/custom/custom.c
 * which is built as part of vesc_ui. */
#include "custom.h"

static const char *TAG = "vesc_ui_upd";

static TaskHandle_t s_task;
static bool         s_zeros_pushed;

static void push_zeros_locked(void)
{
    update_speed(0.0f);
    update_current(0.0f);
    update_battery_proc(0.0f);
    update_trip(0.0f);
    update_range(0.0f);
    update_temp_fet(0.0f);
    update_temp_motor(0.0f);
    update_amp_hours(0.0f);
    update_battery_temp(0.0f);
    update_battery_voltage(0.0f);
    update_odometer(0.0f);
    update_fps(0);
}

static void push_rt_locked(void)
{
    bool fresh = vesc_rt_data_is_fresh();
    update_esc_connection_status(fresh);
    if (!fresh) return;

    const vesc_setup_values_t *rt = vesc_rt_data_get_latest();

    update_speed(vesc_rt_data_get_speed_kmh());
    update_current(rt->current_in);
    /* battery_level is 0..1 in VESC protocol → display wants percent. */
    update_battery_proc(rt->battery_level * 100.0f);
    update_trip(vesc_rt_data_get_trip_km());
    /* No range estimator yet — shown as Wh/km efficiency placeholder. */
    update_range(0.0f);
    update_temp_fet(rt->temp_mos);
    update_temp_motor(rt->temp_motor);
    update_amp_hours(vesc_rt_data_get_amp_hours());
    update_battery_temp(rt->temp_mos);
    update_battery_voltage(rt->v_in);
    update_odometer(vesc_rt_data_get_odometer_km());
    update_uptime(vesc_rt_data_get_uptime_ms());
}

static void updater_task(void *arg)
{
    (void)arg;
    const TickType_t period = pdMS_TO_TICKS(50);
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        if (bsp_display_lock(50) == ESP_OK) {
            if (!s_zeros_pushed) {
                push_zeros_locked();
                s_zeros_pushed = true;
            }
            push_rt_locked();
            bsp_display_unlock();
        }
        vTaskDelayUntil(&last, period);
    }
}

esp_err_t vesc_ui_updater_start(void)
{
    if (s_task) return ESP_OK;
    BaseType_t r = xTaskCreate(updater_task, "vesc_ui_upd", 4096, NULL, 4, &s_task);
    if (r != pdPASS) {
        ESP_LOGE(TAG, "task create failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}
