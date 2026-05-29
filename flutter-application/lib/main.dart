import 'dart:async';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';

import 'app.dart';
import 'ble/ble_service.dart';
import 'bridge/foreground_bridge.dart';
import 'coordinator.dart';
import 'i18n/strings.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  if (kDebugMode) {
    // Floods logcat with FBP's internal state — what we actually need to
    // diagnose why a scan / connect tantrum failed on real hardware.
    FlutterBluePlus.setLogLevel(LogLevel.verbose, color: false);
  }
  if (Platform.isAndroid) {
    // Android 13+ blocks every notification until the user grants
    // POST_NOTIFICATIONS at runtime. Ask once on first launch — without
    // this the foreground-service pill silently never appears (the
    // process is alive, the system just drops every notify call).
    await Permission.notification.request();
    // Android 14+ refuses to start a `connectedDevice` FGS unless the
    // caller already holds BLUETOOTH_CONNECT (or another allow-listed
    // permission). Without this the very first startForeground() throws
    // SecurityException and kills the process before the UI ever paints.
    await [
      Permission.bluetoothConnect,
      Permission.bluetoothScan,
    ].request();
  }
  await Coordinator.instance.start();
  final locale = LocaleNotifier();
  await locale.load();
  // Start the foreground service after permissions are resolved so the
  // `connectedDevice` type validates. BleService upgrades the text to
  // "Connected" as soon as the link comes up.
  if (Platform.isAndroid &&
      await Permission.bluetoothConnect.isGranted) {
    unawaited(ForegroundBridge().start());
  }
  // Fire-and-forget — if a head unit is saved, kick off the OS-level
  // autoConnect immediately. The UI shows "connecting…" until the link
  // comes up, no scanner step required.
  BleService.instance.resumeIfPaired();
  runApp(LocaleScope(
    notifier: locale,
    child: const AaBridgeApp(),
  ));
}
