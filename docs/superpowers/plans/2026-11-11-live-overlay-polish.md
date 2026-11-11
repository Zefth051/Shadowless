# Live overlay preview and dashboard polish

**Goal:** Show the full live capture with editable layers, offer a layers-only view,
and bring the Linux dashboard closer to the Windows reference.

**Architecture:** Reuse OverlayCanvas, PreviewClient, existing Qt controls and theme.
No dependencies, capture startup, settings format or Windows code changes.
Work in the existing Linux worktree and preserve all pending changes.

## Tasks

- [x] Add a preview regression that receives a real socket frame, checks layer
  composition, switches to layers only, and confirms preview requests stop.
- [x] Add the Live capture / Layers only selector to OverlayEditor. Use the same
  canvas and media cache; preserve source aspect and layer geometry across modes.
- [x] Recreate the Windows dashboard's large replay/recording cards using the
  existing buttons; refine chrome, audio grouping and spacing. Check narrow layout.
- [x] Build and run the offline suites; inspect rendered dashboard/editor images.
- [x] Diagnose cursor loss from command construction, tests and local GSR evidence;
  document the cause without changing capture behavior on an unverified assumption.

## Review focus

Composition disabled must match the recorded output in live mode. Layers-only
mode must remain editable with capture off. Hidden canvases must stop requests.
No settings or media may be overwritten. Existing dashboard actions and keyboard
navigation must retain their behavior.
