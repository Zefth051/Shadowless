#!/usr/bin/env python3
"""Verify that every native DLL in a Shadowless runtime can resolve its imports.

Resolution order mirrors a clean Windows install: the DLL's own directory
(the bundle), then DLLs that ship with Windows itself. The Visual C++
runtime (VCRUNTIME140*, MSVCP140*, ...) does NOT ship with Windows, so it
must be bundled app-local or installed by Setup. A clean PC without the
Visual C++ Redistributable fails to load obs.dll otherwise.

Usage: check_runtime_dependencies.py <runtime-dir> [<runtime-dir> ...]
Requires: pip install pefile
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

# Not part of Windows; must be present in the bundle.
REDISTRIBUTABLE = re.compile(
    r"^(vcruntime140(_1|_threads)?|msvcp140(_\d|_atomic_wait|_codecvt_ids)?|"
    r"concrt140|vcomp140|vccorlib140)\.dll$",
    re.IGNORECASE,
)

# DLLs that ship with every supported Windows 10/11 x64 install.
WINDOWS_SYSTEM = {
    "kernel32.dll", "kernelbase.dll", "user32.dll", "gdi32.dll", "advapi32.dll",
    "shell32.dll", "ole32.dll", "oleaut32.dll", "rpcrt4.dll", "winmm.dll",
    "ws2_32.dll", "wsock32.dll", "dxgi.dll", "d3d11.dll", "d3d9.dll",
    "d3dcompiler_47.dll", "dwmapi.dll", "avrt.dll", "ntdll.dll", "ucrtbase.dll",
    "msvcrt.dll", "bcrypt.dll", "crypt32.dll", "secur32.dll", "iphlpapi.dll",
    "shlwapi.dll", "comctl32.dll", "comdlg32.dll", "setupapi.dll", "version.dll",
    "psapi.dll", "mfplat.dll", "mf.dll", "mfreadwrite.dll", "propsys.dll",
    "uxtheme.dll", "imm32.dll", "winhttp.dll", "wininet.dll", "dnsapi.dll",
    "ncrypt.dll", "cfgmgr32.dll", "powrprof.dll", "dbghelp.dll", "mmdevapi.dll",
    "dcomp.dll", "d3d12.dll", "dxcore.dll", "dwrite.dll", "d2d1.dll",
    "windowscodecs.dll", "combase.dll", "userenv.dll", "wtsapi32.dll",
    "netapi32.dll", "mpr.dll", "shcore.dll", "gdiplus.dll", "msimg32.dll",
    "avicap32.dll", "bcryptprimitives.dll", "coremessaging.dll", "normaliz.dll",
    "opengl32.dll", "usp10.dll", "wldap32.dll",
    # Loader installed with the GPU driver; optional for the libmpv player only.
    "vulkan-1.dll",
}


def classify(import_name: str, bundled: set[str]) -> str:
    """Return 'bundled', 'system', 'redistributable-missing' or 'missing'."""
    name = import_name.lower()
    if name in bundled:
        return "bundled"
    if name.startswith(("api-ms-win-", "ext-ms-win-")):
        # Universal CRT api sets are part of Windows 10+.
        return "system"
    if name in WINDOWS_SYSTEM:
        return "system"
    if REDISTRIBUTABLE.match(name):
        return "redistributable-missing"
    return "missing"


def analyze(root: Path) -> dict[str, dict[str, set[str]]]:
    import pefile  # imported lazily so classify() stays importable without it

    root = Path(root)
    binaries = [p for p in root.rglob("*") if p.suffix.lower() in {".dll", ".exe"}]
    bundled = {p.name.lower() for p in binaries}
    problems: dict[str, dict[str, set[str]]] = {
        "redistributable-missing": {},
        "missing": {},
    }
    for binary in binaries:
        try:
            pe = pefile.PE(str(binary), fast_load=True)
            pe.parse_data_directories(
                directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]]
            )
        except pefile.PEFormatError:
            continue
        with pe:
            for entry in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
                dll = entry.dll.decode("ascii", "replace")
                kind = classify(dll, bundled)
                if kind in problems:
                    problems[kind].setdefault(dll.lower(), set()).add(
                        str(binary.relative_to(root))
                    )
    return problems


def main(argv: list[str]) -> int:
    if not argv:
        print(__doc__)
        return 2
    exit_code = 0
    for directory in argv:
        problems = analyze(Path(directory))
        for kind, found in problems.items():
            for dll, importers in sorted(found.items()):
                exit_code = 1
                print(f"[{kind}] {dll} <- {len(importers)} importer(s), e.g. "
                      f"{sorted(importers)[0]}")
        if exit_code == 0:
            print(f"OK: {directory}")
    return exit_code


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
