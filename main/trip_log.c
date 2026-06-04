/* See trip_log.h. Trip history on LittleFS: /vescfs/trips/<id>/{summary,series}.bin */
#include "trip_log.h"

#include "app_fs.h"
#include "vesc_trip_persist.h"

#include "esp_log.h"
#include "esp_timer.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *TAG = "trip_log";

#define MAX_TRIPS         50
#define SAMPLE_INTERVAL_US (10ULL * 1000 * 1000)
#define TRIP_MAGIC        0x54524950u   /* "TRIP" */
#define TRIP_VERSION      2             /* v2: sample temps are int16 °C×10 */

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t id;
    uint32_t duration_s;     /* sample_count * 10 */
    uint32_t sample_count;
    float    distance_km;
    float    consumed_ah;
    float    max_speed_kmh;
} trip_summary_t;

typedef struct {
    uint32_t t_s;            /* seconds into the trip */
    int16_t  speed_dkmh;     /* km/h * 10 */
    int16_t  current_da;     /* input current A * 10 */
    uint16_t voltage_dv;     /* V * 10 */
    int16_t  temp_motor_dc;  /* motor temp °C * 10 (int16 — i8 overflowed >127°C) */
    int16_t  temp_fet_dc;    /* FET temp °C * 10 */
    uint8_t  batt_pct;
    uint8_t  pad;
} trip_sample_t;             /* 16 bytes */

static bool           s_fs_done;       /* did the one-time on-FS init run */
static volatile bool  s_pending_new;   /* rollover requested (set from any task) */
static uint32_t       s_cur_id;
static trip_summary_t s_sum;
static int64_t        s_last_sample_us;

/* ---- path helpers ---- */
static void trips_root(char *b, size_t n) { snprintf(b, n, "%s/trips", app_fs_base()); }
static void trip_dir(char *b, size_t n, uint32_t id) { snprintf(b, n, "%s/trips/%u", app_fs_base(), (unsigned)id); }
static void trip_file(char *b, size_t n, uint32_t id, const char *name)
{ snprintf(b, n, "%s/trips/%u/%s", app_fs_base(), (unsigned)id, name); }

/* ---- scan / prune ---- */
static int scan_ids(uint32_t *ids, int cap, uint32_t *max_out)
{
    char root[40];
    trips_root(root, sizeof root);
    DIR *d = opendir(root);
    if (!d) { if (max_out) *max_out = 0; return 0; }
    int cnt = 0;
    uint32_t mx = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;   /* numeric trip folders */
        uint32_t id = (uint32_t)strtoul(e->d_name, NULL, 10);
        if (id == 0) continue;
        if (id > mx) mx = id;
        if (cnt < cap) ids[cnt++] = id;
    }
    closedir(d);
    if (max_out) *max_out = mx;
    return cnt;
}

static void delete_trip(uint32_t id)
{
    char p[64];
    trip_file(p, sizeof p, id, "summary.bin"); remove(p);
    trip_file(p, sizeof p, id, "series.bin");  remove(p);
    trip_dir(p, sizeof p, id); rmdir(p);
    ESP_LOGI(TAG, "pruned trip %u", (unsigned)id);
}

static void prune_to_max(void)
{
    uint32_t ids[80];
    int cnt = scan_ids(ids, 80, NULL);
    while (cnt > MAX_TRIPS) {
        /* find + delete the smallest id */
        int min_i = 0;
        for (int i = 1; i < cnt; i++) if (ids[i] < ids[min_i]) min_i = i;
        delete_trip(ids[min_i]);
        ids[min_i] = ids[--cnt];
    }
}

/* ---- summary I/O ---- */
static void summary_write(void)
{
    char p[64];
    trip_file(p, sizeof p, s_cur_id, "summary.bin");
    FILE *f = fopen(p, "wb");
    if (!f) return;
    fwrite(&s_sum, 1, sizeof s_sum, f);
    fclose(f);
}

static void summary_reset(uint32_t id)
{
    memset(&s_sum, 0, sizeof s_sum);
    s_sum.magic = TRIP_MAGIC;
    s_sum.version = TRIP_VERSION;
    s_sum.id = id;
}

