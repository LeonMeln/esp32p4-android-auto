/*
    Copyright 2022 Benjamin Vedder benjamin@vedder.se
    Adapted from VESC firmware (GPL-3.0).

    VESC CAN/COMM protocol enums and status structs. Subset used by the
    display: CAN_PACKET_*, COMM_PACKET_* IDs and the per-controller
    realtime status messages.
*/

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CAN_PACKET_SET_DUTY                     = 0,
    CAN_PACKET_SET_CURRENT                  = 1,
    CAN_PACKET_SET_CURRENT_BRAKE            = 2,
    CAN_PACKET_SET_RPM                      = 3,
    CAN_PACKET_SET_POS                      = 4,
    CAN_PACKET_FILL_RX_BUFFER               = 5,
    CAN_PACKET_FILL_RX_BUFFER_LONG          = 6,
    CAN_PACKET_PROCESS_RX_BUFFER            = 7,
    CAN_PACKET_PROCESS_SHORT_BUFFER         = 8,
    CAN_PACKET_STATUS                       = 9,
    CAN_PACKET_SET_CURRENT_REL              = 10,
    CAN_PACKET_SET_CURRENT_BRAKE_REL        = 11,
    CAN_PACKET_SET_CURRENT_HANDBRAKE        = 12,
    CAN_PACKET_SET_CURRENT_HANDBRAKE_REL    = 13,
    CAN_PACKET_STATUS_2                     = 14,
    CAN_PACKET_STATUS_3                     = 15,
    CAN_PACKET_STATUS_4                     = 16,
    CAN_PACKET_PING                         = 17,
    CAN_PACKET_PONG                         = 18,
    CAN_PACKET_DETECT_APPLY_ALL_FOC         = 19,
    CAN_PACKET_DETECT_APPLY_ALL_FOC_RES     = 20,
    CAN_PACKET_CONF_CURRENT_LIMITS          = 21,
    CAN_PACKET_CONF_STORE_CURRENT_LIMITS    = 22,
    CAN_PACKET_CONF_CURRENT_LIMITS_IN       = 23,
    CAN_PACKET_CONF_STORE_CURRENT_LIMITS_IN = 24,
    CAN_PACKET_CONF_FOC_ERPMS               = 25,
    CAN_PACKET_CONF_STORE_FOC_ERPMS         = 26,
    CAN_PACKET_STATUS_5                     = 27,
    CAN_PACKET_POLL_TS5700N8501_STATUS      = 28,
    CAN_PACKET_CONF_BATTERY_CUT             = 29,
    CAN_PACKET_CONF_STORE_BATTERY_CUT       = 30,
    CAN_PACKET_SHUTDOWN                     = 31,
    CAN_PACKET_STATUS_6                     = 58,
    CAN_PACKET_NOTIFY_BOOT                  = 57,
    CAN_PACKET_MAKE_ENUM_32_BITS            = 0xFFFFFFFF,
} CAN_PACKET_ID;

typedef enum {
    HW_TYPE_VESC = 0,
    HW_TYPE_VESC_BMS,
    HW_TYPE_CUSTOM_MODULE,
} HW_TYPE;

typedef enum {
    COMM_FW_VERSION                 = 0,
    COMM_GET_VALUES                 = 4,
    COMM_SET_DUTY                   = 5,
    COMM_SET_CURRENT                = 6,
    COMM_SET_CURRENT_BRAKE          = 7,
    COMM_SET_RPM                    = 8,
    COMM_REBOOT                     = 29,
    COMM_ALIVE                      = 30,
    COMM_FORWARD_CAN                = 34,
    COMM_GET_VALUES_SETUP           = 47,
    COMM_GET_VALUES_SELECTIVE       = 50,
    COMM_GET_VALUES_SETUP_SELECTIVE = 51,
    COMM_LISP_GET_STATS             = 134,
} COMM_PACKET_ID;

typedef struct {
    int      id;
    uint32_t rx_time;
    float    rpm;
    float    current;
    float    duty;
} can_status_msg;

typedef struct {
    int      id;
    uint32_t rx_time;
    float    amp_hours;
    float    amp_hours_charged;
} can_status_msg_2;

typedef struct {
    int      id;
    uint32_t rx_time;
    float    watt_hours;
    float    watt_hours_charged;
} can_status_msg_3;

typedef struct {
    int      id;
    uint32_t rx_time;
    float    temp_fet;
    float    temp_motor;
    float    current_in;
    float    pid_pos_now;
} can_status_msg_4;

typedef struct {
    int      id;
    uint32_t rx_time;
    float    v_in;
    int32_t  tacho_value;
} can_status_msg_5;

typedef struct {
    int      id;
    uint32_t rx_time;
    float    adc_1;
    float    adc_2;
    float    adc_3;
    float    ppm;
} can_status_msg_6;

/* Subset of SETUP_VALUES the dashboard cares about. Filled by
 * vesc_rt_data.c from COMM_GET_VALUES_SETUP_SELECTIVE responses. */
typedef struct {
    float    temp_mos;
    float    temp_motor;
    float    current_motor;
    float    current_in;
    float    duty_now;
    float    rpm;
    float    speed;              /* m/s */
    float    v_in;
    float    battery_level;      /* 0..1 */
    float    amp_hours;
    float    amp_hours_charged;
    float    watt_hours;
    float    watt_hours_charged;
    float    tachometer;         /* meters, signed */
    float    tachometer_abs;     /* meters, trip */
    float    position;
    uint8_t  fault_code;
    uint8_t  vesc_id;
    uint8_t  num_vescs;
    float    battery_wh;
    uint32_t odometer;           /* meters */
    uint32_t uptime_ms;
    uint32_t rx_time;            /* esp_timer_get_time()/1000 */
} vesc_setup_values_t;

#define VESC_FW_VERSION_MAJOR  6
#define VESC_FW_VERSION_MINOR  5
#define VESC_HW_NAME           "ESP32-P4 AA + VESC Display"

#define VESC_PACKET_MAX_PL_LEN 512

#ifdef __cplusplus
}
#endif
