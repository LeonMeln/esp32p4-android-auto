/*
 * VBAT-in-active-mode experiment.
 *
 * Espressif's IDF leaves PMU.lp_sys[PMU_MODE_LP_ACTIVE].dig_power.vddbat_mode
 * = 0 on the P4, so when ESP_3V3 collapses (USB unplug while running) the LP
 * power domain loses supply, LP_TIMER and LP_STORE2/3 reset, and on the
 * next boot time(NULL) starts at 0. The official VBAT API only flips
 * vddbat_mode = 1 as part of esp_deep_sleep_start().
 *
 * Interestingly, on the newer ESP32-H21 the same field is initialized to
 * vddbat_mode = 2 for LP_ACTIVE (pmu_param.c:362), proving the hardware
 * supports VBAT-routing while running on at least one chip in the family.
 *
 * This module pokes the LP_ACTIVE config to 1 (the value that worked for
 * LP_SLEEP) and triggers PMU.vbat_cfg.sw_update so the analog state
 * machine picks up the change. Then it logs PMU.vbat_cfg.ana_vddbat_mode
 * (read-only status) so we can see what the analog side decided.
 *
 * Procedure: flash, watch the boot log for the ana_vddbat_mode readback,
 * then yank USB while the dashboard is up; the CR2032 on H8 should keep
 * the LP domain alive. Plug back in: if `dev_settings: clock at boot: HH:MM`
 * reflects the last set time, the trick worked.
 *
 * This is UNDOCUMENTED for the P4 — possible outcomes: works, no-op, or
 * destabilizes the PMU. If anything misbehaves, idf.py erase-flash and
 * revert this file.
 */

#include "vbat_experiment.h"

#include "esp_log.h"
#include "esp_rom_sys.h"
#include "hal/pmu_types.h"
#include "soc/pmu_struct.h"

static const char *TAG = "vbat_exp";

void vbat_experiment_enable_active_routing(void)
{
    /* Read what PMU currently reports the analog side at. */
    uint32_t before = PMU.vbat_cfg.ana_vddbat_mode;

    /* Configure LP_ACTIVE to use VBAT routing. Try value 1 first — that's
     * what LP_SLEEP uses on P4 when the official VBAT API is in play.
     * The 2-bit field allows 0..3; on H21 the LP_ACTIVE default is 2,
     * but P4 doesn't enumerate what mode 2 means here, so start with 1. */
    PMU.lp_sys[PMU_MODE_LP_ACTIVE].dig_power.vddbat_mode = 1;

    /* Write-trigger so the analog state machine picks up the new mode. */
    PMU.vbat_cfg.sw_update = 1;

    /* The SW_UPDATE bit is self-clearing on commit — wait a few cycles
     * before reading the analog readback to give the PMU time to settle. */
    esp_rom_delay_us(200);

    uint32_t after = PMU.vbat_cfg.ana_vddbat_mode;
    ESP_LOGW(TAG, "vddbat_mode poke: ana before=%u after=%u (1=VBAT)",
             (unsigned)before, (unsigned)after);

    if (after == 1u) {
        ESP_LOGW(TAG, "PMU acknowledged active-mode VBAT routing — try USB unplug");
    } else {
        ESP_LOGW(TAG, "PMU did NOT switch to VBAT in active mode (ana=%u)",
                 (unsigned)after);
    }
}
