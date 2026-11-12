import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

PACKAGING = Path(__file__).resolve().parents[2] / "packaging"
SPEC = importlib.util.spec_from_file_location("build_appimage", PACKAGING / "build_appimage.py")


class AppImageTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.build = self.root / "build"
        (self.build / "app").mkdir(parents=True)
        (self.build / "plugins").mkdir()
        (self.build / "app/shadowless").write_text("app")
        (self.build / "plugins/libshadowless-overlay.so").write_text("plugin")
        (self.build / "engine/helper-libs").mkdir(parents=True)
        for name in ["gpu-screen-recorder", "gpu-screen-recorder.bin", "gsr-kms-server", "GSR_PROVENANCE.md", "helper-libs/libdrm.so.2"]:
            (self.build / "engine" / name).write_text("engine")
            (self.build / "engine" / name).chmod(0o755)
        self.module = importlib.util.module_from_spec(SPEC)
        SPEC.loader.exec_module(self.module)

    def test_staging_preserves_plugin_path_and_licenses(self):
        appdir = self.root / "AppDir"
        self.module.stage_appdir(self.build, appdir)
        self.assertEqual((appdir / "usr/bin/shadowless").read_text(), "app")
        self.assertEqual((appdir / "usr/plugins/libshadowless-overlay.so").read_text(), "plugin")
        self.assertTrue((appdir / "usr/share/doc/shadowless/LICENSE").is_file())
        self.assertTrue((appdir / "io.github.zefth051.Shadowless.desktop").is_file())
        self.assertTrue((appdir / "io.github.zefth051.Shadowless.png").is_file())
        self.assertTrue((appdir / "usr/engine/gpu-screen-recorder.bin").is_file())
        self.assertTrue((appdir / "usr/share/doc/shadowless/gsr-source/LICENSE").is_file())

    def test_missing_managed_recorder_does_not_create_partial_appdir(self):
        (self.build / "engine/gpu-screen-recorder.bin").unlink()
        with self.assertRaises(FileNotFoundError):
            self.module.stage_appdir(self.build, self.root / "AppDir")
        self.assertFalse((self.root / "AppDir").exists())

    def test_missing_plugin_does_not_create_partial_appdir(self):
        (self.build / "plugins/libshadowless-overlay.so").unlink()
        with self.assertRaises(FileNotFoundError):
            self.module.stage_appdir(self.build, self.root / "AppDir")
        self.assertFalse((self.root / "AppDir").exists())

    def test_staging_never_merges_into_existing_directory(self):
        appdir = self.root / "existing"
        appdir.mkdir()
        (appdir / "keep").write_text("keep")
        with self.assertRaises(FileExistsError):
            self.module.stage_appdir(self.build, appdir)
        self.assertEqual((appdir / "keep").read_text(), "keep")

    def test_invalid_version_is_rejected_before_any_download(self):
        result = subprocess.run(["python3", str(PACKAGING / "build_appimage.py"), "--version", "../../escape"], capture_output=True)
        self.assertEqual(result.returncode, 2)

    def test_dependency_check_reports_missing_tools_without_starting_capture(self):
        appdir = self.root / "AppDir"
        self.module.stage_appdir(self.build, appdir)
        result = subprocess.run([str(appdir / "AppRun"), "--check-dependencies"], env={"PATH": str(self.root)}, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("ffmpeg", result.stderr)
        self.assertNotIn("host tool: gpu-screen-recorder", result.stderr)

    def test_host_ffmpeg_does_not_inherit_bundle_library_or_qt_paths(self):
        appdir = self.root / "AppDir"
        self.module.stage_appdir(self.build, appdir)
        host = self.root / "host"
        host.mkdir()
        recorder = host / "ffmpeg"
        recorder.write_text('#!/bin/sh\nprintf "%s|%s" "$LD_LIBRARY_PATH" "${QT_PLUGIN_PATH-unset}"\n')
        recorder.chmod(0o755)
        app = appdir / "usr/bin/shadowless"
        app.write_text('#!/bin/sh\nexec ffmpeg\n')
        app.chmod(0o755)
        env = dict(os.environ, PATH=str(host), LD_LIBRARY_PATH=str(host), QT_PLUGIN_PATH="host-qt")
        result = subprocess.run([str(appdir / "AppRun"), "--help"], env=env, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, str(host) + "|unset")

    def test_bundled_recorder_wins_over_conflicting_host_recorder(self):
        appdir = self.root / "AppDir"
        self.module.stage_appdir(self.build, appdir)
        host = self.root / "host"
        host.mkdir()
        for target, text in [(host / "gpu-screen-recorder", "host"),
                             (appdir / "usr/engine/gpu-screen-recorder", "managed")]:
            target.write_text('#!/bin/sh\nprintf "' + text + '"\n')
            target.chmod(0o755)
        app = appdir / "usr/bin/shadowless"
        app.write_text('#!/bin/sh\nexec gpu-screen-recorder\n')
        app.chmod(0o755)
        result = subprocess.run([str(appdir / "AppRun"), "--help"],
                                env=dict(os.environ, PATH=str(host)), capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "managed")

    def test_dependency_check_reports_missing_managed_engine(self):
        appdir = self.root / "AppDir"
        self.module.stage_appdir(self.build, appdir)
        (appdir / "usr/engine/gsr-kms-server").unlink()
        result = subprocess.run([str(appdir / "AppRun"), "--check-dependencies"],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("managed recording engine: gsr-kms-server", result.stderr)


if __name__ == "__main__":
    unittest.main()
