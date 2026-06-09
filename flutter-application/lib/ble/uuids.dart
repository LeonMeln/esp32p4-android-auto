/// NotifBridge GATT service UUIDs — must mirror firmware main/notif_bridge.c.
library;

class NotifBridgeUuids {
  static const service = '7b4e4f00-3f8e-4d2a-9d5c-2c9f1a6e0001';
  static const charInbound = '7b4e4f00-3f8e-4d2a-9d5c-2c9f1a6e0002';
  static const charOutbound = '7b4e4f00-3f8e-4d2a-9d5c-2c9f1a6e0003';
  static const charStatus = '7b4e4f00-3f8e-4d2a-9d5c-2c9f1a6e0004';

  /// WRITE_NO_RSP, 2 bytes [hour, minute]. Optional: older head-unit
  /// firmware won't expose it, so the app probes for presence at discovery
  /// time and only starts pushing the clock when the characteristic exists.
  static const charTime = '7b4e4f00-3f8e-4d2a-9d5c-2c9f1a6e0005';

  /// READ. Returns `<ip>\n<port>\n<ssid>\n<password>\n<version>` so the app
  /// can auto-join the head unit's SoftAP and POST a bundled firmware image
  /// to `http://<ip>:<port>/ota`. Optional — absent on firmware without the
  /// OTA-from-app feature, in which case the app hides the update action.
  static const charOtaInfo = '7b4e4f00-3f8e-4d2a-9d5c-2c9f1a6e0006';

  /// WRITE | NOTIFY. BLE OTA control channel: the phone writes BEGIN/END/ABORT
  /// opcodes, the head unit notifies status (READY/PROGRESS/DONE/ERROR). Lets
  /// the app flash firmware straight over the BLE link with no WiFi/SoftAP
  /// hop. Optional — absent on firmware without BLE OTA, in which case the app
  /// offers only the WiFi path. See lib/firmware/ble_ota.dart.
  static const charOtaCtrl = '7b4e4f00-3f8e-4d2a-9d5c-2c9f1a6e0007';

  /// WRITE_NO_RSP. BLE OTA data channel: raw firmware bytes streamed
  /// sequentially after the BEGIN/READY handshake on [charOtaCtrl].
  static const charOtaData = '7b4e4f00-3f8e-4d2a-9d5c-2c9f1a6e0008';
}
