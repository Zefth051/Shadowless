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
  `/etc/udev/rules.d/71-shadowless-input-access.rules` (the manual-setup file `70-shadowless-input.rules` from the docs is never touched) atomically (`O_EXCL` temp + `rename`, 0644),
  then reloads udev and re-triggers input devices (`change` events). Rules match the kernel ids (`ATTRS{id/vendor}`/`ATTRS{id/product}`) plus the device class flag, so USB, Bluetooth and built-in keyboards all work and a different model never matches. It refuses anything else.
  Exit codes: 0 ok, 2 usage/validation, 3 I/O, 4 udev command failed (rule installed).
* The app (unprivileged) enumerates devices from sysfs (name, vendor, product, class by key
  capabilities, readable or not via `access(2)`), shows what is already readable, lets the user
  pick devices (keyboards and mice preselected, nothing broader than the picked models), then runs
  `pkexec <helper> install ...`. No device is opened to enumerate. Dismissing the dialog (pkexec
  126/127) is not an error: Shadowless keeps working without input capture.
* If every needed device is already readable the overlay goes LIVE immediately and nothing is asked.
* Persistence: the rule lives in `/etc/udev/rules.d`, so it survives reboots and reconnects, and
  matches by vendor/product, not by `eventN`. **Remove input access** runs `remove`.
* Packaging and the AppImage problem: pkexec cannot read a FUSE-mounted AppImage, and running a user-owned copy as
  root is unsafe (any process of the same user can swap it between authorization and execution; a 0700 directory
  does not prevent that). So:
  * A helper that root fully controls (system package under `/usr/libexec/shadowless/`, plus the polkit policy in
    `packaging/polkit/`) is run in place by pkexec.
  * Otherwise pkexec runs a fixed root-owned `/usr/bin/sh` script. The helper bytes are streamed on stdin, written by
    root into a root-only directory under `/run`, and must match the SHA-256 given in argv (argv of a root process
    cannot be changed by the user). Nothing user-controlled is executed as root, and a tampered stream fails.
  * Cost: the system dialog names `sh -c …` instead of the helper (distro packages avoid this with the policy file).
* Devices are grouped per physical device (vendor:product) and classified from verified capabilities (autorepeat for
  keyboards, REL_X/REL_Y for mice, gamepad buttons) and the device name, so a keyboard's vendor mouse interface or a
  mouse's keyboard-style interface is not offered. Only a true tie shows a one-click choice.
* **Remove** deletes the rule, reloads udev, waits for it to settle and removes the user's ACL from nodes of the
  removed devices that no other rule still tags `uaccess`, so access ends immediately (without it, logind keeps ACLs
  until reconnect). Reconnected devices get a new node and, with no rule, no ACL.
* Distributions: any systemd/udev + polkit system (Arch/CachyOS, Fedora, Ubuntu, openSUSE).
  Without `pkexec` the panel falls back to showing the exact rule and commands (manual path).

## Security properties (tested)

No shell, no `system()`, absolute paths for `udevadm`, cleared environment, strict input
validation, template-only output, refuses symlinked or unexpected targets, `--root` (tests only)
is rejected when running under pkexec, no passwords or secrets handled by Shadowless.

## Security review (Linux 0.3.8 development)

Scope: `linux/helper/input_setup/`, the pkexec delivery in `InputAccess::run`, and the polkit policy. Reviewed by reading the
code and extending the tests; no external audit. Threat model: a local attacker who runs code as the same unprivileged user
(including a swapped AppImage) tries to get root, or to make the user grant more than they intended.

| # | Finding | Severity | Disposition |
| --- | --- | --- | --- |
| 1 | Remove skipped the ACL revocation when the udev reload failed, so the user's ACLs stayed on already-connected devices although the rule was gone and the UI said access was removed. | Low | **Fixed.** The helper now revokes ACLs first and reports the reload failure (exit 4) afterwards. Test: `removeStillRevokesAclsWhenTheUdevReloadFails`. |
| 2 | pkexec exit 127 was shown as "cancelled", hiding real failures (not authorized, polkit missing, helper blocked). | Low (honesty) | **Fixed** in `InputAccess`: each class has its own result and plain message; technical detail only in `diagnostics()` and the log. |
| 3 | A noexec `/run` makes the staged helper fail (sh exit 126), which looked like a dismissed dialog. | Low | **Fixed** by the same classification (`HelperBlocked`, manual steps offered). |
| 4 | AppImage path: the polkit prompt authorizes the generic `/usr/bin/sh -c <script>`, and the helper hash arrives in argv from the same user. The hash protects against swapping the bytes **after** the user starts the flow, not against a malicious initiator. Any same-user process can already run `pkexec sh -c …` and ask for the password; this path adds no capability, but the dialog is less informative than a packaged helper's. | Residual, design-level | **Accepted and documented.** An AppImage is user-writable, so nothing inside it can be trusted against the same user. Distribution packages should ship the root-owned helper and the polkit policy (clear prompt, exec path annotation); the AppImage keeps the staged path as the fallback. |
| 5 | `--root` and `--no-reload` are test options. `--root` is rejected when `PKEXEC_UID` is set (always the case under pkexec), so a user cannot redirect root writes. | None | Verified; covered by existing tests. |
| 6 | Rule text is generated from a fixed template from validated `vvvv:pppp:class` triples; existing files are re-validated line by line before replace; symlinks, hard links, `..` and non-regular files are refused; writes use `O_EXCL|O_NOFOLLOW` and `renameat` in a pinned directory; the environment of child processes is empty and programs are absolute paths. | None | Verified by reading and by `unsafeTargets`/`unexpectedContents`. |
| 7 | Staging directory is created by root with `mktemp -d` under `/run` and removed by `trap`; a SIGKILL could leave a root-only directory until reboot. | Negligible | Accepted. |

Residual risk stated to users: while a device is granted, any program running as the user can read it. Removing access ends
it immediately. A hostile device that spoofs a granted vendor:product only receives the same user-level access.

## What the user sees

1. **Capture overlays → Peripheral overlay**, pick an overlay. If the devices are already readable the status goes
   LIVE immediately; nothing is asked.
2. If not: status PERMISSION DENIED and a button **Enable input access…**. It shows an inline card (not a separate window, so it cannot open behind the overlay editor) with one row
   per detected device, preselected, explains what changes, and has **Not now**.
3. **Continue** opens the system authorization dialog (pkexec). Cancelling it changes nothing and Shadowless keeps working.
4. On success the overlay goes LIVE without restarting. **Remove input access** undoes it.
5. Some desktops (notably Hyprland without `hyprpolkitagent`) run no polkit authentication agent, so the system cannot show its dialog.
   Shadowless detects that (`pkexec` exit 127 + "No authentication agent") and says so, shows the manual steps, and names the agents to start.
6. No polkit/pkexec or no helper (unusual builds): the panel shows the manual steps from `LINUX_INPUT_OVERLAY.md`.

Known limits: `/run` must allow executing files (default on mainstream distributions) for the AppImage path.
Wayland sessions on non-systemd distributions without logind will not apply `uaccess` ACLs; the panel then keeps
reporting PERMISSION DENIED.
