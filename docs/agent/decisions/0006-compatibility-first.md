# 0006: Compatibility with target systems is a permanent design constraint

Status: accepted

Decision: Shadowless prioritizes user comfort and compatibility with its priority
systems (Windows 10, Windows 11, Arch Linux, CachyOS). Rules are in
`docs/PRODUCT_PHILOSOPHY.md`; requirements, evidence levels and the distribution
expansion plan are in `docs/COMPATIBILITY.md`. Neither is copied elsewhere.

Why: Windows 0.3.3 raised the per-application audio minimum to build 20348 and
made capture start throw when saved advanced/separate audio could not run. Windows
10 users who updated lost Instant Replay and Recording. CI ran on build 20348 and
could not see it.

Consequences:

- Optional or advanced features fail soft: core capture keeps working with a
  runtime fallback, a visible non-blocking notice, and an unchanged saved config.
- Features declare behavior on the four systems; tests simulate real OS/build
  differences; compatibility is claimed only with evidence.
- Releases with new capability gates need a check on the oldest supported system.
