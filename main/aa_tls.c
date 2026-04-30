#include "aa_tls.h"

#include <string.h>

#include "aa_certs.h"
#include "esp_log.h"
#include "mbedtls/error.h"

static const char *TAG = "aa_tls";

/* mbedTLS BIO callbacks: we don't talk to a socket directly. Outbound
 * handshake bytes accumulate in t->tx_buf (caller drains them); inbound
 * bytes are written into t->rx_buf by aa_tls_feed_rx() and consumed here. */
static int bio_send(void *ctx, const unsigned char *buf, size_t len)
{
    aa_tls_t *t = (aa_tls_t *)ctx;
    if (t->tx_len + len > sizeof(t->tx_buf)) {
        return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
    }
    memcpy(t->tx_buf + t->tx_len, buf, len);
    t->tx_len += len;
    return (int)len;
}

static int bio_recv(void *ctx, unsigned char *buf, size_t len)
{
    aa_tls_t *t = (aa_tls_t *)ctx;
    size_t avail = t->rx_len - t->rx_off;
    if (avail == 0) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    size_t n = avail < len ? avail : len;
    memcpy(buf, t->rx_buf + t->rx_off, n);
    t->rx_off += n;
    /* Reset offset once drained so next feed starts at index 0. */
    if (t->rx_off == t->rx_len) {
        t->rx_off = 0;
        t->rx_len = 0;
    }
    return (int)n;
}

esp_err_t aa_tls_init(aa_tls_t *t)
{
    memset(t, 0, sizeof(*t));

    mbedtls_entropy_init(&t->entropy);
    mbedtls_ctr_drbg_init(&t->ctr_drbg);
    mbedtls_ssl_init(&t->ssl);
    mbedtls_ssl_config_init(&t->conf);
    mbedtls_x509_crt_init(&t->cert);
    mbedtls_pk_init(&t->pkey);

    static const char *seed = "aa_head_unit";
    int ret = mbedtls_ctr_drbg_seed(&t->ctr_drbg, mbedtls_entropy_func, &t->entropy,
                                    (const unsigned char *)seed, strlen(seed));
    if (ret) { ESP_LOGE(TAG, "ctr_drbg_seed: -0x%04x", -ret); goto err; }

    ret = mbedtls_x509_crt_parse(&t->cert,
                                 (const unsigned char *)AA_CERTIFICATE_PEM,
                                 sizeof(AA_CERTIFICATE_PEM));
    if (ret) { ESP_LOGE(TAG, "crt_parse: -0x%04x", -ret); goto err; }

    ret = mbedtls_pk_parse_key(&t->pkey,
                               (const unsigned char *)AA_PRIVATE_KEY_PEM,
                               sizeof(AA_PRIVATE_KEY_PEM),
                               NULL, 0, mbedtls_ctr_drbg_random, &t->ctr_drbg);
    if (ret) { ESP_LOGE(TAG, "pk_parse_key: -0x%04x", -ret); goto err; }

    ret = mbedtls_ssl_config_defaults(&t->conf,
                                      MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret) { ESP_LOGE(TAG, "config_defaults: -0x%04x", -ret); goto err; }

    /* aasdk doesn't verify either side; we mirror that. */
    mbedtls_ssl_conf_authmode(&t->conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&t->conf, mbedtls_ctr_drbg_random, &t->ctr_drbg);

    ret = mbedtls_ssl_conf_own_cert(&t->conf, &t->cert, &t->pkey);
    if (ret) { ESP_LOGE(TAG, "conf_own_cert: -0x%04x", -ret); goto err; }

    ret = mbedtls_ssl_setup(&t->ssl, &t->conf);
    if (ret) { ESP_LOGE(TAG, "ssl_setup: -0x%04x", -ret); goto err; }

    mbedtls_ssl_set_bio(&t->ssl, t, bio_send, bio_recv, NULL);
    return ESP_OK;

err:
    aa_tls_deinit(t);
    return ESP_FAIL;
}

void aa_tls_deinit(aa_tls_t *t)
{
    mbedtls_pk_free(&t->pkey);
    mbedtls_x509_crt_free(&t->cert);
    mbedtls_ssl_config_free(&t->conf);
    mbedtls_ssl_free(&t->ssl);
    mbedtls_ctr_drbg_free(&t->ctr_drbg);
    mbedtls_entropy_free(&t->entropy);
}

esp_err_t aa_tls_feed_rx(aa_tls_t *t, const uint8_t *data, size_t len)
{
    if (t->rx_len + len > sizeof(t->rx_buf)) {
        ESP_LOGE(TAG, "rx buffer overflow (%u + %u)", (unsigned)t->rx_len, (unsigned)len);
        return ESP_ERR_NO_MEM;
    }
    memcpy(t->rx_buf + t->rx_len, data, len);
    t->rx_len += len;
    return ESP_OK;
}

esp_err_t aa_tls_handshake_step(aa_tls_t *t,
                                uint8_t *out_buf, size_t out_capacity,
                                size_t *out_len,
                                bool *done)
{
    *done = false;
    *out_len = 0;

    int ret = mbedtls_ssl_handshake(&t->ssl);
    if (ret == 0) {
        *done = true;
    } else if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
               ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
        char errbuf[128];
        mbedtls_strerror(ret, errbuf, sizeof(errbuf));
        ESP_LOGE(TAG, "ssl_handshake: -0x%04x %s", -ret, errbuf);
        return ESP_FAIL;
    }

    if (t->tx_len > 0) {
        if (t->tx_len > out_capacity) {
            ESP_LOGE(TAG, "tx %u > capacity %u", (unsigned)t->tx_len, (unsigned)out_capacity);
            return ESP_ERR_NO_MEM;
        }
        memcpy(out_buf, t->tx_buf, t->tx_len);
        *out_len = t->tx_len;
        t->tx_len = 0;
    }
    return ESP_OK;
}
