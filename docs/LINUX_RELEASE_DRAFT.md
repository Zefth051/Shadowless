# Shadowless Linux 0.3.7 — peripheral overlay preview, bounded input retry, camera catalog

Existing clips and settings are preserved; new features are opt-in. Update from inside the app (**Check for updates**), then quit and reopen.

## What changed and why

- **Peripheral overlay (new, opt-in).** In **Capture overlays**, show pressed keys, mouse buttons or gamepad inputs (QWERTY, QWERTY Full, Arrow Keys, AZERTY, DualSense placeholder). The preview never invents key presses and reports LIVE, IDLE, DISCONNECTED, PERMISSION DENIED or ERROR.
- **Input events next to clips (opt-in).** `<clip>.inputs.json` stores only the keys of the chosen overlay with timing, never text; the clip player can show them with an **Inputs** toggle.
- **Permissions stay yours.** Shadowless never changes permissions or asks for the `input` group; the editor shows a narrow `udev` rule you install yourself (docs/LINUX_INPUT_OVERLAY.md).
- **Input devices that cannot be read are retried a bounded number of times**, not on every device event.
- **Camera catalog (read-only).** V4L2 devices are listed from sysfs without opening them; no camera capture yet.
- **Main window:** notices scroll instead of being cut off, and the duplicate Save button is gone (use the Instant Clip card or the hotkey).
- **Game detection:** executables listed under several titles are no longer mislabelled.
- **Settings stay editable while capture stops.**

## Known limitations

- Peripheral overlays and saved input events were verified with unit tests and simulated devices; real keyboard/mouse hotplug and a physical gamepad are not validated yet. Recordings that were paused have no saved events, and exported clips do not include the overlay.
- Discord presence and automatic game detection depend on the bundled Windows executable catalog; game names cannot be edited in the app yet. Live Plasma validation is still pending.
- Windows has no peripheral overlay yet.

### Download and compatibility

Download `Shadowless-0.3.7-linux-x86_64.AppImage` and `SHA256SUMS.txt`, verify with
`sha256sum -c SHA256SUMS.txt`, then make the AppImage executable. Keep it in a
permanent writable location for in-app updates.

The recorder and matching KMS helper are bundled; no separate GSR installation is
required. Hyprland or Plasma 6 on native Wayland, compatible graphics drivers and
glibc, PipeWire/pipewire-pulse, FFmpeg/ffprobe, pactl and pw-loopback remain host
requirements. Hyprland additionally uses hyprctl. Direct KMS authorization uses the
system Polkit agent; Shadowless never handles your password or runs its UI as root.

The screenshot gallery contains earlier-build UI and synthetic demo clips; it is
not a benchmark or proof of live Plasma capture.

### Sources and attribution

The release provides the exact build inventory and matching dependency source
materials with checksums. Application and modified GSR source are available from
this release tag through GitHub's source archives. See the included component
notices and dependency audit for source/build mappings.

Derived from Captail by FaulMit; GPL-2.0-or-later attribution is retained. The
combined Linux runtime includes components with GPLv3 requirements. Developed with
AI assistance, including OpenAI Codex.

This Linux release keeps the Windows update channel unchanged.

[Full changelog](https://github.com/Zefth051/Shadowless/compare/linux-v0.3.6...linux-v0.3.7)
