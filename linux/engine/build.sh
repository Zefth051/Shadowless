#!/usr/bin/env bash
set -euo pipefail
project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
build_dir=${SHADOWLESS_BUILD_DIR:-"$project_root/.qa/linux-build"}
engine="$build_dir/engine"
source_dir="$project_root/linux/third_party/gpu-screen-recorder"
(cd "$source_dir" && sha256sum --check --quiet ../GSR_SOURCES.sha256)
mkdir -p "$engine/helper-libs"
meson setup "$build_dir/gsr-build" "$source_dir" --buildtype=release \
    -Dcapabilities=false -Dsystemd=false -Dnvidia_suspend_fix=false
meson compile -C "$build_dir/gsr-build" -j "${SHADOWLESS_BUILD_JOBS:-4}" gpu-screen-recorder gsr-kms-server
cp "$build_dir/gsr-build/gpu-screen-recorder" "$engine/gpu-screen-recorder.bin"
cp "$build_dir/gsr-build/gsr-kms-server" "$engine/gsr-kms-server"
cp "$project_root/linux/engine/launch-recorder" "$engine/gpu-screen-recorder"
chmod 755 "$engine/gpu-screen-recorder"
# Copy the resolved non-glibc closure; libdrm can have extra distro dependencies.
python3 - "$engine" <<'PY'
from pathlib import Path
import re
import shutil
import subprocess
import sys
engine = Path(sys.argv[1])
libraries = engine / 'helper-libs'
for path in libraries.iterdir():
    path.unlink()
output = subprocess.check_output(['ldd', str(engine / 'gsr-kms-server')], text=True)
if 'not found' in output:
    raise SystemExit('Managed KMS helper has unresolved dependencies: ' + output)
baseline = {'libc.so.6', 'libm.so.6', 'libdl.so.2', 'libpthread.so.0', 'librt.so.1'}
for name, path in re.findall(r'^[ \t]*([^/\s]+) => (/\S+)', output, re.M):
    if name not in baseline:
        target = libraries / name
        shutil.copy2(path, target)
        subprocess.run(['patchelf', '--set-rpath', '$ORIGIN', str(target)], check=True)
if not (libraries / 'libdrm.so.2').is_file():
    raise SystemExit('Managed KMS helper did not resolve libdrm.so.2')
PY
patchelf --set-rpath '$ORIGIN/lib' "$engine/gsr-kms-server"
cp "$project_root/linux/third_party/GSR_PROVENANCE.md" "$engine/GSR_PROVENANCE.md"
cp "$project_root/linux/third_party/GSR_SOURCES.sha256" "$engine/GSR_SOURCES.sha256"
