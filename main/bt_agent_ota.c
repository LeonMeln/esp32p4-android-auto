#include "bt_agent_ota.h"

#include "sdkconfig.h"

#ifndef CONFIG_BT_AGENT_OTA_ENABLED

esp_err_t bt_agent_ota_check_and_update(void) { return ESP_OK; }

#else  /* CONFIG_BT_AGENT_OTA_ENABLED */

#include <stdio.h>
#include <string.h>

#include "bt_agent_fw.h"
#include "bt_link.h"
#include "esp_loader.h"
#include "esp_log.h"
#include "esp32_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ota_screen.h"

static const char *TAG = "bt_agent_ota";

/* Merged image starts at 0x1000 (bootloader offset on ESP32 classic).
 * We pass `--target-offset 0x1000` to esptool merge_bin so the blob has no
 * leading 4 KiB of padding — and crucially, we do NOT write to 0x0..0x1000
 * which the ROM bootloader rejects on this chip. Without that, the first
 * write succeeds (the leading FF page is silently dropped by the ROM) but
 * the second block returns FLASH_WRITE_FAIL — exactly what we saw. */
#define BT_AGENT_FLASH_ADDR  0x1000

/* esp_loader_flash_write block size — library expects multiples of 1024.
 * Larger blocks reduce per-block ROM overhead but eat stack. 4 KiB is the
 * usual sweet spot for UART. */
#define FLASH_BLOCK_SIZE  (4 * 1024)

static esp_err_t do_flash_image(char *err_buf, size_t err_buf_len)
{
    const uint8_t *data = bt_agent_fw_data();
    size_t         size = bt_agent_fw_size();
    if (!data || size == 0) {
        ESP_LOGE(TAG, "embedded blob empty — bt_agent_fw.bin missing?");
        snprintf(err_buf, err_buf_len, "blob missing");
        return ESP_ERR_NOT_FOUND;
    }

    /* Initialize the esp_serial_flasher port. It owns the UART for the
     * duration of the flash; bt_link must have already released it. */
    loader_esp32_config_t cfg = {
        .baud_rate         = 115200,
        .uart_port         = BT_AGENT_UART_PORT,
        .uart_rx_pin       = BT_AGENT_UART_RX,
        .uart_tx_pin       = BT_AGENT_UART_TX,
        .reset_trigger_pin = BT_AGENT_RST_PIN,
        .gpio0_trigger_pin = BT_AGENT_IO0_PIN,
        /* Defaults for the rest (rx/tx_buffer_size=0 → library defaults,
         * uart_queue=NULL, dont_initialize_peripheral=false). */
    };
    ota_screen_set_status("Connecting to bootloader…");
    if (loader_port_esp32_init(&cfg) != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "loader_port_esp32_init failed");
        snprintf(err_buf, err_buf_len, "port init failed");
        return ESP_FAIL;
    }

    esp_loader_connect_args_t connect = ESP_LOADER_CONNECT_DEFAULT();
    if (esp_loader_connect(&connect) != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "esp_loader_connect failed — agent not in bootloader?");
        snprintf(err_buf, err_buf_len, "ROM sync failed");
        loader_port_esp32_deinit();
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "connected to agent ROM bootloader");

    /* Try to bump baud for the actual transfer. Falls back gracefully if the
     * ROM doesn't support the requested rate. */
    if (esp_loader_change_transmission_rate(CONFIG_BT_AGENT_FLASH_BAUD) ==
            ESP_LOADER_SUCCESS) {
        loader_port_change_transmission_rate(CONFIG_BT_AGENT_FLASH_BAUD);
        ESP_LOGI(TAG, "baud → %d", CONFIG_BT_AGENT_FLASH_BAUD);
    }

    ota_screen_set_status("Erasing flash…");
    ota_screen_set_progress(0, size);
    if (esp_loader_flash_start(BT_AGENT_FLASH_ADDR, size, FLASH_BLOCK_SIZE)
            != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "flash_start failed");
        snprintf(err_buf, err_buf_len, "erase failed");
        loader_port_esp32_deinit();
        return ESP_FAIL;
    }

    ota_screen_set_status("Writing firmware…");
    size_t   off = 0;
    int      pct_last = -1;
    while (off < size) {
        size_t chunk = (size - off > FLASH_BLOCK_SIZE)
                           ? FLASH_BLOCK_SIZE : (size - off);
        if (esp_loader_flash_write((void *)(data + off), chunk) !=
                ESP_LOADER_SUCCESS) {
            ESP_LOGE(TAG, "flash_write @ 0x%x failed", (unsigned)off);
            snprintf(err_buf, err_buf_len,
                     "write failed @ 0x%x", (unsigned)off);
            loader_port_esp32_deinit();
            return ESP_FAIL;
        }
        off += chunk;
        ota_screen_set_progress(off, size);
        int pct = (int)((off * 100) / size);
        if (pct / 10 != pct_last / 10) {
            ESP_LOGI(TAG, "flashed %d%% (%u/%u bytes)",
                     pct, (unsigned)off, (unsigned)size);
            pct_last = pct;
        }
    }

    ota_screen_set_status("Verifying…");
    /* Pass true → reboot into the new app. Library handles the reset. */
    if (esp_loader_flash_finish(true) != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "flash_finish failed");
        snprintf(err_buf, err_buf_len, "verify/finish failed");
        loader_port_esp32_deinit();
        return ESP_FAIL;
    }
    loader_port_esp32_deinit();
    ESP_LOGI(TAG, "flash done, agent rebooting");
    return ESP_OK;
}

