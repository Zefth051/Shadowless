# Linux editor audio refinement

The user approved stable audio rows with clear activation buttons and compact
trim controls matching the existing Shadowless theme. Preserve independent
recording volume/mute while investigating removal of virtual input devices.

Use the existing Qt player, track selection, waveform and export paths. Show all
four available tracks in the editor, retain the row when muted, and format trim
times as minutes:seconds.milliseconds with direct text entry and I/O shortcuts.
Do not infer missing tracks from today's capture settings: old files may contain
different sources. Do not fabricate four tracks in files that contain fewer.

For capture, prefer private streams over public source devices only if real
signal, gain, mute and cleanup checks pass. Hiding devices from a selector alone
does not establish working capture. Never change global device volumes or defaults.
The previous disconnected-stream attempt produced silence, so repeat its signal
checks before adopting any replacement. No new dependencies or Windows changes.

Alternatives: retain the current tiny checkboxes/scroll allocation (does not meet
the UI request); remove capture gain/mute for direct GSR capture (user rejected).
The implementation choice is to reuse the current controls and investigate a
private recording route without sacrificing their behavior.
