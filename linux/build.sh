#!/usr/bin/env bash
set -euo pipefail
project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${SHADOWLESS_BUILD_DIR:-"$project_root/.qa/linux-build"}
if [[ -z ${SHADOWLESS_LAYERSHELL_PREFIX:-} && -f "$project_root/.qa/deps/usr/lib/libLayerShellQtInterface.so" ]]; then
    export SHADOWLESS_LAYERSHELL_PREFIX="$project_root/.qa/deps/usr"
fi
mkdir -p -- "$build_dir"
"$project_root/linux/engine/build.sh"
cd -- "$build_dir"
qmake6 "$project_root/linux/Shadowless.pro" "SHADOWLESS_PUBLIC_UPDATES=${SHADOWLESS_PUBLIC_UPDATES:-false}"
make qmake_all
make -j"${SHADOWLESS_BUILD_JOBS:-4}"
./tests/shadowless-gsr-tests
./tests/overlays/shadowless-overlay-tests
# Keep test clips and Trash on the same filesystem, including container mounts.
mkdir -p "$build_dir/test-data" "$build_dir/test-tmp"
XDG_DATA_HOME="$build_dir/test-data" TMPDIR="$build_dir/test-tmp" \
    DBUS_SESSION_BUS_ADDRESS=unix:path=/dev/null QT_QPA_PLATFORM=offscreen ./tests/ui/shadowless-ui-tests
./tests/player/shadowless-player-tests
QT_QPA_PLATFORM=offscreen ./tests/playback/shadowless-playback-tests
QT_QPA_PLATFORM=offscreen ./tests/preview/shadowless-preview-tests
./tests/desktop_entry/shadowless-desktop-entry-check
./tests/updates/shadowless-update-tests
