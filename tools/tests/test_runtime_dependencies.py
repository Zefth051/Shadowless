import os
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_runtime_dependencies as deps  # noqa: E402


class ClassifyTests(unittest.TestCase):
    def test_visual_cpp_runtime_is_not_a_windows_component(self):
        for name in ("VCRUNTIME140.dll", "VCRUNTIME140_1.dll", "MSVCP140.dll"):
            self.assertEqual(deps.classify(name, set()), "redistributable-missing")

    def test_bundled_runtime_satisfies_import(self):
        self.assertEqual(deps.classify("VCRUNTIME140.dll", {"vcruntime140.dll"}), "bundled")

    def test_windows_components_and_api_sets_are_system(self):
        for name in ("KERNEL32.dll", "api-ms-win-crt-runtime-l1-1-0.dll", "D3DCOMPILER_47.dll"):
            self.assertEqual(deps.classify(name, set()), "system")

    def test_unknown_dll_is_missing(self):
        self.assertEqual(deps.classify("somethirdparty.dll", set()), "missing")


@unittest.skipUnless(os.environ.get("SHADOWLESS_RUNTIME_DIR"),
                     "set SHADOWLESS_RUNTIME_DIR to a built runtime directory")
class BundleTests(unittest.TestCase):
    def test_every_import_resolves_on_a_clean_windows_install(self):
        problems = deps.analyze(Path(os.environ["SHADOWLESS_RUNTIME_DIR"]))
        self.assertEqual(problems["redistributable-missing"], {},
                         "Visual C++ runtime DLLs are imported but not bundled")
        self.assertEqual(problems["missing"], {})


if __name__ == "__main__":
    unittest.main()
