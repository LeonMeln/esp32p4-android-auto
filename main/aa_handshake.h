#pragma once

#include "esp_err.h"

/* Run the AA control-channel handshake on an already-accepted TCP socket:
 *   1. send VersionRequest, receive VersionResponse
 *   2. drive TLS handshake by ping-ponging SSL_HANDSHAKE frames
 *   3. send AuthComplete{status=OK}
 * Returns ESP_OK once auth is done; the caller can then receive encrypted
 * AA frames (not yet implemented). */
esp_err_t aa_handshake_run(int sock);
