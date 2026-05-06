/*
 * cockpit_ui.h — VESC Cockpit Dashboard, LVGL 8.x (тестировалось на 8.3)
 * Дисплей: 800x480, тёмная тема, акцент кислотно-зелёный (#b6ff2e)
 *
 * Публичный API:
 *   cockpit_ui_init()    — создать экран и все виджеты (вызвать один раз)
 *   cockpit_ui_update()  — обновить значения (вызывать при поступлении данных)
 *
 * Все строки форматируются внутри — снаружи передавай сырые числа.
 */

#ifndef COCKPIT_UI_H
#define COCKPIT_UI_H

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COCKPIT_MODE_ECO    = 1,
    COCKPIT_MODE_NORMAL = 2,
    COCKPIT_MODE_SPORT  = 3,
} cockpit_mode_t;

typedef struct {
    /* primary */
    uint16_t  speed_kmh;            /* 0..255    */
    uint16_t  speed_max_kmh;        /* для масштаба полосы, обычно 60 */
    uint8_t   battery_percent;      /* 0..100    */
    float     voltage_v;            /* напр. 54.2 */
    uint16_t  power_w;              /* мощность в ваттах, 0..powerMax */
    uint16_t  power_max_w;          /* обычно 4500 */
    float     motor_current_a;
    int16_t   motor_temp_c;
    int16_t   controller_temp_c;
    uint16_t  rpm;
    /* trip */
    float     trip_km;              /* поездка */
    uint32_t  odometer_km;          /* всего */
    uint16_t  range_km;             /* остаток хода */
    uint16_t  speed_avg_kmh;
    uint32_t  ride_time_s;
    /* status */
    cockpit_mode_t mode;
    bool      bluetooth;
    bool      gps;
} cockpit_data_t;

/* создаёт screen и все виджеты, делает screen активным */
void cockpit_ui_init(void);

/* обновляет все значения; вызывай при поступлении данных от VESC */
void cockpit_ui_update(const cockpit_data_t *d);

#ifdef __cplusplus
}
#endif

#endif /* COCKPIT_UI_H */
