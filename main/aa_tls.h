#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/pk.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

/* TLS state machine that runs on top of an external transport
 * (we feed it AA SSL_HANDSHAKE payloads, it tells us what bytes to
 * wrap into the next outbound payload). Acts as the SSL **client** —
 * gearhead is the server, matching aasdk Cryptor::setConnectState. */
typedef struct {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cert;
    mbedtls_pk_context pkey;

    /* Inbound / outbound BIO buffers must fit at least one full TLS record.
     * Sized at 24 KiB to comfortably hold a 16 KiB TLS payload plus headroom —
     * H.264 I-frames from gearhead routinely arrive as ~16 KB ciphertext and
     * the previous 8 KiB sizing overflowed at the first keyframe. */
    uint8_t  rx_buf[24576];
    size_t   rx_len;
    size_t   rx_off;

    uint8_t  tx_buf[24576];
    size_t   tx_len;
} aa_tls_t;

esp_err_t aa_tls_init(aa_tls_t *t);
void      aa_tls_deinit(aa_tls_t *t);

/* Feed handshake bytes received from the peer (from an SSL_HANDSHAKE frame). */
esp_err_t aa_tls_feed_rx(aa_tls_t *t, const uint8_t *data, size_t len);

/* Drive the handshake one step. Drains pending TLS output into out_buf
 * (capacity out_capacity). out_len returns how many bytes the caller should
 * wrap in the next SSL_HANDSHAKE frame.
 * Sets *done = true once the handshake is complete. */
esp_err_t aa_tls_handshake_step(aa_tls_t *t,
                                uint8_t *out_buf, size_t out_capacity,
                                size_t *out_len,
                                bool *done);

/* Encrypt plaintext into one TLS record. Output goes to out_buf
 * (returns its length in *out_len). Reuses the BIO tx_buf. */
esp_err_t aa_tls_encrypt(aa_tls_t *t,
                         const uint8_t *plain, size_t plain_len,
                         uint8_t *out_buf, size_t out_capacity,
                         size_t *out_len);

/* Decrypt one TLS record (ciphertext) into out_buf. Pumps the data
 * through the rx BIO and consumes mbedtls_ssl_read until the record
 * is fully consumed. *out_len is the plaintext length. */
esp_err_t aa_tls_decrypt(aa_tls_t *t,
                         const uint8_t *cipher, size_t cipher_len,
                         uint8_t *out_buf, size_t out_capacity,
                         size_t *out_len);
