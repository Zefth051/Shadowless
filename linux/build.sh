#!/usr/bin/env bash
set -euo pipefail
project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${SHADOWLESS_BUILD_DIR:-"$project_root/.qa/linux-build"}
if [[ -z ${SHADOWLESS_LAYERSHELL_PREFIX:-} && -f "$project_root/.qa/deps/usr/lib/libLayerShellQtInterface.so" ]]; then
    export SHADOWLESS_LAYERSHELL_PREFIX="$project_root/.qa/deps/usr"
fi
mkdir -p -- "$build_dir"
cd -- "$build_dir"
qmake6 "$project_root/linux/Shadowless.pro"
make qmake_all
make -j"${SHADOWLESS_BUILD_JOBS:-4}"
./tests/shadowless-gsr-tests
./tests/overlays/shadowless-overlay-tests
DBUS_SESSION_BUS_ADDRESS=unix:path=/dev/null QT_QPA_PLATFORM=offscreen ./tests/ui/shadowless-ui-tests
