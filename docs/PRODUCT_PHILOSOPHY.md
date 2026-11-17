# Product philosophy

Single source of truth for how Shadowless is designed. Agents (Claude Code,
Codex) and contributors read this before adding or reviewing any feature,
installer, setting, permission flow or UI. It applies to Windows and Linux.

> Shadowless is a simple program: install it once and it should just work.
> Shadowless helps the user and keeps them as comfortable as possible.
>
> Shadowless adapts to the user; the user does not adapt to Shadowless.

## Principles

1. **Install once, just works.** One installation, then the features work
   without technical setup.
2. **Zero unnecessary friction.** Avoid terminals, commands, manual
   configuration, extra dependencies and repeated steps wherever possible.
3. **User comfort first.** Intuitive, quick, pleasant.
4. **Sensible defaults.** A good first-run configuration; no need to
   understand internals.
5. **Complexity belongs in the software.** If something is technically hard,
   solve it inside Shadowless instead of handing it to the user.
6. **Progressive disclosure.** Advanced options exist for those who need
   them, hidden from everyone else.
7. **Safe and transparent.** Simplicity never costs security, privacy or
   honesty. When the OS requires authorization, use its native UI and say
   plainly why it is needed, what is granted and how to undo it.
8. **Cross-platform consistency.** Linux and Windows feel the same, within
   each system's technical limits.
9. **Graceful degradation.** A missing capability disables only that feature;
   everything else keeps working and the UI says so calmly. See
   [Compatibility and reliability](#compatibility-and-reliability).
10. **No repeated setup.** Never ask again for a permission or setting that was
    granted and is still valid. Detect it and proceed silently.
11. **Comfort and compatibility with the target systems come first.** Shadowless
    is built for the systems in [COMPATIBILITY.md](COMPATIBILITY.md); a feature
    that helps one system but breaks the core on another is not finished.

## Compatibility and reliability

Permanent rule: Shadowless always prioritizes user comfort and compatibility
with its target operating systems. The current priority systems are Windows 10,
Windows 11, Arch Linux and CachyOS (on Linux: Wayland, Hyprland and KDE Plasma
where relevant). The list is a priority, not a guarantee: no hardware or
configuration is promised to work without evidence. Real minimum requirements,
support tiers and what counts as evidence live in
[COMPATIBILITY.md](COMPATIBILITY.md), the only place they are defined.

1. **Four systems per feature.** Every new feature states how it behaves on
   Windows 10, Windows 11, Arch Linux and CachyOS, or why it is n/a there.
2. **Core is protected.** A new feature must not stop the main functions
   (Instant Replay, Recording) from working on a compatible system.
3. **Safe fallback.** When an advanced capability is unavailable, use a safe
   fallback whenever technically possible instead of failing.
4. **Optional never breaks core.** If an optional feature fails, Instant Replay
   and Recording keep running; the failure is reported calmly.
5. **Shadowless resolves it.** Do not make the user investigate technical
   errors, install dependencies by hand or change their system when it can be
   done safely for them.
6. **No needless administrator rights.** Ask for elevation only when nothing
   narrower works, through the native dialog.
7. **Preferences are never silently rewritten.** A temporary incompatibility is
   handled at runtime (effective settings); the saved choice stays so it works
   again on a capable system. Tell the user when a fallback is active.
8. **Easy install, detection, clear messages, recovery.** Prefer one install,
   automatic detection, a plain message with the next step, and automatic
   recovery over documentation of workarounds.
9. **Tests reflect real differences.** Tests cover real differences between
   Windows versions (for example build 19045 vs 22631) and Linux environments;
   a CI runner that happens to match the newest case is not enough.
10. **No claim without evidence.** Do not declare compatibility, "works on",
    or "fixed" without evidence appropriate to the tier in COMPATIBILITY.md.
    Say what was and was not verified.

Why this exists: Windows 0.3.3 required build 20348 for per-application audio
and aborted capture start on Windows 10 for saved advanced settings. The CI
runner was build 20348, so nothing caught it. See ADR 0006.

## Checklist before adding or reviewing a feature

1. Does it really help the user?
2. Can it work automatically?
3. Are we adding unnecessary steps?
4. Can the experience be simpler?
5. Does it require technical knowledge?
6. Does it keep Shadowless light, comfortable and easy to understand?
7. How does it behave on Windows 10, Windows 11, Arch Linux and CachyOS, and
   what happens when its capability is missing?
8. Can it fail without affecting Instant Replay and Recording, and does it
   avoid changing saved preferences?

If an answer is bad, change the design; do not document the friction away. Reviews
and QA apply the same checklist.

## How it shapes decisions

- **Permissions:** detect existing access first and go live silently; ask only
  when needed, through the native system dialog, with a plain-language reason.
  A declined request never blocks the app. Narrowest grant that works; always
  reversible from the GUI. Reference implementation: `LINUX_INPUT_ACCESS.md`.
- **Install:** one artifact, no manual post-install commands. A manual path may
  exist only as a documented fallback, never as the primary route.
- **Settings:** new options need a sensible default and should rarely be
  required. Prefer automatic detection over a toggle.
- **Errors:** say what happened and the next click, not a log or command.
  Technical detail goes in expandable diagnostics.
- **Dependencies:** bundle or degrade; do not make the user install things.

Safety rules in `AGENTS.md` (security, tests, duo, no publishing without
approval) still apply and are never traded for convenience.
