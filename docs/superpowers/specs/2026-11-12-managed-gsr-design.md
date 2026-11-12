# Shadowless-managed GSR

## Intent

The Linux AppImage must capture without an installed GPU Screen Recorder package.
Shadowless owns the engine version and can carry reviewed patches in future.
Keep the recording engine source in the Linux branch so engine changes can be
reviewed, built, and released together with Shadowless.

## Approach

Vendor the pinned GSR engine source in `linux/third_party/gpu-screen-recorder/`.
Bundle its source-built recorder executable and matching KMS helper. Retain
the existing subprocess/IPC integration. Linking GSR internals into the Qt app
would require a larger integration rewrite without improving this requirement;
downloading the engine at first launch would add a network dependency.
The source-backed, bundled subprocess is the proposed approach. It gives users
one Shadowless application without requiring all its components to share a process.
Do not include GPU Screen Recorder GTK, GPU Screen Recorder UI, their overlays,
or their launchers. Those are separate projects, not required by the recorder.
Only the capture engine and required helper are shipped; Shadowless owns the UI
and the features it exposes. Keep upstream attribution and record local changes.

## Proposed implementation

- Pin GSR 6.1.7, source commit `1d4946a16139b446e6308f741e9ba99f2ed5b3a7`.
  Verify the imported source, record build inputs, and compile the overlay against
  that engine's plugin header. Keep local patches and corresponding source with
  the distribution inputs.
- Package the recorder, matching helper, required libraries, license and source
  provenance. AppImage execution always selects the managed recorder, never a
  recorder found on the user's PATH. Source builds use the same managed engine.
- Update the engine with Shadowless releases; no separate engine updater or
  engine-selection UI. Do not introduce speculative optimizations.
- Use GSR's existing Polkit authorization for direct KMS capture. Never run the
  Qt application as root or silently grant permanent capabilities. Ensure the
  matching helper is accessible outside a user-only FUSE mount and can load its
  dependencies after Polkit sanitizes the environment.
- Keep drivers, Hyprland, PipeWire and the existing non-GSR host tools as system
  prerequisites. Removing every Linux system dependency is outside this change.
- Update the AppRun checks, packaging workflow, installation guides, comparison
  and third-party notices to describe the managed engine accurately.

## Validation

Test launch with no host recorder, selection despite a conflicting host recorder,
missing/corrupt engine failures, library isolation, helper pairing, and source
verification. Build the real engine and AppImage and run packaging/offline tests.
Live KMS recording and cancelled authorization need a real Polkit session; report
separately if that interaction cannot be exercised in the available session.

## Status

Approved by the project owner on 2026-11-12, including the source-backed packaging
design and Polkit authorization behavior. Implementation and validation are tracked
in the implementation plan; live authorization/capture results must be reported
separately from offline build and packaging results.
