#!/usr/bin/env bash
# Wipe every build / test / cache artefact. Equivalent to `git clean -fdX`
# but explicit (so you can read what it does before running it).
#
# Usage:  scripts/clean-all.sh [--dry-run]

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

DRY=0
[[ "${1:-}" == "--dry-run" ]] && DRY=1

zap() {
    if (( DRY )); then
        printf 'WOULD: rm -rf %s\n' "$@"
    else
        rm -rf "$@"
    fi
}

zap_files() {
    if (( DRY )); then
        printf 'WOULD: rm -f %s\n' "$@"
    else
        rm -f "$@"
    fi
}

echo "=== tests/apps build dirs ==="
mapfile -t dirs < <(find tests/apps -maxdepth 2 -type d \
    \( -name 'build' -o -name 'build_linux' -o -name 'build_esp32p4' \))
[[ ${#dirs[@]} -gt 0 ]] && zap "${dirs[@]}" || echo "(none)"

echo "=== tests/apps sdkconfig snapshots ==="
mapfile -t files < <(find tests/apps -maxdepth 2 -type f \
    \( -name 'sdkconfig' -o -name 'sdkconfig.old' \))
[[ ${#files[@]} -gt 0 ]] && zap_files "${files[@]}" || echo "(none)"

echo "=== Per-app mock copies (regenerated each configure) ==="
mapfile -t comp_dirs < <(find tests/apps -maxdepth 2 -type d -name components)
[[ ${#comp_dirs[@]} -gt 0 ]] && zap "${comp_dirs[@]}" || echo "(none)"

echo "=== pytest / python caches ==="
mapfile -t caches < <(find . -name __pycache__ -type d -not -path './.git/*')
caches+=(.pytest_cache)
zap "${caches[@]}"

echo "=== pytest-embedded logs (/tmp) ==="
zap /tmp/pytest-embedded

echo "=== junit reports ==="
mapfile -t reports < <(find . -maxdepth 2 -name 'results-*.xml' -not -path './.git/*')
[[ ${#reports[@]} -gt 0 ]] && zap_files "${reports[@]}" || echo "(none)"

echo "=== Done ==="
(( DRY )) && echo "(dry-run; nothing actually deleted)"
