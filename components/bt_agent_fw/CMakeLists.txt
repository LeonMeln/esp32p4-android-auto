# BT agent firmware blob — embedded into the host bin and flashed to the
# D1 Mini ESP32 over UART by main/bt_agent_ota.c on version mismatch.
#
# Drop the agent's bt_agent_fw.bin (output of its `idf.py build`, the
# single combined image: bootloader+partition_table+app — produced by
# `idf.py merge-bin`) into this directory before enabling
# CONFIG_BT_AGENT_OTA_ENABLED.

set(_blob "bt_agent_fw.bin")

if(CONFIG_BT_AGENT_OTA_ENABLED)
    if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/${_blob}")
        message(FATAL_ERROR
            "BT_AGENT_OTA_ENABLED=y but ${CMAKE_CURRENT_LIST_DIR}/${_blob} "
            "is missing. Build the BT agent firmware (esptool merge-bin) "
            "and copy the merged image here.")
    endif()
    idf_component_register(
        SRCS "bt_agent_fw.c"
        INCLUDE_DIRS "."
        EMBED_FILES "${_blob}"
    )
else()
    # Stub registration so the symbols exist (returning empty span) but no
    # binary is required and the dep graph stays clean.
    idf_component_register(
        SRCS "bt_agent_fw.c"
        INCLUDE_DIRS "."
    )
endif()
