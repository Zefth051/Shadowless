import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "check_secrets.py"


class SecretScanTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.git("init", "-q")
        self.git("config", "user.email", "qa@example.invalid")
        self.git("config", "user.name", "QA")

    def git(self, *args):
        return subprocess.run(["git", "-C", str(self.root), *args], check=True, capture_output=True)

    def scan(self, *args):
        return subprocess.run(["python3", str(SCRIPT), "--root", str(self.root), *args], capture_output=True, text=True)

    def test_clean_repo_and_safe_template(self):
        (self.root / ".env.example").write_text("TOKEN=replace-me\n")
        self.assertEqual(self.scan().returncode, 0)

    def test_untracked_secret_is_reported_without_value(self):
        token = "gh" + "p_" + "A1b2C3d4" * 5
        (self.root / "settings.txt").write_text(token)
        result = self.scan()
        self.assertEqual(result.returncode, 1)
        self.assertIn("settings.txt", result.stdout)
        self.assertNotIn(token, result.stdout + result.stderr)

    def test_staged_secret_is_detected_even_after_worktree_cleanup(self):
        (self.root / "config.txt").write_text("AK" + "IA" + "Z" * 16)
        self.git("add", "config.txt")
        (self.root / "config.txt").write_text("clean")
        self.assertEqual(self.scan().returncode, 1)

    def test_deleted_history_secret_is_detected(self):
        (self.root / "removed.txt").write_text("-----BEGIN " + "PRIVATE KEY-----\n")
        self.git("add", ".")
        self.git("commit", "-qm", "fixture")
        self.git("rm", "removed.txt")
        self.git("commit", "-qm", "remove")
        self.assertEqual(self.scan().returncode, 0)
        result = self.scan("--history")
        self.assertEqual(result.returncode, 1)
        self.assertIn("history", result.stdout)
        self.assertIn("removed.txt", result.stdout)

    def test_sensitive_filename_and_ignored_local_credentials(self):
        (self.root / ".env").write_text("local")
        self.assertEqual(self.scan().returncode, 1)
        (self.root / ".gitignore").write_text(".env\n")
        self.assertEqual(self.scan().returncode, 0)
        self.git("add", "-f", ".env")
        self.assertEqual(self.scan().returncode, 1)

    def test_credentials_in_url_and_long_secret_assignments(self):
        for content in ["https://tester:" + "password123@example.invalid", 'api_key="' + "aB91Cx37" * 4 + '"']:
            (self.root / "config.txt").write_text(content)
            self.assertEqual(self.scan().returncode, 1)

    def test_historical_sensitive_name_is_detected_after_unchanged_rename(self):
        (self.root / ".env").write_text("local settings")
        self.git("add", ".env")
        self.git("commit", "-qm", "fixture")
        self.git("mv", ".env", "safe.txt")
        self.git("commit", "-qm", "rename")
        result = self.scan("--history")
        self.assertEqual(result.returncode, 1)
        self.assertIn("sensitive-filename", result.stdout)

    def test_credentials_in_filename_are_redacted(self):
        token = "gh" + "p_" + "A1b2C3d4" * 5
        (self.root / (".env." + token)).write_text("settings")
        result = self.scan()
        self.assertEqual(result.returncode, 1)
        self.assertNotIn(token, result.stdout + result.stderr)
        self.assertIn("REDACTED", result.stdout)


if __name__ == "__main__":
    unittest.main()
