cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32p4_android_auto)

# main/lv_conf.h is the single source of truth for LVGL configuration.
# It must reach every component that does '#include "lv_conf.h"' or
# '#include "lvgl.h"' (BSP, esp_lv_adapter, vesc_ui, Super_VESC_Display
# generated/custom files). Putting main/ on the global include path is
# the simplest way to satisfy that. Pair this with CONFIG_LV_CONF_SKIP=n
# in sdkconfig so the LVGL package picks up our header instead of
# generating its own from Kconfig.
idf_build_set_property(COMPILE_OPTIONS "-I${CMAKE_SOURCE_DIR}/main" APPEND)
