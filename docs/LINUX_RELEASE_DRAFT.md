# Shadowless Linux 0.3.4 — QOL update

This release makes capture authorization accessible, explains why capture settings
are locked, and improves everyday controls on Linux. Existing clips and settings
are preserved.

### Improved

- **Capture authorization:** Release the control overlay only when the managed KMS engine requests Polkit authorization, then restore its previous visibility after authorization or failure. Background startup stays hidden; an explicit hide or quit prevents restoration. System authorization is still required for direct KMS capture without existing privileges.
- **Active capture settings:** Keep the capture-lock notice visible while scrolling and slightly dim locked fields, so it is clear why recording options cannot change until capture stops. Appearance, shortcuts and startup settings remain available.
- **Settings readability:** Keep checked boxes visibly filled while capture locks their settings, enlarge checkbox indicators, and keep engine notices on the dashboard instead of settings or the editor.
- **Appearance:** Add validated, persistent HEX accents with readable button text and keyboard focus, compact capture cards and consistent native control states.
- **Recent clips:** Keep playback, containing-folder and Trash actions visible in narrow panels using adaptive filename elision.
- **Game folders:** Reject conflicting game identities and preserve files when falling back to the output root. Plasma uses that fallback without invoking Hyprland tools.
- **Application audio:** Use application icon metadata when available, with theme lookup and a bundled generic fallback. Include the Qt ICO reader for packaged app icons.
- **Plasma Wayland:** Accept KDE Plasma sessions and use portal shortcuts, including configured triggers and desktop-approved assignments, without requiring Hyprland tools. Live Plasma capture and shortcut approval remain unverified.
- **Updates:** Report HTTP/rate-limit and TLS failures with actionable details. Public discovery, verified download and atomic installation were exercised on temporary AppImages; the reported intermittent 0.3.3 check error has not been reproduced.

### Upgrade notes

- Use **Check for updates** from an update-enabled 0.3.3 AppImage, then quit and reopen after capture finishes. Keep the AppImage in a writable location. A manual AppImage installation remains available if the old build reports an error.

### Known limitations

- Live Plasma capture/shortcut approval and the real Polkit interaction still need desktop validation. The full HTTPS updater passes isolated download/install tests; a real public 0.3.3-to-0.3.4 upgrade remains an owner test after publication.

### Download and compatibility

Download `Shadowless-0.3.4-linux-x86_64.AppImage` and `SHA256SUMS.txt`, verify with
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

[Full changelog](https://github.com/Zefth051/Shadowless/compare/linux-v0.3.3...linux-v0.3.4)
