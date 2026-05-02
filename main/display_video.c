#include "display_video.h"

#include <string.h>

#include "bsp/esp-bsp.h"
#include "driver/ppa.h"
#include "esp_cache.h"
#include "esp_check.h"
#include "esp_private/esp_cache_private.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ota_screen.h"

static const char *TAG = "display_video";

#define ALIGN_UP(num, align) (((num) + ((align) - 1)) & ~((align) - 1))

/* Panel-native dimensions. BSP defines them as portrait (H=480 W=800 in
 * native coords). The OTA screen rotates LVGL output by 90° so the user
 * looks at landscape; for dummy-draw video the rotation has to be done by
 * PPA, since dummy_draw_blit bypasses the LVGL render pipeline. */
#define PANEL_NATIVE_W   BSP_LCD_H_RES   /* 480 */
#define PANEL_NATIVE_H   BSP_LCD_V_RES   /* 800 */

/* User-facing landscape resolution after rotation. */
#define USER_W           800
#define USER_H           480

static lv_display_t       *s_disp;
static esp_lcd_panel_handle_t s_panel;
static ppa_client_handle_t s_ppa;
static size_t              s_cache_line;
static void               *s_fb[3];
static int                 s_fb_count;
static int                 s_fb_idx;
static bool                s_dummy_draw_on;

esp_err_t display_video_init(void)
{
    if (s_panel) return ESP_OK;

    s_disp = (lv_display_t *)ota_screen_get_display();
    if (!s_disp) {
        ESP_LOGW(TAG, "no LVGL display — video will be silent");
        return ESP_ERR_INVALID_STATE;
    }

    s_panel = bsp_display_get_panel_handle();
    if (!s_panel) {
        ESP_LOGE(TAG, "bsp_display_get_panel_handle returned NULL");
        return ESP_FAIL;
    }

    /* Pull whatever number of LCD framebuffers BSP allocated. We don't
     * own these — they're panel-attached SPIRAM buffers wired to DMA. */
    s_fb_count = CONFIG_BSP_LCD_DPI_BUFFER_NUMS;
    if (s_fb_count > 3) s_fb_count = 3;
    esp_err_t err = ESP_FAIL;
    if (s_fb_count == 3) {
        err = esp_lcd_dpi_panel_get_frame_buffer(s_panel, 3,
                                                 &s_fb[0], &s_fb[1], &s_fb[2]);
    } else if (s_fb_count == 2) {
        err = esp_lcd_dpi_panel_get_frame_buffer(s_panel, 2,
                                                 &s_fb[0], &s_fb[1]);
    } else {
        err = esp_lcd_dpi_panel_get_frame_buffer(s_panel, 1, &s_fb[0]);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "get_frame_buffer: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "got %d LCD framebuffers (%p, %p, %p)",
             s_fb_count, s_fb[0], s_fb[1], s_fb[2]);

    ppa_client_config_t ppa_cfg = { .oper_type = PPA_OPERATION_SRM };
    err = ppa_register_client(&ppa_cfg, &s_ppa);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ppa_register_client: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &s_cache_line);
    if (err != ESP_OK) s_cache_line = 64;

    ESP_LOGI(TAG, "ready (panel %dx%d native, user %dx%d landscape)",
             PANEL_NATIVE_W, PANEL_NATIVE_H, USER_W, USER_H);
    return ESP_OK;
}

esp_err_t display_video_show_yuv420(const uint8_t *yuv,
                                    uint16_t src_w, uint16_t src_h)
{
    if (!s_panel || !s_ppa || !s_disp) return ESP_ERR_INVALID_STATE;
    if (!yuv || src_w == 0 || src_h == 0) return ESP_ERR_INVALID_ARG;

    /* Lazy switch to dummy-draw on first frame so the OTA UI stays visible
     * until video actually arrives. */
    if (!s_dummy_draw_on) {
        if (esp_lv_adapter_set_dummy_draw(s_disp, true) == ESP_OK) {
            s_dummy_draw_on = true;
            ESP_LOGI(TAG, "switched LVGL adapter to dummy-draw");
        }
    }

    int idx = s_fb_idx;
    s_fb_idx = (s_fb_idx + 1) % s_fb_count;

    /* PPA: YUV420 source → RGB565 panel-native, rotated 90° so the user
     * sees the source landscape upright on the physically-landscape panel.
     * Source at 854×480 (or whatever phone really sends) is wider than the
     * 800-tall rotated target — PPA scales to fit. */
    size_t out_buf_size = ALIGN_UP(PANEL_NATIVE_W * PANEL_NATIVE_H * 2,
                                   s_cache_line);
    ppa_srm_oper_config_t op = {
        .in = {
            .buffer = yuv,
            .pic_w = src_w,
            .pic_h = src_h,
            .block_w = src_w,
            .block_h = src_h,
            .block_offset_x = 0,
            .block_offset_y = 0,
            .srm_cm = PPA_SRM_COLOR_MODE_YUV420,
            .yuv_range = COLOR_RANGE_LIMIT,
            .yuv_std   = COLOR_CONV_STD_RGB_YUV_BT601,
        },
        .out = {
            .buffer = s_fb[idx],
            .buffer_size = out_buf_size,
            .pic_w = PANEL_NATIVE_W,
            .pic_h = PANEL_NATIVE_H,
            .block_offset_x = 0,
            .block_offset_y = 0,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        /* 90° CW maps a landscape source onto the portrait-native panel so
         * the user, holding the device in landscape, sees the image upright.
         * scale_x is from-source-X to to-output-X-after-rotation. After
         * rotation source W (854) becomes output H, and source H (480)
         * becomes output W; scale to fit (800/854 vertically, 480/480 = 1
         * horizontally). */
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_90,
        .scale_x = (float)PANEL_NATIVE_W / (float)src_h,
        .scale_y = (float)PANEL_NATIVE_H / (float)src_w,
        .mirror_x = false,
        .mirror_y = false,
        .rgb_swap = 0,
        .byte_swap = 0,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    esp_err_t err = ppa_do_scale_rotate_mirror(s_ppa, &op);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ppa: %s (src %ux%u)", esp_err_to_name(err),
                 (unsigned)src_w, (unsigned)src_h);
        return err;
    }

    /* Coordinates are in user-facing landscape orientation; the LVGL
     * adapter knows about its 90° config and translates to panel-native
     * before the DMA blit. */
    err = esp_lv_adapter_dummy_draw_blit(s_disp, 0, 0, USER_W, USER_H,
                                         s_fb[idx], true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "dummy_draw_blit: %s", esp_err_to_name(err));
    }
    return err;
}
