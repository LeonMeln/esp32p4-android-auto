#!/usr/bin/env bash
# Build (or flash / monitor / size / …) the P4 firmware for a head-unit board.
# Each board gets its own build directory and sdkconfig so the two configs never
# clobber each other, and the per-board sdkconfig.defaults.<board> overlay is
# layered on top of the common sdkconfig.defaults (later file wins).
#
# Usage:
#   scripts/build_board.sh                       # build EVERY board (firmware images)
#   scripts/build_board.sh all [idf.py args...]  # same, optionally with custom args
#   scripts/build_board.sh waveshare build
#   scripts/build_board.sh jc4880 -p /dev/cu.usbmodem* flash monitor
#   scripts/build_board.sh jc4880 size
#
# With no board (or "all"), runs the given idf.py command — defaulting to
# `build` — for each board in turn. A plain `idf.py build` (no wrapper) still
# builds only Waveshare from the base sdkconfig.defaults.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

# Boards with a sdkconfig.defaults.<board> overlay. Keep in sync with release.sh.
BOARDS=(waveshare jc4880)

# Make idf.py available if the caller forgot to source export.sh.
if ! command -v idf.py >/dev/null 2>&1; then
    if [[ -n "${IDF_PATH:-}" && -f "$IDF_PATH/export.sh" ]]; then
        # shellcheck disable=SC1091
        . "$IDF_PATH/export.sh" >/dev/null
    fi
fi
command -v idf.py >/dev/null 2>&1 || {
    echo "build_board: idf.py not found — run '. \$IDF_PATH/export.sh' first" >&2; exit 1; }

# run_board <board> <idf.py args...>
run_board() {
    local board="$1"; shift
    local overlay="sdkconfig.defaults.${board}"
    [[ -f "$overlay" ]] || { echo "build_board: $overlay not found" >&2; exit 1; }
    idf.py -B "build_${board}" \
           -D SDKCONFIG="build_${board}/sdkconfig" \
           -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;${overlay}" \
           "$@"
}

board="${1:-all}"

case "$board" in
    all)
        # No board (or explicit "all"): run for every board. Default cmd: build.
        shift || true
        args=("$@"); [[ ${#args[@]} -gt 0 ]] || args=(build)
        for b in "${BOARDS[@]}"; do
            echo "==> build_board: ${b} -> idf.py ${args[*]}"
            run_board "$b" "${args[@]}"
        done
        ;;
    waveshare|jc4880)
        shift
        run_board "$board" "$@"
        ;;
    *)
        echo "usage: $0 [all|waveshare|jc4880] [idf.py args...]" >&2
        exit 2
        ;;
esac
