#!/usr/bin/env bash
set -euo pipefail
project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${SHADOWLESS_BUILD_DIR:-"$project_root/.qa/linux-build"}
export QT_QPA_PLATFORM=wayland
exec "$build_dir/app/shadowless" "$@"
