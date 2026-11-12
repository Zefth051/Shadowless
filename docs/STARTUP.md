# Startup options on Linux

Settings has two independent options, both off by default:

- **Launch Shadowless at login** opens Shadowless in the background at the next
  desktop login. It does not enable recording by itself.
- **Start Instant Clip when Shadowless launches** starts replay buffering on any
  fresh launch, including a manual launch. It does not register login startup.
  A previous continuous-recording selection never starts recording automatically.

Saving applies login registration immediately; replay-on-launch takes effect on
the next launch. Cancel leaves both saved options unchanged. Capture startup
errors use the existing dashboard status messages; drivers, permissions, audio,
and engine prerequisites still apply.

## Registration

Shadowless writes its own desktop entry to
`$XDG_CONFIG_HOME/autostart/io.github.zefth051.Shadowless.desktop` (normally under
`~/.config`). This supports sessions that process XDG autostart entries.

Standalone Hyprland also receives an explicitly marked Shadowless block in its
default `hypr/hyprland.lua` configuration, or `hypr/hyprland.conf` for older
configurations. Lua launches on `hyprland.start`; legacy configuration uses
`exec-once`. Saving or reloading the configuration does not launch another copy.
Other configuration text and symlinks are preserved. Disabling removes only the
Shadowless block and its desktop entry. A malformed block or failed write is
reported instead of silently claiming registration succeeded. Custom Hyprland
configuration locations are not currently supported by automatic registration.

AppImage registration uses the persistent AppImage filename, never the temporary
FUSE mount path. Source builds use the executable in the build directory; retain
that directory while login startup is enabled. Moving an AppImage requires
launching the moved copy once so its enabled registration can be refreshed.

The existing single-instance protocol handles sessions that process both
registrations: background launches do not open or toggle the existing window.
On older Hyprland configuration, the generated command uses the standard
`base64` utility to keep shell paths separate from hyprlang variable/comment
parsing.

## Clip folders

Every recent clip has an **Open containing folder** icon between **Open** and
**Move clip to Trash**. It opens that clip's actual directory, including its
game subfolder, through the desktop file manager.
