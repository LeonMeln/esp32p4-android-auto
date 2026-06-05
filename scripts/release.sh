#!/usr/bin/env bash
# One-shot release helper: bump versions, build the P4 firmware for every
# supported board, bundle them into the Flutter companion app, build the APK,
# and stage versioned per-board artifacts.
#
# Steps:
#   1. bump version.txt (P4 firmware) and flutter-application/pubspec.yaml (app)
#   2. scripts/build_board.sh <board> reconfigure + build, for each board
#      (reconfigure so cmake picks up PROJECT_VER from version.txt)
#   3. scripts/stage_firmware_asset.sh    (copy fresh bins → app assets/firmware/)
#   4. flutter build apk --release        (bundles all firmwares for over-WiFi OTA;
#                                          the app auto-selects by board model)
#   5. copy artifacts → release/esp32p4_android_auto-<board>-<fw>.bin (per board)
#                       + release/aa_bridge-<app>.apk
#
# Usage:
#   scripts/release.sh                 # patch-bump both fw and app
#   scripts/release.sh 1.1.6 0.1.6     # explicit fw + app versions
#
# Does NOT commit. After it finishes, review and commit version.txt,
# flutter-application/pubspec.yaml, release/*, and your code changes.
#
# BT-agent firmware is versioned separately (tools/bt_agent + pack_fw_blobs.sh)
# and is NOT touched here — bump it only when tools/bt_agent/ changes.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

# Boards to build a firmware image for. Each must have a sdkconfig.defaults.<board>
# overlay and is built into its own build_<board>/ dir by scripts/build_board.sh.
BOARDS=(waveshare jc4880)

# --- ensure idf.py / flutter are available ------------------------------------
if ! command -v idf.py >/dev/null 2>&1; then
    if [[ -n "${IDF_PATH:-}" && -f "$IDF_PATH/export.sh" ]]; then
        # shellcheck disable=SC1091
        . "$IDF_PATH/export.sh" >/dev/null
    fi
fi
command -v idf.py  >/dev/null 2>&1 || { echo "release: idf.py not found — run '. \$IDF_PATH/export.sh' first" >&2; exit 1; }
command -v flutter >/dev/null 2>&1 || { echo "release: flutter not found in PATH" >&2; exit 1; }

# esptool (bundled with ESP-IDF) builds the single-file merged flash image used by
# release/flash.py. 'merge_bin' (underscore) is accepted by both esptool v4 (IDF) and v5.
if   command -v esptool    >/dev/null 2>&1; then ESPTOOL=(esptool)
elif command -v esptool.py >/dev/null 2>&1; then ESPTOOL=(esptool.py)
else ESPTOOL=(python -m esptool); fi

# --- resolve versions ---------------------------------------------------------
bump_patch() {  # 1.2.3 -> 1.2.4
    local v="$1" major minor patch
    major="${v%%.*}"; v="${v#*.}"; minor="${v%%.*}"; patch="${v#*.}"
    echo "${major}.${minor}.$((patch + 1))"
}

CUR_FW="$(head -n1 version.txt | tr -d '[:space:]')"
CUR_APP_FULL="$(grep -E '^version:' flutter-application/pubspec.yaml | awk '{print $2}')"
CUR_APP="${CUR_APP_FULL%%+*}"
CUR_BUILD="${CUR_APP_FULL##*+}"

NEW_FW="${1:-$(bump_patch "$CUR_FW")}"
NEW_APP="${2:-$(bump_patch "$CUR_APP")}"
NEW_BUILD=$((CUR_BUILD + 1))

echo "==> firmware : ${CUR_FW} -> ${NEW_FW}"
echo "==> app      : ${CUR_APP}+${CUR_BUILD} -> ${NEW_APP}+${NEW_BUILD}"

# --- bump version files -------------------------------------------------------
printf '%s\n' "$NEW_FW" > version.txt
# portable in-place edit (BSD + GNU sed both accept -i with a backup suffix)
sed -i.bak -E "s/^version:.*/version: ${NEW_APP}+${NEW_BUILD}/" flutter-application/pubspec.yaml
rm -f flutter-application/pubspec.yaml.bak

# --- build firmware for every board ------------------------------------------
for board in "${BOARDS[@]}"; do
    echo "==> build ${board}: reconfigure + build"
    scripts/build_board.sh "$board" reconfigure
    scripts/build_board.sh "$board" build
done

# --- bundle into the app + build the APK --------------------------------------
echo "==> staging firmwares into the Flutter app"
scripts/stage_firmware_asset.sh

echo "==> flutter build apk"
( cd flutter-application && flutter pub get && flutter build apk --release )

APK="flutter-application/build/app/outputs/flutter-apk/app-release.apk"
[[ -f "$APK" ]] || APK="$(find flutter-application/build -name 'app-release.apk' -print -quit 2>/dev/null || true)"
[[ -n "$APK" && -f "$APK" ]] || { echo "release: built APK not found" >&2; exit 1; }

# --- stage versioned artifacts (keep only the latest of each kind) ------------
echo "==> staging release artifacts"
mkdir -p release
rm -f release/esp32p4_android_auto-*.bin release/aa_bridge-*.apk
for board in "${BOARDS[@]}"; do
    cp "build_${board}/esp32p4_android_auto.bin" \
       "release/esp32p4_android_auto-${board}-${NEW_FW}.bin"

    # Single-file flasher image (bootloader + partition table + otadata + app),
    # written at offset 0x0 by release/flash.py. Offsets/files come straight from the
    # board's flasher_args.json; no flash flags are passed, so the mode/freq/size baked
    # into the bootloader header by the build are kept.
    pairs=$(python3 -c '
import json,sys
fa=json.load(open(sys.argv[1]))["flash_files"]; bdir=sys.argv[2]
print(" ".join("%s %s/%s"%(a,bdir,f) for a,f in sorted(fa.items(),key=lambda kv:int(kv[0],16))))
' "build_${board}/flasher_args.json" "build_${board}")
    # shellcheck disable=SC2086  # $pairs intentionally word-splits into <addr> <file> args
    "${ESPTOOL[@]}" --chip esp32p4 merge_bin \
        -o "release/esp32p4_android_auto-${board}-${NEW_FW}-merged.bin" \
        $pairs >/dev/null
done
cp "$APK"                            "release/aa_bridge-${NEW_APP}.apk"

echo
echo "==> done."
for board in "${BOARDS[@]}"; do
    echo "    firmware : release/esp32p4_android_auto-${board}-${NEW_FW}.bin"
    echo "    flasher  : release/esp32p4_android_auto-${board}-${NEW_FW}-merged.bin"
done
cat <<EOF
    apk      : release/aa_bridge-${NEW_APP}.apk
    USB flash: release/flash.command (Mac) / release/flash.bat (Windows)

Review, then commit:
    git add -A version.txt flutter-application/pubspec.yaml release/
    git commit -m "release: fw ${NEW_FW} + app ${NEW_APP}"
EOF
