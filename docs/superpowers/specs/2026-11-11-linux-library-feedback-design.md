# Linux library and save feedback

Approved scope: optional organization by game, library filtering, and passive
save notifications with the Windows confirmation sound. Cursor capture is deferred.

Reuse the Windows executable catalog and existing LayerShellQt passive surface.
Organization defaults off, leaves existing clips alone, and keeps unidentified
captures in the root. Detect games only on save/recording start using read-only
Hyprland queries and process metadata; never focus or activate a game.
Preserve saved files on routing failures and avoid replacing existing files.
Scan nested collections asynchronously and filter before limiting visible clips.

Notifications show saving, success or failure without keyboard/pointer input,
activation, animation, or polling. Play the bundled sound asynchronously only
after a confirmed save. Reuse one surface, timer and sound effect.
Offline checks cannot prove Rocket League fullscreen behavior or FPS impact;
verify native Wayland focus/input where available and report remaining limits.

Approaches: desktop notifications depend on the notification daemon and its focus
policy; a passive LayerShellQt surface reuses existing app behavior and controls
input explicitly. Use the latter. No additional dependencies.
