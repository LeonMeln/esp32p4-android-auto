#!/usr/bin/env bash
# Stage the freshly-built P4 firmware image into the Flutter app's assets so
# it gets bundled into the APK for on-device (over-WiFi) OTA flashing.
#
# Copies:
#   build/esp32p4_android_auto.bin -> flutter-application/assets/firmware/esp32p4_android_auto.bin
#   version.txt                    -> flutter-application/assets/firmware/version.txt
#
# Run after `idf.py build` and before `flutter build apk`. Keeps the bundled
# binary and the version string the app shows in lockstep with what was built.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT}/build/esp32p4_android_auto.bin"
VER="${ROOT}/version.txt"
DEST="${ROOT}/flutter-application/assets/firmware"

if [[ ! -f "$BIN" ]]; then
    echo "stage_firmware_asset: $BIN not found — run 'idf.py build' first" >&2
    exit 1
fi

mkdir -p "$DEST"
cp "$BIN" "$DEST/esp32p4_android_auto.bin"
# First line of version.txt only (strip any trailing blank lines / comments).
head -n1 "$VER" | tr -d '[:space:]' > "$DEST/version.txt"

echo "stage_firmware_asset: bundled $(wc -c < "$BIN" | tr -d ' ') bytes, version $(cat "$DEST/version.txt")"
