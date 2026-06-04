/*
    Copyright 2025 Super VESC Display
    Copyright 2026 Adapted to ESP-IDF for ESP32-P4

    Trip / Ah / uptime persistence. Stored in SPIFFS (/vescfs/trip.bin) via the
    shared app_fs mount, NOT NVS. The "current VESC value + offset" pattern is
    unchanged — when the VESC reboots and its tachometer/Ah/uptime drop, we fold
    the previous reading into the offset so displayed totals don't jump back.

    The SPIFFS mount is asynchronous (first boot formats the partition), so the
    saved state is loaded lazily on the first update once the FS is ready.
*/

#include "vesc_trip_persist.h"
#include "app_fs.h"

#include "esp_log.h"
#include "esp_timer.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "trip_persist";

#define TRIP_MAGIC        0x54524950u   /* "TRIP" */
#define SAVE_INTERVAL_US  (10ULL * 1000 * 1000)   /* same 10 s throttle as before */

typedef struct {
    uint32_t magic;
    float    trip_total_m;
    float    ah_total;
    uint32_t uptime_total_ms;
} trip_blob_t;

static bool     s_initialized;
static float    s_trip_offset_meters;
static float    s_ah_offset;
static uint32_t s_uptime_offset_ms;
static float    s_current_vesc_trip;
static float    s_current_vesc_ah;
static uint32_t s_current_vesc_uptime;
static int64_t  s_last_save_us;
static bool     s_first_update = true;
static bool     s_have_saved_state;
static bool     s_load_attempted;   /* deferred until app_fs is ready */
static void   (*s_reset_cb)(void);

static void trip_path(char *buf, size_t n) { snprintf(buf, n, "%s/trip.bin", app_fs_base()); }

