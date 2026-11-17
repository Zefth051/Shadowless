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
   everything else keeps working and the UI says so calmly.
10. **No repeated setup.** Never ask again for a permission or setting that was
    granted and is still valid. Detect it and proceed silently.

## Checklist before adding or reviewing a feature

1. Does it really help the user?
2. Can it work automatically?
3. Are we adding unnecessary steps?
4. Can the experience be simpler?
5. Does it require technical knowledge?
6. Does it keep Shadowless light, comfortable and easy to understand?

If an answer is bad, change the design; do not document the friction away.

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
