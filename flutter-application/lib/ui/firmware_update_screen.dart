import 'package:flutter/material.dart';

import '../ble/ble_service.dart';
import '../firmware/firmware_updater.dart';
import '../firmware/ota_info.dart';
import '../i18n/strings.dart';

/// Over-WiFi firmware update screen. Reads the head unit's SoftAP credentials
/// + version over BLE, then (on confirm) joins that AP and uploads the
/// firmware image bundled in the APK.
class FirmwareUpdateScreen extends StatefulWidget {
  const FirmwareUpdateScreen({super.key});

  @override
  State<FirmwareUpdateScreen> createState() => _FirmwareUpdateScreenState();
}

class _FirmwareUpdateScreenState extends State<FirmwareUpdateScreen> {
  final _updater = FirmwareUpdater();

  OtaInfo? _info;
  String? _bundled;
  bool _loading = true;
  bool _busy = false;
  UpdateState _state = const UpdateState(UpdatePhase.idle);

  @override
  void initState() {
    super.initState();
    _updater.state.listen((s) {
      if (mounted) setState(() => _state = s);
    });
    _load();
  }

  @override
  void dispose() {
    _updater.dispose();
    super.dispose();
  }

  Future<void> _load() async {
    final bundled = await FirmwareUpdater.bundledVersion();
    final info = BleService.instance.supportsOta
        ? await BleService.instance.readOtaInfo()
        : null;
    if (!mounted) return;
    setState(() {
      _bundled = bundled;
      _info = info;
      _loading = false;
    });
  }

  Future<void> _confirmAndFlash() async {
    final info = _info;
    if (info == null) return;
    final go = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(t(ctx, 'fw.warn.title')),
        content: Text(t(ctx, 'fw.warn.body')),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: Text(t(ctx, 'fw.cancel')),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: Text(t(ctx, 'fw.warn.go')),
          ),
        ],
      ),
    );
    if (go != true) return;
    setState(() => _busy = true);
    await _updater.run(info);
    if (mounted) setState(() => _busy = false);
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text(t(context, 'fw.title'))),
      body: _loading
          ? const Center(child: CircularProgressIndicator())
          : Padding(
              padding: const EdgeInsets.all(16),
              child: _body(context),
            ),
    );
  }

  Widget _body(BuildContext context) {
    if (BleService.instance.currentState != BleConnState.connected) {
      return _infoMessage(Icons.bluetooth_disabled, t(context, 'fw.disconnected'));
    }
    final info = _info;
    if (info == null) {
      return _infoMessage(Icons.info_outline, t(context, 'fw.unsupported'));
    }

    final bundled = _bundled ?? '?';
    final upToDate = info.version == bundled;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Card(
          child: Column(
            children: [
              ListTile(
                leading: const Icon(Icons.memory),
                title: Text(t(context, 'fw.device')),
                trailing: Text(info.version,
                    style: Theme.of(context).textTheme.titleMedium),
              ),
              const Divider(height: 1),
              ListTile(
                leading: const Icon(Icons.phone_android),
                title: Text(t(context, 'fw.bundled')),
                trailing: Text(bundled,
                    style: Theme.of(context).textTheme.titleMedium?.copyWith(
                          color: upToDate ? null : Colors.green,
                        )),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        if (upToDate)
          Text(t(context, 'fw.uptodate'),
              style: TextStyle(color: Colors.grey[600])),
        const SizedBox(height: 8),
        _progress(context),
        const Spacer(),
        FilledButton.icon(
          onPressed: _busy ? null : _confirmAndFlash,
          icon: _busy
              ? const SizedBox(
                  width: 18,
                  height: 18,
                  child: CircularProgressIndicator(strokeWidth: 2))
              : const Icon(Icons.system_update),
          label: Text(_busy ? t(context, 'fw.flashing') : t(context, 'fw.flash')),
        ),
      ],
    );
  }

  Widget _progress(BuildContext context) {
    final s = _state;
    if (s.phase == UpdatePhase.idle) return const SizedBox.shrink();
    final showBar = s.phase == UpdatePhase.uploading;
    final isError = s.phase == UpdatePhase.error;
    final isDone = s.phase == UpdatePhase.done;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        if (showBar) ...[
          LinearProgressIndicator(value: s.progress),
          const SizedBox(height: 4),
          Text('${(s.progress * 100).round()}%',
              textAlign: TextAlign.center),
          const SizedBox(height: 8),
        ],
        if (s.message != null)
          Text(
            isDone ? t(context, 'fw.done') : s.message!,
            textAlign: TextAlign.center,
            style: TextStyle(
              color: isError
                  ? Theme.of(context).colorScheme.error
                  : isDone
                      ? Colors.green
                      : null,
            ),
          ),
      ],
    );
  }

  Widget _infoMessage(IconData icon, String text) {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Icon(icon, size: 48, color: Colors.grey),
          const SizedBox(height: 12),
          Text(text, textAlign: TextAlign.center),
        ],
      ),
    );
  }
}
