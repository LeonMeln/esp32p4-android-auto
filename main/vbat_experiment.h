#pragma once

/* See vbat_experiment.c for context. Call once during boot, after PMU /
 * sleep subsystem init has run (i.e. after settings_init / nvs_flash_init
 * is fine — they don't touch PMU). Safe to call multiple times. */
void vbat_experiment_enable_active_routing(void);
