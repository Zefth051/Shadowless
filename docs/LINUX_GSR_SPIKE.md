# Native Linux / GSR experiment

Date: 2026-11-10. Status: exploratory prototype; no production migration approved.

## Intent and constraints

Test Shadowless on Linux using GPU Screen Recorder's actual capture pipeline,
retaining Shadowless's visual identity and core replay workflow, with Linux-specific
settings. The user requested a separate branch and clarified that the Linux
application should be native without needing to port Microsoft frameworks.

The experiment lives in the `linux/` subtree on `experiment/linux-gsr`, based on
committed Windows HEAD `5f633f5`. The original `shadowless-fork` worktree and all its
pending Windows changes were left intact. Those uncommitted changes are not
silently included in this branch. The existing icon, theme tokens, and English
resource strings are embedded as inert data, not loaded by WPF.

## Approaches

1. **Qt 6 / C++ with a managed GSR subprocess — chosen for this spike.** Native
   Linux executable and widgets, direct access to Unix sockets, no Microsoft
   runtime. Reconstructs the interface; existing C# application logic is not
   automatically reused. Qt is an engineering choice for this experiment.
2. **Rust or C++ / GTK.** Also native and viable. It would still require rebuilding
   WPF controls; no clear advantage for preserving this custom Shadowless layout in
   the initial capture experiment.
3. **C# / Avalonia with GSR.** Could retain more C# logic, but adds a .NET runtime
   and was set aside after the user clarified the preference for a native Linux
   branch. No C# backend was retained.

GSR is launched by Shadowless, not operated through GSR's own GUI. Shadowless constructs
capture options, supervises the child process, sends JSON requests, waits for
save completion, and presents the resulting clips. This exercises GSR's complete
capture/encoding method. Extracting/linking GSR internals, copying its source, or
maintaining a GSR fork is not necessary for this spike and has not been done.

## Ownership and capability boundaries

`GsrCapture` owns one QProcess and private socket directory. A single outstanding
IPC request prevents overlapping mutations. Stop queues behind a pending save;
window close waits for capture shutdown. Timeout/failure paths report uncertainty
instead of claiming a clip was saved. The destructor sends SIGINT to its own
child as a last-resort cleanup, then applies a bounded kill fallback.

`GsrCapabilities` parses `--info`. Only advertised screen/portal sources and
hardware H.264, HEVC, or AV1 enter the UI. The initial UI exposes no Windows or OBS
settings. A separate QSettings namespace prevents Windows config contamination.
No automatic recording, global desktop shortcut installation, root execution,
or changes to KMS helper permissions are introduced.

## Observed environment

- CachyOS Linux, Hyprland, Wayland.
- GPU Screen Recorder 6.1.5; NVIDIA adapter; DP-1 at 1920 × 1080.
- Hardware H.264/HEVC advertised; AV1 not advertised and therefore not offered.
- Qt 6.12.0 and GCC 16.2.1, C++17; qmake6 and GNU Make.
- Existing GSR KMS helper was already configured on this machine.

## Results

- Initial direct GSR/gsr-cli test: two H.264 replays and simultaneous continuous
  recording; all three files decoded without errors.
- Native Qt adapter suite: 14 passing cases including two live hardware-codec
  cases, zero failures. The live cases produced eight MP4/AAC clips total:
  two replays, one recording during replay, and one independent recording per
  H.264/HEVC case. Video was 1920 × 1080 at approximately 60 FPS.
- ffprobe confirmed streams and duration; FFmpeg fully decoded every native
  adapter test clip without errors.
- Save-followed-immediately-by-Stop preserved the clip before stopping.
- Reused the same adapter for replay then independent continuous recording.
- Native GUI opened on Wayland; Hyprland reported `xwayland: false`.
- A replay was started, saved, and stopped through the native window; its clip
  appeared with a thumbnail and decoded without errors.
- The executable is an x86-64 ELF linked to Qt libraries. No .NET/Mono runtime
  is linked. The Linux target never invokes the Windows solution or OBS build.
- An independent review found no important correctness issue; its output-volume
  free-space finding was corrected before completing the experiment.

Local evidence (ignored by Git): `.qa/linux-gsr/` for clips, per-file ffprobe JSON,
capture logs, and GUI screenshots; `.qa/linux-build/` for native build outputs.

The unchanged Windows baseline command fails here with `NETSDK1100` because it
targets Windows/WPF on Linux. Windows native builds and existing Windows QA were
not run. This is not evidence of a Windows regression in the Linux-only changes.

## Not established

No game/OBS performance comparison, prolonged session, driver reset, suspend,
multiple-GPU setup, AMD/Intel hardware, X11 capture, portal capture, HDR, AV1,
microphone audio, or real separate audio-track recording was validated. Separate
track argument mapping has offline coverage. No claim of full UI or behavior
parity is made. The built-in editor, global shortcuts, tray, advanced routing,
game detection, and capture recovery remain future work.

## Next decision after this experiment

If the prototype is kept, agree on a native UI scope and capture contract before
porting more behavior: configuration/capability model, desktop-global shortcuts,
tray lifecycle, app audio, replay library/editor, and recovery. Preserve the
process boundary initially; consider in-process GSR code only if an actual
requirement cannot be met through its supported IPC/CLI. Review the exact Qt/GSR
distribution and license obligations when preparing a package.
