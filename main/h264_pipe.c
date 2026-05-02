#include "h264_pipe.h"

#include <string.h>

#include "display_video.h"
#include "esp_h264_dec.h"
#include "esp_h264_dec_param.h"
#include "esp_h264_dec_sw.h"
#include "esp_h264_types.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"

static const char *TAG = "h264_pipe";

/* The ring is byte-stream — one xRingbufferReceive call returns whatever
 * contiguous bytes are available, which we then feed to the decoder which
 * does its own NAL framing via Annex B start codes. This decouples AAP
 * frame boundaries from decoder calls, so a phone-side fragmented frame
 * that we already reassembled into one push() can still be split across
 * several decode loops without losing bytes. 96 KiB headroom for ~1.6 s
 * at 480p worst-case (60 KiB/frame I-frame * 30 fps with hiccups). */
#define H264_RING_SIZE_BYTES   (96 * 1024)
#define H264_TASK_STACK_BYTES  (8 * 1024)
#define H264_TASK_PRIORITY     5

static RingbufHandle_t       s_ring;
static esp_h264_dec_handle_t s_dec;
static esp_h264_dec_param_handle_t s_dec_param;
static TaskHandle_t          s_task;

/* Stats — reported once per second from the decoder task. */
static uint32_t  s_decoded_frames;
static uint32_t  s_decode_errors;
static uint64_t  s_decode_total_us;

static void log_stats_once_per_second(void)
{
    static int64_t window_start_us;
    int64_t now = esp_timer_get_time();
    if (window_start_us == 0) {
        window_start_us = now;
        return;
    }
    if (now - window_start_us < 1000000) return;

    if (s_decoded_frames > 0 || s_decode_errors > 0) {
        ESP_LOGI(TAG, "decoded %u frames (avg %llu us/frame), errors %u",
                 (unsigned)s_decoded_frames,
                 (unsigned long long)(s_decoded_frames
                     ? (s_decode_total_us / s_decoded_frames) : 0),
                 (unsigned)s_decode_errors);
    }
    s_decoded_frames  = 0;
    s_decode_errors   = 0;
    s_decode_total_us = 0;
    window_start_us   = now;
}

static void decoder_task(void *arg)
{
    (void)arg;
    bool seen_resolution = false;

    while (true) {
        size_t got = 0;
        uint8_t *chunk = (uint8_t *)xRingbufferReceive(s_ring, &got,
                                                      pdMS_TO_TICKS(100));
        log_stats_once_per_second();
        if (!chunk || got == 0) {
            if (chunk) vRingbufferReturnItem(s_ring, chunk);
            continue;
        }

        /* Drain all NAL units the decoder can find in this chunk. The
         * upstream code treats `consume` as how many input bytes the
         * decoder ate — loop until empty or the decoder rejects. */
        esp_h264_dec_in_frame_t  in  = { .raw_data = { chunk, (uint32_t)got } };
        esp_h264_dec_out_frame_t out = { 0 };
        while (in.raw_data.len > 0) {
            int64_t t0 = esp_timer_get_time();
            esp_h264_err_t e = esp_h264_dec_process(s_dec, &in, &out);
            int64_t dt = esp_timer_get_time() - t0;

            if (e != ESP_H264_ERR_OK) {
                s_decode_errors++;
                ESP_LOGW(TAG, "dec_process err=%d at %u/%u bytes",
                         (int)e,
                         (unsigned)in.consume,
                         (unsigned)in.raw_data.len);
                break;
            }

            if (out.out_size > 0) {
                s_decoded_frames++;
                s_decode_total_us += (uint64_t)dt;
                esp_h264_resolution_t res = {0};
                bool have_res = (s_dec_param &&
                    esp_h264_dec_get_resolution(s_dec_param, &res)
                        == ESP_H264_ERR_OK);
                if (!seen_resolution && have_res) {
                    ESP_LOGI(TAG, "first frame %ux%u, %u bytes I420",
                             res.width, res.height, (unsigned)out.out_size);
                    seen_resolution = true;
                }
                if (have_res) {
                    /* Hand off to the display sink. PPA + dummy_draw_blit
                     * happen synchronously inside; if it stalls we'd see
                     * dropped frames before phone ack timeouts hit. */
                    display_video_show_yuv420(out.outbuf,
                                              res.width, res.height);
                }
            }

            if (in.consume == 0) {
                /* Decoder didn't make progress — bail to avoid spinning. */
                break;
            }
            in.raw_data.buffer += in.consume;
            in.raw_data.len    -= in.consume;
            in.consume = 0;
        }

        vRingbufferReturnItem(s_ring, chunk);
    }
}

esp_err_t h264_pipe_init(void)
{
    if (s_dec) return ESP_OK;

    esp_h264_dec_cfg_sw_t cfg = {
        .pic_type = ESP_H264_RAW_FMT_I420,
    };
    esp_h264_err_t e = esp_h264_dec_sw_new(&cfg, &s_dec);
    if (e != ESP_H264_ERR_OK) {
        ESP_LOGE(TAG, "esp_h264_dec_sw_new: %d", (int)e);
        return ESP_FAIL;
    }
    e = esp_h264_dec_open(s_dec);
    if (e != ESP_H264_ERR_OK) {
        ESP_LOGE(TAG, "esp_h264_dec_open: %d", (int)e);
        esp_h264_dec_del(s_dec);
        s_dec = NULL;
        return ESP_FAIL;
    }
    /* Param handle is optional (only used to read out resolution after the
     * first IDR). Failure here just means we won't log the resolution. */
    if (esp_h264_dec_sw_get_param_hd(s_dec, &s_dec_param) != ESP_H264_ERR_OK) {
        s_dec_param = NULL;
    }

    s_ring = xRingbufferCreate(H264_RING_SIZE_BYTES, RINGBUF_TYPE_BYTEBUF);
    if (!s_ring) {
        ESP_LOGE(TAG, "xRingbufferCreate failed");
        esp_h264_dec_close(s_dec);
        esp_h264_dec_del(s_dec);
        s_dec = NULL;
        return ESP_ERR_NO_MEM;
    }

    BaseType_t ok = xTaskCreate(decoder_task, "h264_dec",
                                H264_TASK_STACK_BYTES, NULL,
                                H264_TASK_PRIORITY, &s_task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "decoder ready (ring %u KiB)",
             (unsigned)(H264_RING_SIZE_BYTES / 1024));
    return ESP_OK;
}

void h264_pipe_push(const uint8_t *data, size_t len)
{
    if (!s_ring || !data || len == 0) return;
    /* Non-blocking: if the ring is full we'd rather drop a frame than
     * stall the AAP receive loop and trigger phone ack timeouts. */
    BaseType_t ok = xRingbufferSend(s_ring, data, len, 0);
    if (ok != pdTRUE) {
        ESP_LOGW(TAG, "ring full, dropped %u bytes", (unsigned)len);
    }
}
