#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Reset trip + amp-hours counters held by the battery calc backend.
 * Wired to the dashboard's reset icon (custom.c::reset_icon_pressed).
 * Real implementation will land with the vesc_app backend; the stub in
 * vesc_stubs.c just logs the call so the build is linkable today. */
void battery_calc_reset_trip_and_ah(void);

#ifdef __cplusplus
}
#endif
