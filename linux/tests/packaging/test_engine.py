import os
from pathlib import Path
import shutil
import subprocess
import signal
import sys
import time
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


class ManagedEngineTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.engine = Path(self.temp.name) / "engine"
        self.engine.mkdir()
        (self.engine / "helper-libs").mkdir()
        (self.engine / "helper-libs/libdrm.so.2").write_text("private drm")
        for name, body in {
            "gpu-screen-recorder.bin": '#!/bin/sh\nprintf "%s\\n" "$SHADOWLESS_GSR_KMS_SERVER"\ncat "${SHADOWLESS_GSR_KMS_SERVER%/*}/lib/libdrm.so.2"\n',
            "gsr-kms-server": "#!/bin/sh\nexit 0\n",
        }.items():
            target = self.engine / name
            target.write_text(body)
            target.chmod(0o755)

    def launch(self):
        shutil.copy2(ROOT / "linux/engine/launch-recorder", self.engine / "gpu-screen-recorder")
        return subprocess.run(["/bin/sh", str(self.engine / "gpu-screen-recorder")],
                              env=dict(os.environ, SHADOWLESS_GSR_KMS_SERVER="/wrong/host-helper"),
                              capture_output=True, text=True)

    def test_matching_helper_is_staged_outside_bundle_and_removed_after_exit(self):
        result = self.launch()
        self.assertEqual(result.returncode, 0, result.stderr)
        helper, libraries = result.stdout.splitlines()
        self.assertTrue(helper.startswith("/tmp/shadowless-kms-"), helper)
        self.assertEqual(libraries, "private drm")
        self.assertFalse(Path(helper).exists())

    def test_missing_managed_engine_never_falls_back_to_host(self):
        (self.engine / "gpu-screen-recorder.bin").unlink()
        result = self.launch()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("managed recording engine", result.stderr)

    def test_missing_matching_helper_fails_before_recorder_launch(self):
        (self.engine / "gsr-kms-server").unlink()
        result = self.launch()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "")

    def test_termination_forwards_interrupt_and_waits_for_recorder_cleanup(self):
        target = self.engine / "gpu-screen-recorder.bin"
        target.write_text('#!' + sys.executable + '\nimport os, signal, time\n'
                          'def finish(*args):\n print("flushed", flush=True)\n raise SystemExit(0)\n'
                          'signal.signal(signal.SIGINT, finish)\n'
                          'print(os.environ["SHADOWLESS_GSR_KMS_SERVER"], flush=True)\n'
                          'while True: time.sleep(1)\n')
        shutil.copy2(ROOT / "linux/engine/launch-recorder", self.engine / "gpu-screen-recorder")
        process = subprocess.Popen(["/bin/sh", str(self.engine / "gpu-screen-recorder")],
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            helper = process.stdout.readline().strip()
            process.send_signal(signal.SIGTERM)
            output, errors = process.communicate(timeout=5)
            self.assertEqual(process.returncode, 0, errors)
            self.assertEqual(output.strip(), "flushed")
            self.assertFalse(Path(helper).exists())
        finally:
            if process.poll() is None:
                process.kill()
                process.communicate()

    def test_hard_killing_launcher_cannot_leave_managed_recorder_running(self):
        source = self.engine / "guard.c"
        source.write_text('#include "shadowless_parent.h"\n#include <stdio.h>\n#include <stdlib.h>\n'
                          'int main(void) { shadowless_bind_parent(atoi(getenv("SHADOWLESS_GSR_PARENT_PID"))); signal(SIGINT, SIG_IGN); '
                          'printf("%d\\n", getpid()); fflush(stdout); for (;;) pause(); }\n')
        subprocess.run(["cc", "-I", str(ROOT / "linux/third_party/gpu-screen-recorder/include"),
                        str(source), "-o", str(self.engine / "gpu-screen-recorder.bin")], check=True)
        shutil.copy2(ROOT / "linux/engine/launch-recorder", self.engine / "gpu-screen-recorder")
        process = subprocess.Popen(["/bin/sh", str(self.engine / "gpu-screen-recorder")],
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        recorder = None
        try:
            recorder = int(process.stdout.readline())
            process.kill()
            process.wait(timeout=5)
            deadline = time.monotonic() + 3
            while time.monotonic() < deadline:
                status = Path(f"/proc/{recorder}/stat")
                if not status.exists() or status.read_text().split()[2] == "Z":
                    break
                time.sleep(0.02)
            else:
                self.fail("Managed recorder survived its launcher being killed")
        finally:
            if process.poll() is None:
                process.kill()
            if recorder is not None:
                try:
                    os.kill(recorder, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            process.communicate(timeout=5)


if __name__ == "__main__":
    unittest.main()
