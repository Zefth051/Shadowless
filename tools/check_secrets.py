#!/usr/bin/env python3
"""Check publishable files, the index and optionally all reachable Git history.

Reports paths and rule names only. This is a local guard for recognizable secrets,
not a guarantee that arbitrary credentials or personal data cannot be present.
"""
import argparse
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys


RULES = {
    "private-key": rb"-----BEGIN (?:RSA |EC |DSA |OPENSSH |ENCRYPTED )?PRIVATE KEY-----",
    "github-token": rb"\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,})\b",
    "aws-key": rb"\b(?:AKIA|ASIA)[A-Z0-9]{16}\b",
    "slack-token": rb"\bxox[baprs]-[A-Za-z0-9-]{20,}\b",
    "credential-url": rb"https?://[^\s/:'\"]+:[^\s/@'\"]+@",
    "secret-assignment": rb"(?i)\b(?:api[_-]?key|client[_-]?secret|access[_-]?token|password)\s*[=:]\s*['\"]?[A-Za-z0-9_+/=-]{24,}",
}
PATTERNS = {name: re.compile(pattern) for name, pattern in RULES.items()}


def sensitive_name(name):
    base = PurePosixPath(name).name.lower()
    return (
        (base == ".env" or base.startswith(".env."))
        and base not in {".env.example", ".env.sample", ".env.template"}
    ) or base in {"id_rsa", "id_ed25519", "credentials.json", ".netrc", ".npmrc", ".pypirc"} or base.endswith((".pfx", ".p12", ".pem", ".key"))


def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args], stderr=subprocess.PIPE)


def scan(root, history):
    findings = set()
    checked = 0

    def check(name, data, location):
        nonlocal checked
        checked += 1
        if sensitive_name(name):
            findings.add((location, name, "sensitive-filename"))
        for rule, pattern in PATTERNS.items():
            if pattern.search(data):
                findings.add((location, name, rule))

    paths = git(root, "ls-files", "--cached", "--others", "--exclude-standard", "-z").split(b"\0")
    for raw in set(paths) - {b""}:
        name = raw.decode("utf-8", "surrogateescape")
        path = root / name
        if path.is_symlink():
            check(name, path.readlink().as_posix().encode(), "worktree")
        elif path.is_file():
            check(name, path.read_bytes(), "worktree")

    objects = {}
    for entry in git(root, "ls-files", "--stage", "-z").split(b"\0"):
        if not entry:
            continue
        metadata, raw = entry.split(b"\t", 1)
        mode, oid, _stage = metadata.split()
        if mode != b"160000":
            objects[(oid, raw)] = "index"
    if history:
        # rev-list deduplicates blobs, so an unchanged rename can hide old names.
        changes = git(root, "log", "--all", "--root", "-m", "--format=", "--raw", "-z", "--no-renames").split(b"\0")
        for index in range(0, len(changes) - 1, 2):
            name = changes[index + 1].decode("utf-8", "surrogateescape")
            if sensitive_name(name):
                findings.add(("history", name, "sensitive-filename"))
        for line in git(root, "-c", "core.quotePath=false", "rev-list", "--objects", "--all").splitlines():
            oid, _, raw = line.partition(b" ")
            if raw:
                objects.setdefault((oid, raw), "history")

    # Stream Git objects rather than spawning a process for every historical file.
    with subprocess.Popen(["git", "-C", str(root), "cat-file", "--batch"], stdin=subprocess.PIPE, stdout=subprocess.PIPE) as reader:
        try:
            for (oid, raw), location in objects.items():
                reader.stdin.write(oid + b"\n")
                reader.stdin.flush()
                header = reader.stdout.readline().split()
                if len(header) != 3:
                    raise RuntimeError("Could not inspect a Git object.")
                size = int(header[2])
                data = reader.stdout.read(size)
                if len(data) != size or reader.stdout.read(1) != b"\n":
                    raise RuntimeError("Incomplete Git object read.")
                if header[1] == b"blob":
                    check(raw.decode("utf-8", "surrogateescape"), data, location)
        finally:
            reader.stdin.close()
        if reader.wait() != 0:
            raise RuntimeError("Git object inspection failed.")
    return checked, sorted(findings)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--history", action="store_true", help="Also inspect all reachable branches, tags and stashes")
    args = parser.parse_args()
    try:
        root = Path(git(args.root, "rev-parse", "--show-toplevel").decode().strip())
        checked, findings = scan(root, args.history)
    except (OSError, subprocess.CalledProcessError, RuntimeError) as error:
        # Git stderr and exception paths could themselves contain credentials.
        print("Secret scan could not complete (" + type(error).__name__ + ").", file=sys.stderr)
        return 2
    for location, name, rule in findings:
        display = name.encode("utf-8", "surrogateescape")
        for pattern in PATTERNS.values():
            display = pattern.sub(b"[REDACTED]", display)
        print(f"FAIL {location}: {display.decode('utf-8', 'surrogateescape')!r} [{rule}]")
    print(f"Checked {checked} file versions; {len(findings)} finding(s). Secret values are never printed.")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
