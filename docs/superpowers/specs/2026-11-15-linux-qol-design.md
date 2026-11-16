# Linux QOL and Plasma Wayland

## Intent
Improve the Linux release's daily usability, repair public update checks, and support Plasma 6 Wayland alongside Hyprland. Preserve capture, user files and existing settings.

## Approach
Extend the existing Qt/LayerShellQt application and desktop portals. Desktop-specific Hyprland calls must only run in Hyprland. A separate KDE application would duplicate the capture and UI stack and is rejected. X11 is outside this change.

## Deliverables
- Reproduce the 0.3.3 update-check failure using the actual public package; repair its cause, preserve TLS validation, trusted repository and checksum checks, and show actionable network errors.
- Keep clip playback, folder and Trash actions visible in narrow layouts.
- Accept valid #RRGGBB accents, persist them, and preserve existing named presets.
- Verify game folder detection and safe unidentified fallback without moving existing clips. Never guess a game when identity is ambiguous.
- Repair application audio icons using available desktop metadata and a bundled fallback.
- Permit Plasma Wayland startup without hyprctl; use supported portal shortcuts and existing capture/overlay stack. Preserve Hyprland bindings. Document any feature with no supported Plasma equivalent.

## Validation and limits
Run Qt unit/UI tests and packaging tests, build the application and package, and exercise HTTPS in the bundled runtime. Test Plasma integration where an actual Plasma session is available; otherwise explicitly record that live capture, shortcuts and overlay behavior remain unverified there. No version bump, publication, push or changes to existing releases are included. Windows implementation is separate; shared roadmap items stay open until parity is established.

## Concrete compatibility decisions
- On Plasma, automatic game identification falls back to the existing output root whenever no supported reliable focused-window identity is available. Do not introduce KWin scripts or privileged inspection.
- Conflicting catalog identities count as unidentified. Preserve Steam-title fallback only for a focused native Steam identity and no contradictory catalog identity.
- Portal shortcuts request the configured triggers, honor desktop approval, and report assigned bindings; settings changes must re-register the portal session rather than displaying inactive custom values. Guard Hyprland cleanup and reload handling as well as startup.
- Custom accents derive hover, dim and readable foreground colors from #RRGGBB. Retain the existing application icon as fallback.
- Retain name-based audio routing; preserve available icon metadata alongside display names and use a bundled generic icon when desktop icons cannot be resolved.
- Verify clip actions at 400 px library width and with scaled UI. Record live Plasma capture, shortcut, layer-surface and tray validation independently.

## Approved visual refresh
Approved by the owner on 2026-11-15, with an additional Linux UI facelift using Emil design engineering and Ponytail. Preserve the native Qt application and existing workflow. Improve visual hierarchy, typography, spacing, surfaces, control consistency, pressed/hover/focus states, clip rows and settings readability. Use Windows as a reference, not a framework rewrite. Keyboard panel activation remains instant; do not add perpetual decorative animation or recording overhead. Capture representative dashboard/settings screenshots with synthetic media for visual review.
