#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns pointer to the embedded BT-agent firmware image (merged
 * bootloader+partition_table+app produced by `esptool merge-bin`).
 * Returns NULL when CONFIG_BT_AGENT_OTA_ENABLED is unset (no blob
 * embedded — the file just contains a stub). */
const uint8_t *bt_agent_fw_data(void);

/* Length in bytes of the blob returned by bt_agent_fw_data(). 0 when
 * disabled. */
size_t bt_agent_fw_size(void);

#ifdef __cplusplus
}
#endif
