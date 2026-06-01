/// Orchestrates an over-WiFi firmware update of the head unit:
///   1. request location permission (Android needs it to join a WiFi by SSID)
///   2. join + bind the process to the head unit's SoftAP
///   3. POST the bundled firmware image to `http://<ip>:<port>/ota`
///   4. unbind so the phone returns to its normal network
///
/// The head unit writes the image to its spare OTA partition, verifies it and
/// reboots — so a successful upload ends with the BLE link dropping and the
/// device coming back on the new version a few seconds later.
library;

import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/services.dart' show PlatformException, rootBundle;
import 'package:permission_handler/permission_handler.dart';

import '../bridge/wifi_bridge.dart';
import 'ota_info.dart';

enum UpdatePhase { idle, permission, joining, uploading, verifying, done, error }

class UpdateState {
  const UpdateState(this.phase, {this.progress = 0, this.message});
  final UpdatePhase phase;

  /// 0..1 upload fraction, meaningful only while [phase] == uploading.
  final double progress;
  final String? message;
}

class FirmwareUpdater {
  FirmwareUpdater({WifiBridge? wifi}) : _wifi = wifi ?? WifiBridge();

  final WifiBridge _wifi;

  static const _asset = 'assets/firmware/esp32p4_android_auto.bin';
  static const _versionAsset = 'assets/firmware/version.txt';

  /// Version string of the firmware image bundled in this APK.
  static Future<String> bundledVersion() async =>
      (await rootBundle.loadString(_versionAsset)).trim();

  final _ctrl = StreamController<UpdateState>.broadcast();
  Stream<UpdateState> get state => _ctrl.stream;

  void _emit(UpdatePhase p, {double progress = 0, String? message}) =>
      _ctrl.add(UpdateState(p, progress: progress, message: message));

  /// Run the full flow against [info] (read from the head unit over BLE).
  /// Returns true on a confirmed flash. Never throws — failures surface
  /// through the [state] stream and the bool result.
  Future<bool> run(OtaInfo info) async {
    var joined = false;
    try {
      // WifiNetworkSpecifier shows its own system approval dialog and does
      // not require ACCESS_FINE_LOCATION, but some OEM builds still gate WiFi
      // ops on it — request best-effort and never block the flow on the
      // result.
      _emit(UpdatePhase.permission);
      try {
        await Permission.location.request();
      } catch (_) {}

      _emit(UpdatePhase.joining, message: 'Подключение к ${info.ssid}…');
      await _wifi.join(info.ssid, info.password);
      joined = true;

      final bytes = (await rootBundle.load(_asset)).buffer.asUint8List();
      _emit(UpdatePhase.uploading, progress: 0,
          message: 'Загрузка прошивки (${(bytes.length / 1024).round()} КБ)…');

      final ok = await _post(info.otaUrl, bytes);
      if (!ok) {
        _emit(UpdatePhase.error, message: 'Устройство отклонило прошивку');
        return false;
      }
      _emit(UpdatePhase.done, message: 'Прошивка принята — устройство перезагружается');
      return true;
    } on PlatformException catch (e) {
      _emit(UpdatePhase.error, message: e.message ?? 'Ошибка подключения к WiFi');
      return false;
    } catch (e) {
      _emit(UpdatePhase.error, message: '$e');
      return false;
    } finally {
      if (joined) {
        try {
          await _wifi.unbind();
        } catch (_) {}
      }
    }
  }

  Future<bool> _post(String url, List<int> bytes) async {
    final client = HttpClient()
      ..connectionTimeout = const Duration(seconds: 15);
    try {
      final req = await client.postUrl(Uri.parse(url));
      req.headers.contentType = ContentType('application', 'octet-stream');
      req.contentLength = bytes.length;

      const chunk = 16 * 1024;
      var sent = 0;
      while (sent < bytes.length) {
        final end = (sent + chunk) < bytes.length ? sent + chunk : bytes.length;
        req.add(bytes.sublist(sent, end));
        await req.flush();
        sent = end;
        _emit(UpdatePhase.uploading, progress: sent / bytes.length);
      }
      // Device verifies + commits after the body lands.
      _emit(UpdatePhase.verifying, progress: 1, message: 'Проверка образа…');
      final resp = await req.close().timeout(const Duration(seconds: 60));
      final body = await resp.transform(utf8.decoder).join();
      return resp.statusCode == 200 &&
          body.toLowerCase().contains('rebooting');
    } finally {
      client.close(force: true);
    }
  }

  void dispose() => _ctrl.close();
}
