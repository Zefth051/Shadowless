#!/usr/bin/env python3
"""Build a private-test x86_64 AppImage from an already validated Linux build."""
import argparse
import hashlib
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tempfile
import urllib.request

REPO = Path(__file__).resolve().parents[2]
PACKAGING = Path(__file__).resolve().parent
# Continuous upstream tags can move; fail closed if their bytes change.
TOOLS = {
    "linuxdeploy": ("linuxdeploy/linuxdeploy", "continuous", "linuxdeploy-x86_64.AppImage", "df908b19f1ed1cdafa79508d415aa12d6c56ce4d56d4149aa8c09286718706e5"),
    "appimagetool": ("AppImage/appimagetool", "1.9.1", "appimagetool-x86_64.AppImage", "ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0"),
    "runtime": ("AppImage/type2-runtime", "continuous", "runtime-x86_64", "617aa77314d57c8b8e4fae9c3f0b35af7b30d5529cb69e98f64f92510ed98a46"),
}


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def acquire_tools(cache):
    cache.mkdir(parents=True, exist_ok=True)
    result = {}
    for name, (repository, tag, asset, digest) in TOOLS.items():
        path = cache / (digest[:16] + "-" + asset)
        if not path.is_file() or sha256(path) != digest:
            url = f"https://github.com/{repository}/releases/download/{tag}/{asset}"
            with tempfile.NamedTemporaryFile(dir=cache, delete=False) as target:
                temporary = Path(target.name)
                try:
                    with urllib.request.urlopen(url, timeout=60) as response:
                        shutil.copyfileobj(response, target)
                    target.close()
                    if sha256(temporary) != digest:
                        raise RuntimeError(f"SHA-256 verification failed for {name}")
                    temporary.replace(path)
                finally:
                    temporary.unlink(missing_ok=True)
        path.chmod(0o755)
        if name == "runtime":
            result[name] = path
            continue
        extracted = cache / (digest[:16] + "-extracted")
        if not (extracted / "squashfs-root/AppRun").exists():
            extracted.mkdir(exist_ok=True)
            subprocess.run([str(path), "--appimage-extract"], cwd=extracted, check=True, stdout=subprocess.DEVNULL)
        result[name] = extracted / "squashfs-root/AppRun"
    return result


def stage_appdir(build, appdir):
    required = [build / "app/shadowless", build / "plugins/libshadowless-overlay.so"]
    for path in required:
        if not path.is_file():
            raise FileNotFoundError(f"Build the Linux application and plugin first: {path.name}")
    appdir.mkdir()  # Never merge or clean a caller's existing directory.
    for directory in ["usr/bin", "usr/plugins", "usr/share/doc/shadowless", "usr/libexec/shadowless-host"]:
        (appdir / directory).mkdir(parents=True)
    for source, target in zip(required, ["usr/bin/shadowless", "usr/plugins/libshadowless-overlay.so"]):
        shutil.copy2(source, appdir / target)
    shutil.copy2(PACKAGING / "AppRun", appdir / "AppRun")
    (appdir / "AppRun").chmod(0o755)
    desktop = "io.github.zefth051.Shadowless"
    shutil.copy2(PACKAGING / (desktop + ".desktop"), appdir / (desktop + ".desktop"))
    shutil.copy2(REPO / "linux/app/assets/shadowless-moon.png", appdir / (desktop + ".png"))
    for name in ["LICENSE", "THIRD_PARTY_NOTICES.md", "README.md"]:
        shutil.copy2(REPO / name, appdir / "usr/share/doc/shadowless" / name)
    shutil.copy2(PACKAGING / "README.md", appdir / "usr/share/doc/shadowless/APPIMAGE.md")
    for name in ["AI_DEVELOPMENT.md", "FEATURES.md", "DEPENDENCY_AUDIT.md"]:
        shutil.copy2(REPO / "docs" / name, appdir / "usr/share/doc/shadowless" / name)
    shutil.copytree(REPO / "licenses", appdir / "usr/share/doc/shadowless/licenses")
    for tool in ["gpu-screen-recorder", "ffmpeg", "ffprobe", "hyprctl", "pactl", "pw-loopback"]:
        target = appdir / "usr/libexec/shadowless-host" / tool
        shutil.copy2(PACKAGING / "host-tool", target)
        target.chmod(0o755)


