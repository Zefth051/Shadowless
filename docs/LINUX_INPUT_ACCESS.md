# Input access setup (Linux) — design

Goal: any Linux user can enable the Peripheral overlay from the Shadowless UI, with a system
authorization dialog, never a terminal, `sudo`, the `input` group or hand-written udev rules.
Shadowless itself never runs as root; only a tiny helper is elevated, once, on request.

## What the desktop offers (checked, not assumed)

| Mechanism | Global key/mouse **state**? | Verdict |
| --- | --- | --- |
| Wayland protocols (core, `zwp_keyboard_shortcuts_inhibit`, layer-shell) | No: clients only receive input for their own surfaces | Unusable |
| xdg-desktop-portal `GlobalShortcuts` (KDE, Hyprland, GNOME) | Only pre-registered shortcuts, and registering a key makes the compositor consume it | Unusable for passive display; would break typing and games |
| Portals `InputCapture` / `RemoteDesktop` + libei | Capture-at-edges and injection, not passive observation | Unusable |
| KWin / Hyprland IPC | Window, workspace and focus events; no key state | Unusable (already used for game detection) |
| X11 (XInput2 raw events / XRecord) | Yes, but only for X11 sessions; XWayland sees nothing | Out of scope (Shadowless targets native Wayland) |
| logind `TakeDevice` | Only for the session controller (the compositor) | Unusable |
| **evdev (`/dev/input/event*`)** | Yes, compositor independent | Required |

So evdev stays. The question is only how to grant read access safely.

## Access options compared

| Option | Security | Works in AppImage | UX | Verdict |
| --- | --- | --- | --- | --- |
| `input` group | Every program you run can read every keyboard, permanently | yes | terminal | Rejected |
| Broad udev rule for all keyboards/mice | Same exposure, no per-device consent | needs root install | terminal | Rejected |
| Hand-written per-device rule (what we did manually) | Minimal | needs root install | terminal | Too hard for users |
| Root daemon that filters events and forwards only allowlisted ids | Best (no program but Shadowless sees keys) | needs a system service + packaging per distro | one-time dialog | Best long-term, too heavy and distro-specific for now (roadmap) |
| **Polkit-elevated one-shot helper that installs a narrow per-device `uaccess` rule** | Minimal and per-device; only the active local session gets read access; revocable | **yes** (helper staged like GSR's KMS helper) | one dialog, no terminal | **Chosen** |

Residual risk, stated in the UI: a granted device can be read by any program running as you
until you remove the access.

## Architecture

* `shadowless-input-setup` (new, `linux/helper/input_setup/`): pure C++17/POSIX, no Qt, no
  network, no config parsing. Subcommands `status`, `install`, `remove`. It receives only
  `VENDOR:PRODUCT:CLASS` triples (4 hex digits each, class keyboard|mouse|gamepad, at most 16),
  **generates the rule file from a fixed template** (callers cannot supply rule text), writes
  `/etc/udev/rules.d/70-shadowless-input.rules` atomically (`O_EXCL` temp + `rename`, 0644),
  then reloads udev and re-triggers only the affected vendors. It refuses anything else.
  Exit codes: 0 ok, 2 usage/validation, 3 I/O, 4 udev command failed (rule installed).
* The app (unprivileged) enumerates devices from sysfs (name, vendor, product, class by key
  capabilities, readable or not via `access(2)`), shows what is already readable, lets the user
  pick devices (keyboards and mice preselected, nothing broader than the picked models), then runs
  `pkexec <helper> install ...`. No device is opened to enumerate. Dismissing the dialog (pkexec
  126/127) is not an error: Shadowless keeps working without input capture.
* If every needed device is already readable the overlay goes LIVE immediately and nothing is asked.
* Persistence: the rule lives in `/etc/udev/rules.d`, so it survives reboots and reconnects, and
  matches by vendor/product, not by `eventN`. **Remove input access** runs `remove`.
* Packaging: AppImage bundles the helper. Because pkexec needs a root-readable path, the launcher
  copies it to a private 0700 directory, as the bundled GSR KMS helper already does. Distro
  packages may install it under `libexec` and ship a polkit action; both paths use the same CLI.
* Distributions: any systemd/udev + polkit system (Arch/CachyOS, Fedora, Ubuntu, openSUSE).
  Without `pkexec` the panel falls back to showing the exact rule and commands (manual path).

## Security properties (tested)

No shell, no `system()`, absolute paths for `udevadm`, cleared environment, strict input
validation, template-only output, refuses symlinked or unexpected targets, `--root` (tests only)
is rejected when running under pkexec, no passwords or secrets handled by Shadowless.
