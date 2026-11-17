#!/usr/bin/env bash
# Build (incremental) and launch Shadowless for manual testing.
# Usage: ./run.sh [app args]    SHADOWLESS_SKIP_BUILD=1 skips the build step.
set -euo pipefail
project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${SHADOWLESS_BUILD_DIR:-"$project_root/.qa/linux-build"}
if [[ -z ${SHADOWLESS_LAYERSHELL_PREFIX:-} && -f "$project_root/.qa/deps/usr/lib/libLayerShellQtInterface.so" ]]; then
    export SHADOWLESS_LAYERSHELL_PREFIX="$project_root/.qa/deps/usr"
fi
if [[ -z ${SHADOWLESS_SKIP_BUILD:-} ]]; then
    mkdir -p -- "$build_dir"
    if command -v meson >/dev/null 2>&1 || [[ ! -x "$build_dir/engine/gpu-screen-recorder" ]]; then
        "$project_root/linux/engine/build.sh"
    else
        echo "run.sh: meson not found; reusing existing GSR engine build." >&2
    fi
    cd -- "$build_dir"
    [[ -f Makefile ]] || qmake6 "$project_root/linux/Shadowless.pro" "SHADOWLESS_PUBLIC_UPDATES=${SHADOWLESS_PUBLIC_UPDATES:-false}"
    make qmake_all
    make -j"${SHADOWLESS_BUILD_JOBS:-4}" -C app
    make -j"${SHADOWLESS_BUILD_JOBS:-4}" -C plugins 2>/dev/null || true
fi
export QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-wayland}
exec "$build_dir/app/shadowless" "$@"
