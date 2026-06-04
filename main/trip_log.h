/*
 * Per-trip logging to LittleFS (/vescfs/trips/<id>/). Each trip is a folder
 * with a summary.bin (running totals) and series.bin (a time-series sample
 * appended every ~10 s). A new trip starts on "reset trip" / battery swap
 * (hooked via vesc_trip_persist's reset callback); the current trip continues
 * across reboots. At most 50 trips are kept — the oldest are pruned on boot.
 *
 * Intended to back a future on-device (or off-device) trip analyzer.
 */
#pragma once

#include "vesc_can/vesc_datatypes.h"   /* vesc_setup_values_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Registers the reset hook. FS work is deferred until app_fs is mounted. */
void trip_log_init(void);

/* Call every RT-data tick with the latest VESC snapshot. Internally throttles
 * the time-series sampling to ~10 s and tracks max speed. */
void trip_log_tick(const vesc_setup_values_t *rt);

/* Finalize the current trip and begin a new one. Wired to vesc_trip_persist's
 * reset callback (fires on the dashboard reset button and on battery-swap). */
void trip_log_new_trip(void);

#ifdef __cplusplus
}
#endif