static bool load_state(void)
{
    if (!app_fs_ready()) return false;
    char path[40];
    trip_path(path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    trip_blob_t b;
    size_t r = fread(&b, 1, sizeof b, f);
    fclose(f);
    if (r != sizeof b || b.magic != TRIP_MAGIC || b.trip_total_m < 0.0f || b.ah_total < 0.0f) {
        return false;
    }
    /* Park the saved totals into the offset vars; the first update converts
     * them into proper offsets relative to the live VESC counters. */
    s_trip_offset_meters = b.trip_total_m;
    s_ah_offset          = b.ah_total;
    s_uptime_offset_ms   = b.uptime_total_ms;
    ESP_LOGI(TAG, "loaded: trip=%.2f m, Ah=%.2f, uptime=%u ms",
             b.trip_total_m, b.ah_total, (unsigned)b.uptime_total_ms);
    return true;
}

static void save_state(void)
{
    if (!app_fs_ready()) return;
    char path[40];
    trip_path(path, sizeof path);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    trip_blob_t b = {
        .magic           = TRIP_MAGIC,
        .trip_total_m    = s_current_vesc_trip   + s_trip_offset_meters,
        .ah_total        = s_current_vesc_ah     + s_ah_offset,
        .uptime_total_ms = s_current_vesc_uptime + s_uptime_offset_ms,
    };
    fwrite(&b, 1, sizeof b, f);
    fclose(f);
}

void trip_persist_init(void)
{
    app_fs_ensure();   /* mount the backup FS at boot so trips persist while riding */

    s_trip_offset_meters = 0.0f;
    s_ah_offset          = 0.0f;
    s_uptime_offset_ms   = 0;
    s_current_vesc_trip   = 0.0f;
    s_current_vesc_ah     = 0.0f;
    s_current_vesc_uptime = 0;
    s_last_save_us        = 0;
    s_first_update        = true;
    s_have_saved_state    = false;
    s_load_attempted      = false;
    s_initialized         = true;
    ESP_LOGI(TAG, "init (SPIFFS-backed, load deferred until FS ready)");
}

void trip_persist_update(float vesc_trip_meters,
                         float vesc_amp_hours,
                         uint32_t vesc_uptime_ms)
{
    if (!s_initialized) return;

    /* Deferred load: the SPIFFS mount finishes asynchronously after boot. */
    if (!s_load_attempted && app_fs_ready()) {
        s_have_saved_state = load_state();
        s_load_attempted   = true;
        s_first_update     = true;   /* re-run the offset conversion below */
    }

    if (s_first_update) {
        if (s_have_saved_state) {
            float saved_trip   = s_trip_offset_meters;
            float saved_ah     = s_ah_offset;
            uint32_t saved_up  = s_uptime_offset_ms;
            s_trip_offset_meters = saved_trip - vesc_trip_meters;
            s_ah_offset          = saved_ah   - vesc_amp_hours;
            s_uptime_offset_ms   = saved_up   - vesc_uptime_ms;
            if (s_trip_offset_meters < 0.0f) s_trip_offset_meters = 0.0f;
            if (s_ah_offset          < 0.0f) s_ah_offset          = 0.0f;
            if (saved_up < vesc_uptime_ms) s_uptime_offset_ms = 0;
            ESP_LOGI(TAG, "offsets: trip=%.2f m, Ah=%.2f, uptime=%u ms",
                     s_trip_offset_meters, s_ah_offset, (unsigned)s_uptime_offset_ms);
        }
        /* Only finalize first_update once the load has been attempted, so a
         * pre-FS-ready first tick doesn't lock us out of applying saved state. */
        if (s_load_attempted) {
            s_first_update = false;
        }
    }

    /* Detect mid-run VESC reboot: counters that suddenly drop fold into the
     * offset so totals stay monotonic. */
    if (vesc_trip_meters < s_current_vesc_trip - 1.0f) {
        s_trip_offset_meters += s_current_vesc_trip;
    }
    if (vesc_amp_hours < s_current_vesc_ah - 0.01f) {
        s_ah_offset += s_current_vesc_ah;
    }
    if (vesc_uptime_ms + 1000u < s_current_vesc_uptime) {
        s_uptime_offset_ms += s_current_vesc_uptime;
    }

    s_current_vesc_trip   = vesc_trip_meters;
    s_current_vesc_ah     = vesc_amp_hours;
    s_current_vesc_uptime = vesc_uptime_ms;

    int64_t now_us = esp_timer_get_time();
    if (now_us - s_last_save_us >= SAVE_INTERVAL_US) {
        save_state();
        s_last_save_us = now_us;
    }
}

float trip_persist_get_trip_km(void)
{
    if (!s_initialized) return 0.0f;
    return (s_current_vesc_trip + s_trip_offset_meters) / 1000.0f;
}

float trip_persist_get_amp_hours(void)
{
    if (!s_initialized) return 0.0f;
    return s_current_vesc_ah + s_ah_offset;
}

uint32_t trip_persist_get_uptime_ms(void)
{
    if (!s_initialized) return 0;
    return s_current_vesc_uptime + s_uptime_offset_ms;
}

void trip_persist_reset(void)
{
    s_trip_offset_meters  = 0.0f;
    s_ah_offset           = 0.0f;
    s_uptime_offset_ms    = 0;
    s_current_vesc_trip   = 0.0f;
    s_current_vesc_ah     = 0.0f;
    s_current_vesc_uptime = 0;
    s_first_update        = true;
    s_have_saved_state    = false;
    s_load_attempted      = true;   /* nothing to load after a wipe */

    if (app_fs_ready()) {
        char path[40];
        trip_path(path, sizeof path);
        remove(path);
    }
    ESP_LOGI(TAG, "reset complete");

    if (s_reset_cb) s_reset_cb();   /* roll the trip logger over to a new trip */
}

void trip_persist_set_reset_cb(void (*cb)(void))
{
    s_reset_cb = cb;
}

bool trip_persist_is_initialized(void)
{
    return s_initialized;
}
