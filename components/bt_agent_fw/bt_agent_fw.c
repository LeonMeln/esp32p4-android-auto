#include "bt_agent_fw.h"

#include "sdkconfig.h"

#ifdef CONFIG_BT_AGENT_OTA_ENABLED

/* EMBED_FILES with name "bt_agent_fw.bin" generates these symbols. */
extern const uint8_t _binary_bt_agent_fw_bin_start[] asm("_binary_bt_agent_fw_bin_start");
extern const uint8_t _binary_bt_agent_fw_bin_end[]   asm("_binary_bt_agent_fw_bin_end");

const uint8_t *bt_agent_fw_data(void) { return _binary_bt_agent_fw_bin_start; }
size_t         bt_agent_fw_size(void)
{
    return (size_t)(_binary_bt_agent_fw_bin_end - _binary_bt_agent_fw_bin_start);
}

#else

const uint8_t *bt_agent_fw_data(void) { return NULL; }
size_t         bt_agent_fw_size(void) { return 0; }

#endif
