#pragma once

#include "aa_tls.h"
#include "esp_err.h"

/* Post-auth message loop. Reads encrypted control-channel messages from the
 * peer, decrypts them, dispatches: ServiceDiscoveryRequest, ChannelOpenRequest,
 * PingRequest, etc. Replies via TLS-encrypted bulk frames.
 *
 * Blocks until the peer closes or an error occurs. The TLS context must be
 * already past handshake (i.e. passed through aa_handshake_run successfully). */
esp_err_t aa_service_run(int sock, aa_tls_t *tls);