def stage_qt_plugins(appdir):
    plugins = Path(subprocess.check_output(["qmake6", "-query", "QT_INSTALL_PLUGINS"], text=True).strip())
    selected = []
    for pattern in ["platforms/libqwayland*.so", "platforms/libqoffscreen.so", "wayland-shell-integration/lib*.so",
                    "wayland-graphics-integration-client/lib*.so", "imageformats/libqjpeg.so", "imageformats/libqgif.so",
                    "imageformats/libqwebp.so", "imageformats/libqsvg.so", "iconengines/libqsvgicon.so", "multimedia/lib*.so"]:
        selected.extend(plugins.glob(pattern))
    if not any(path.name.startswith("libqwayland") for path in selected) or not (plugins / "imageformats/libqsvg.so").is_file():
        raise FileNotFoundError("Install the Qt Wayland and SVG plugins before packaging.")
    for path in selected:
        target = appdir / "usr/plugins" / path.relative_to(plugins)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
    prefix = Path(os.environ.get("SHADOWLESS_LAYERSHELL_PREFIX", REPO / ".qa/deps/usr"))
    layer_shell = prefix / "lib/qt6/plugins/wayland-shell-integration/liblayer-shell.so"
    if not layer_shell.is_file():
        layer_shell = plugins / "wayland-shell-integration/liblayer-shell.so"
    if not layer_shell.is_file():
        raise FileNotFoundError("LayerShellQt's Wayland shell plugin is required.")
    shutil.copy2(layer_shell, appdir / "usr/plugins/wayland-shell-integration/liblayer-shell.so")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True)
    parser.add_argument("--build-dir", type=Path, default=REPO / ".qa/linux-build")
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    if not re.fullmatch(r"\d+\.\d+\.\d+", args.version):
        parser.error("version must be X.Y.Z")
    if platform.machine() != "x86_64":
        parser.error("only native x86_64 builds are supported")
    output = (args.output_dir or REPO / "artifacts/release" / ("linux-" + args.version)).resolve()
    output.mkdir(parents=True, exist_ok=True)
    filename = f"Shadowless-{args.version}-linux-x86_64.AppImage"
    destination = output / filename
    if destination.exists() or (output / "SHA256SUMS.txt").exists():
        parser.error("release output already exists; use a new output directory")
    with tempfile.TemporaryDirectory(prefix="appimage-", dir=output) as temporary:
        work = Path(temporary)
        actual_version = subprocess.check_output(
            [str(args.build_dir.resolve() / "app/shadowless"), "--version"],
            env=dict(os.environ, QT_QPA_PLATFORM="offscreen"), text=True).strip()
        if actual_version != "shadowless " + args.version:
            parser.error("application version does not match package version; rebuild with SHADOWLESS_VERSION=" + args.version)
        appdir = work / "Shadowless.AppDir"
        stage_appdir(args.build_dir.resolve(), appdir)
        stage_qt_plugins(appdir)
        tools = acquire_tools(REPO / ".qa/appimage-tools")
        env = dict(os.environ, ARCH="x86_64")
        subprocess.run([str(tools["linuxdeploy"]), "--appdir", str(appdir), "--deploy-deps-only", str(appdir / "usr"),
                        "--custom-apprun", str(PACKAGING / "AppRun")], env=env, check=True)
        # linuxdeploy may generate its own AppRun; preserve our dependency/host boundary.
        (appdir / "AppRun").unlink(missing_ok=True)
        shutil.copy2(PACKAGING / "AppRun", appdir / "AppRun")
        (appdir / "AppRun").chmod(0o755)
        docs = appdir / "usr/share/doc/shadowless"
        inventory = ["Private test artifact. Public distribution requires the source/license review in docs/DEPENDENCY_AUDIT.md.",
                     "glibc build baseline: " + " ".join(platform.libc_ver())]
        for name, path in sorted(tools.items()):
            inventory.append(f"Packaging tool {name}: {TOOLS[name][3]}")
        for path in sorted(appdir.rglob("*")):
            if path.is_file() and not path.is_symlink():
                inventory.append(sha256(path) + "  " + path.relative_to(appdir).as_posix())
        (docs / "BUNDLE_MANIFEST.txt").write_text("\n".join(inventory) + "\n")
        subprocess.run([str(tools["appimagetool"]), "--runtime-file", str(tools["runtime"]), str(appdir), str(work / filename)], env=env, check=True)
        subprocess.run([str(work / filename), "--appimage-extract"], cwd=work, check=True, stdout=subprocess.DEVNULL)
        extracted = work / "squashfs-root"
        subprocess.run([str(extracted / "AppRun"), "--help"], env=dict(os.environ, QT_QPA_PLATFORM="offscreen"), check=True, stdout=subprocess.DEVNULL)
        shutil.copy2(work / filename, destination)
    (output / "SHA256SUMS.txt").write_text(sha256(destination) + "  " + filename + "\n")
    print(destination)


if __name__ == "__main__":
    main()
