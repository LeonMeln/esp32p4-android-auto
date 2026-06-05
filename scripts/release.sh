#!/usr/bin/env bash
# One-shot release helper: bump versions, build the P4 firmware, bundle it into
# the Flutter companion app, build the APK, and stage versioned artifacts.
#
# Steps:
#   1. bump version.txt (P4 firmware) and flutter-application/pubspec.yaml (app)
#   2. idf.py reconfigure + build         (reconfigure so cmake picks up PROJECT_VER)
#   3. scripts/stage_firmware_asset.sh    (copy fresh bin → app assets/firmware/)
#   4. flutter build apk --release        (bundles the firmware for over-WiFi OTA)
#   5. copy artifacts → release/esp32p4_android_auto-<fw>.bin + aa_bridge-<app>.apk
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

# --- ensure idf.py / flutter are available ------------------------------------
if ! command -v idf.py >/dev/null 2>&1; then
    if [[ -n "${IDF_PATH:-}" && -f "$IDF_PATH/export.sh" ]]; then
        # shellcheck disable=SC1091
        . "$IDF_PATH/export.sh" >/dev/null
    fi
fi
command -v idf.py  >/dev/null 2>&1 || { echo "release: idf.py not found — run '. \$IDF_PATH/export.sh' first" >&2; exit 1; }
command -v flutter >/dev/null 2>&1 || { echo "release: flutter not found in PATH" >&2; exit 1; }

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

# --- build firmware -----------------------------------------------------------
echo "==> idf.py reconfigure + build"
idf.py reconfigure
idf.py build

# --- bundle into the app + build the APK --------------------------------------
echo "==> staging firmware into the Flutter app"
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
cp "build/esp32p4_android_auto.bin" "release/esp32p4_android_auto-${NEW_FW}.bin"
cp "$APK"                            "release/aa_bridge-${NEW_APP}.apk"

cat <<EOF

==> done.
    firmware : release/esp32p4_android_auto-${NEW_FW}.bin
    apk      : release/aa_bridge-${NEW_APP}.apk

Review, then commit:
    git add -A version.txt flutter-application/pubspec.yaml release/
    git commit -m "release: fw ${NEW_FW} + app ${NEW_APP}"
EOF
