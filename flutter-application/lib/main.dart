import 'package:flutter/material.dart';

import 'app.dart';
import 'ble/ble_service.dart';
import 'coordinator.dart';
import 'i18n/strings.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Coordinator.instance.start();
  final locale = LocaleNotifier();
  await locale.load();
  // Fire-and-forget — if a head unit is saved, kick off the OS-level
  // autoConnect immediately. The UI shows "connecting…" until the link
  // comes up, no scanner step required.
  BleService.instance.resumeIfPaired();
  runApp(LocaleScope(
    notifier: locale,
    child: const AaBridgeApp(),
  ));
}