static bool summary_load(uint32_t id)
{
    char p[64];
    trip_file(p, sizeof p, id, "summary.bin");
    FILE *f = fopen(p, "rb");
    if (!f) return false;
    trip_summary_t s;
    size_t r = fread(&s, 1, sizeof s, f);
    fclose(f);
    if (r != sizeof s || s.magic != TRIP_MAGIC) return false;
    s_sum = s;
    return true;
}

static void create_trip(uint32_t id)
{
    char dir[48];
    trip_dir(dir, sizeof dir, id);
    mkdir(dir, 0777);
    s_cur_id = id;
    summary_reset(id);
    summary_write();
    s_last_sample_us = 0;
    ESP_LOGI(TAG, "started trip %u", (unsigned)id);
}

/* ---- lazy on-FS init (runs once the mount is ready) ---- */
static void fs_init_once(void)
{
    if (s_fs_done || !app_fs_ready()) return;
    s_fs_done = true;

    char root[40];
    trips_root(root, sizeof root);
    mkdir(root, 0777);

    prune_to_max();

    uint32_t mx = 0;
    scan_ids(NULL, 0, &mx);   /* cap 0: just want the max id */
    if (mx == 0) {
        create_trip(1);            /* very first trip */
    } else {
        s_cur_id = mx;             /* resume the newest (current) trip */
        if (!summary_load(s_cur_id)) summary_reset(s_cur_id);
        ESP_LOGI(TAG, "resumed trip %u (%u samples, %.2f km)",
                 (unsigned)s_cur_id, (unsigned)s_sum.sample_count, s_sum.distance_km);
    }
}

/* Actual rollover — MUST run only on the trip_log_tick (updater) task so it
 * never races a concurrent sample and never does flash I/O on the LVGL thread. */
static void do_new_trip(void)
{
    summary_write();               /* finalize the outgoing trip */
    uint32_t mx = 0;
    scan_ids(NULL, 0, &mx);
    create_trip(mx + 1);
    prune_to_max();
}

/* ---- public ---- */
void trip_log_init(void)
{
    s_fs_done = false;
    s_pending_new = false;
    s_cur_id = 0;
    s_last_sample_us = 0;
    summary_reset(0);
    trip_persist_set_reset_cb(trip_log_new_trip);   /* reset button + battery swap */
    app_fs_ensure();
    ESP_LOGI(TAG, "init (deferred until FS ready)");
}

/* Callable from any task (reset button runs on the LVGL thread, battery-swap on
 * the updater task). Only flags the rollover; trip_log_tick does the work. */
void trip_log_new_trip(void)
{
    s_pending_new = true;
}

void trip_log_tick(const vesc_setup_values_t *rt)
{
    if (!rt) return;
    fs_init_once();
    if (!app_fs_ready() || s_cur_id == 0) return;
    if (s_pending_new) { s_pending_new = false; do_new_trip(); return; }

    float speed_kmh = rt->speed * 3.6f;
    if (speed_kmh > s_sum.max_speed_kmh) s_sum.max_speed_kmh = speed_kmh;

    int64_t now = esp_timer_get_time();
    if (s_last_sample_us != 0 && (now - s_last_sample_us) < (int64_t)SAMPLE_INTERVAL_US) {
        return;
    }
    s_last_sample_us = now;

    s_sum.sample_count++;
    s_sum.duration_s = s_sum.sample_count * 10u;
    s_sum.distance_km = trip_persist_get_trip_km();
    s_sum.consumed_ah = trip_persist_get_amp_hours();

    trip_sample_t s = {
        .t_s          = s_sum.duration_s,
        .speed_dkmh   = (int16_t)(speed_kmh * 10.0f),
        .current_da   = (int16_t)(rt->current_in * 10.0f),
        .voltage_dv   = (uint16_t)(rt->v_in * 10.0f),
        .temp_motor_dc = (int16_t)(rt->temp_motor * 10.0f),
        .temp_fet_dc   = (int16_t)(rt->temp_mos * 10.0f),
        .batt_pct      = (uint8_t)(rt->battery_level * 100.0f),
        .pad           = 0,
    };
    char p[64];
    trip_file(p, sizeof p, s_cur_id, "series.bin");
    FILE *f = fopen(p, "ab");
    if (f) { fwrite(&s, 1, sizeof s, f); fclose(f); }

    summary_write();
}
