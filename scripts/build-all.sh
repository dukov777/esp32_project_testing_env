#!/usr/bin/env bash
# Build every test app for every relevant target.
#   Linux host:  all three apps  -> test_apps/<app>/build_linux/
#   ESP32-P4:    integration app -> test_apps/test_integration_AB/build_esp32p4/
#
# Usage:  scripts/build-all.sh [linux|esp32p4|all]
#         (default: all)

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [[ -z "${IDF_PATH:-}" ]]; then
    echo "ERROR: IDF_PATH not set. Run '. \$IDF_PATH/export.sh' first." >&2
    exit 1
fi

WHAT="${1:-all}"

build_linux() {
    echo "=== Building all test apps for linux ==="
    for app in test_apps/*/; do
        echo "--- $app ---"
        idf.py -C "$app" -B "${app}build_linux" --preview set-target linux build
    done
}

build_esp32p4() {
    echo "=== Building integration app for esp32p4 ==="
    idf.py -C test_apps/test_integration_AB \
           -B test_apps/test_integration_AB/build_esp32p4 \
           set-target esp32p4 build
}

case "$WHAT" in
    linux)    build_linux ;;
    esp32p4)  build_esp32p4 ;;
    all)      build_linux; build_esp32p4 ;;
    *)        echo "Usage: $0 [linux|esp32p4|all]" >&2; exit 2 ;;
esac

echo "=== Done ==="
