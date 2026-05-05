# GUI Guider VESC dashboard, brought verbatim from
# <repo>/Super_VESC_Display/{generated,custom}.  Built against LVGL 8.3
# (matches the original GUI Guider project — no API porting needed).
#
# We do NOT define LV_REALDEVICE: that gates out the vesc_limits /
# vesc_battery_calc backend wiring inside custom.c, and switches
# settings_wrapper.c to its in-memory SIMULATOR_MODE defaults. Result:
# the dashboard renders + the Settings menu is interactive, but values
# are placeholders until the VESC backend is integrated.

file(GLOB SVD_GENERATED  "generated/*.c")
file(GLOB SVD_FONTS      "generated/guider_fonts/*.c")
file(GLOB SVD_IMAGES     "generated/images/*.c")
file(GLOB SVD_CUSTOM     "custom/*.c")

idf_component_register(
    SRCS
        ${SVD_GENERATED}
        ${SVD_FONTS}
        ${SVD_IMAGES}
        ${SVD_CUSTOM}
        "vesc_ui.c"
    INCLUDE_DIRS
        "include"
        "generated"
        "custom"
    REQUIRES
        lvgl__lvgl
)

# Generated images use:
#   #if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#   #include "lvgl.h"
#   #else
#   #include "lvgl/lvgl.h"
#   #endif
# IDF's lvgl component exposes "lvgl.h", not "lvgl/lvgl.h" — set the flag.
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)
