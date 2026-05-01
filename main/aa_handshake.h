#pragma once

#include "aa_tls.h"
#include "esp_err.h"

/* Run the AA control-channel handshake on an already-accepted TCP socket:
 *   1. send VersionRequest, receive VersionResponse
 *   2. drive TLS handshake by ping-ponging SSL_HANDSHAKE frames
 *   3. send AuthComplete{status=OK}
 * Caller owns the aa_tls_t — must be uninitialised on entry; left ready
 * for encrypted reads/writes on success, or already deinit'd on failure. */
esp_err_t aa_handshake_run(int sock, aa_tls_t *tls);
