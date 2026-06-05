#!/usr/bin/env bash
# Stage the freshly-built per-board P4 firmware images into the Flutter app's
# assets so they get bundled into the APK for on-device (over-WiFi) OTA. The app
# picks the matching image at flash time from the board model the head unit
# reports over BLE / GET /info (see lib/firmware/firmware_updater.dart).
#
# Copies, for each board:
#   build_<board>/esp32p4_android_auto.bin
#       -> flutter-application/assets/firmware/esp32p4_android_auto-<board>.bin
# and once:
#   version.txt -> flutter-application/assets/firmware/version.txt
#
# Run after building every board (scripts/build_board.sh <board> build) and
# before `flutter build apk`. Keeps the bundled binaries and the version string
# the app shows in lockstep with what was built.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VER="${ROOT}/version.txt"
DEST="${ROOT}/flutter-application/assets/firmware"
BOARDS=(waveshare jc4880)

mkdir -p "$DEST"

for board in "${BOARDS[@]}"; do
    BIN="${ROOT}/build_${board}/esp32p4_android_auto.bin"
    if [[ ! -f "$BIN" ]]; then
        echo "stage_firmware_asset: $BIN not found — run 'scripts/build_board.sh ${board} build' first" >&2
        exit 1
    fi
    cp "$BIN" "$DEST/esp32p4_android_auto-${board}.bin"
    echo "stage_firmware_asset: bundled ${board}: $(wc -c < "$BIN" | tr -d ' ') bytes"
done

# First line of version.txt only (strip any trailing blank lines / comments).
head -n1 "$VER" | tr -d '[:space:]' > "$DEST/version.txt"
echo "stage_firmware_asset: version $(cat "$DEST/version.txt")"
