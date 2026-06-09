#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

/* Attaches the web file-manager URI handlers to an already-running
 * httpd instance (the one started by ota_http_start, typically). Safe
 * to call once; second call returns ESP_ERR_INVALID_STATE.
 *
 * Endpoints (all under /files, gated by CONFIG_OTA_HTTP_ENABLED since
 * the file manager shares the OTA HTTP server):
 *   GET  /files                  HTML SPA — directory browser + actions
 *   GET  /files/api/list?path=…  JSON listing of one directory
 *   GET  /files/api/download?path=…  stream a file as octet-stream
 *   POST /files/api/upload?path=…    raw body bytes → file at full path
 *   POST /files/api/rename       JSON {src, dst}
 *   POST /files/api/delete       JSON {path}   (unlink file or rmdir empty dir)
 *   POST /files/api/mkdir        JSON {path}
 *
 * All `path` parameters MUST be under BSP_SD_MOUNT_POINT and may not
 * contain ".." segments — anything else gets a 400 back. */
esp_err_t files_http_register(httpd_handle_t server);