esp_err_t bt_agent_ota_check_and_update(void)
{
    const char *expected = CONFIG_BT_AGENT_FW_VERSION;
    const char *actual   = bt_agent_wait_version(CONFIG_BT_AGENT_VERSION_TIMEOUT_MS);

    if (actual && strcmp(actual, expected) == 0) {
        ESP_LOGI(TAG, "agent ver=%s matches — OTA skipped", actual);
        return ESP_OK;
    }

    /* Show the overlay before kicking off any UART/GPIO surgery — the user
     * sees "Updating BT agent" instead of staring at a frozen idle screen
     * for the duration of the ~50 s flash. Also unconditionally shown on
     * mismatch so the user always knows when their agent got rewritten. */
    ota_screen_set_title("Updating BT agent");
    ota_screen_show("Don't power off");
    char subtitle[80];
    if (actual) {
        ESP_LOGW(TAG, "agent ver=%s, expected=%s — reflashing", actual, expected);
        snprintf(subtitle, sizeof(subtitle), "Found %s, expected %s",
                 actual, expected);
    } else {
        ESP_LOGW(TAG, "no BT-VER: in %d ms — assuming agent broken, reflashing",
                 CONFIG_BT_AGENT_VERSION_TIMEOUT_MS);
        snprintf(subtitle, sizeof(subtitle), "Agent firmware unresponsive");
    }
    ota_screen_set_status(subtitle);

    bt_link_suspend_for_flash();
    bt_agent_enter_bootloader();
    /* Small settle window so the ROM bootloader's autobaud has stabilised
     * before esp_serial_flasher starts probing. */
    vTaskDelay(pdMS_TO_TICKS(100));

    char err_msg[64] = "";
    esp_err_t flash_err = do_flash_image(err_msg, sizeof(err_msg));

    bt_link_resume_after_flash();

    if (flash_err != ESP_OK) {
        ESP_LOGE(TAG, "OTA failed (%s) — agent may be unbootable",
                 esp_err_to_name(flash_err));
        char line[96];
        snprintf(line, sizeof(line), "Failed: %s", err_msg);
        ota_screen_set_status_error(line);
        /* Hold the failure on screen long enough for the user to read it.
         * No retry loop — a fresh boot will pick this up again. */
        vTaskDelay(pdMS_TO_TICKS(8000));
        ota_screen_hide();
        return flash_err;
    }

    /* Verify the new firmware came up with the expected version. */
    ota_screen_set_status("Waiting for agent to boot…");
    const char *post = bt_agent_wait_version(CONFIG_BT_AGENT_VERSION_TIMEOUT_MS);
    if (post && strcmp(post, expected) == 0) {
        ESP_LOGI(TAG, "OTA verified, agent ver=%s", post);
        ota_screen_set_status("Done");
        vTaskDelay(pdMS_TO_TICKS(1000));
        ota_screen_hide();
        return ESP_OK;
    }
    ESP_LOGE(TAG, "OTA wrote but agent reports %s (expected %s)",
             post ? post : "<none>", expected);
    char line[96];
    snprintf(line, sizeof(line), "Wrote OK but ver=%s", post ? post : "<none>");
    ota_screen_set_status_error(line);
    vTaskDelay(pdMS_TO_TICKS(8000));
    ota_screen_hide();
    return ESP_FAIL;
}

#endif  /* CONFIG_BT_AGENT_OTA_ENABLED */
