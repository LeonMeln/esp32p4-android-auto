/// SoftAP credentials + OTA endpoint + running firmware version, read from
/// the head unit's NotifBridge OTA characteristic (...0006).
///
/// Wire form is newline-joined UTF-8:
///   `<ip>\n<port>\n<ssid>\n<password>\n<version>`
library;

import 'dart:convert';
import 'dart:typed_data';

class OtaInfo {
  const OtaInfo({
    required this.ip,
    required this.port,
    required this.ssid,
    required this.password,
    required this.version,
  });

  final String ip;
  final int port;
  final String ssid;
  final String password;

  /// Firmware version currently running on the head unit (e.g. "1.1.1").
  final String version;

  String get otaUrl => 'http://$ip:$port/ota';

  /// Parse the raw characteristic bytes. Returns null on a malformed payload
  /// (too few fields / unparseable port) so callers can fall back gracefully.
  static OtaInfo? parse(List<int> raw) {
    final parts = utf8.decode(Uint8List.fromList(raw), allowMalformed: true).split('\n');
    if (parts.length < 5) return null;
    final port = int.tryParse(parts[1].trim());
    if (port == null) return null;
    return OtaInfo(
      ip: parts[0].trim(),
      port: port,
      ssid: parts[2],
      password: parts[3],
      version: parts[4].trim(),
    );
  }

  @override
  String toString() => 'OtaInfo(ssid=$ssid, $otaUrl, v$version)';
}
