#pragma once

#define MODE_BT_CLASSIC       1
#define MODE_WIRELESS_HELPER  2

#ifndef CONNECTION_MODE
#define CONNECTION_MODE MODE_WIRELESS_HELPER
#endif

/* Real AA head unit listens on 5288. Wireless Helper APK has this port
 * hardcoded — it grabs only the IP from mDNS and ignores the published port. */
#define AA_TCP_PORT            5288
#define AA_MDNS_HOSTNAME       "android-auto"
#define AA_MDNS_INSTANCE_NAME  "ESP32-P4 Android Auto"
/* Wireless Helper APK browses for _aawireless._tcp; this is the de-facto
 * service type used by hostapd-based AA dongles. */
#define AA_MDNS_SERVICE_TYPE   "_aawireless"
#define AA_MDNS_PROTO          "_tcp"
