# Rust feasibility assessment

Status: recommendation, 2026-11-16. No migration or new branch has been started.

## Recommendation

Keep the `windows` and `linux` branches for now. A full Rust rewrite is technically
possible, but it does not address the reported capture or notification defects.
Consider a small shared Rust library only after identifying duplicated behavior
that costs more to maintain than a new build toolchain and two language boundaries.

This is a source review, not a Rust prototype or a measured performance study.

## Current implementation

| Area | Windows | Linux |
| --- | --- | --- |
| UI | C#/WPF, .NET 10 (`src/Shadowless/Shadowless.csproj`) | C++17/Qt 6 (`linux/app/ShadowlessLinux.pro` on `linux`) |
| Capture | In-process libobs, serialized graph operations, native C++ bridge | Managed GSR process, native compositor plugin |
| Audio/integration | WASAPI, Windows hotkeys, WinRT, DXGI | PipeWire, Wayland portals, Hyprland/Plasma integration |
| Playback/export | libmpv, FFmpeg tools | libmpv, FFmpeg tools/libraries |
| Distribution | Portable/installer and Store-specific behavior | AppImage, managed GSR/helper lifecycle |

The checkouts already belong to the `windows` and `linux` branches of one Git
repository. They have different engines and interfaces, not just different build
flags. Common product behavior includes settings, replay naming/organization,
library rules, media workflows and update policy, but implementations and
platform contracts differ.

## Options

| Option | What it preserves | Main cost | Assessment |
| --- | --- | --- | --- |
| Continue two native branches | Current UI, capture engines, packaging and QA | Shared product changes require two implementations; parity can drift | Lowest immediate cost; recommended |
| Introduce a shared Rust library gradually | Existing WPF/Qt interfaces and capture engines | C ABI/FFI ownership and error contracts, Cargo in both builds, library packaging | Viable when a specific shared module justifies it |
| Rewrite the application in Rust | Potential to share more application logic | Rebuild the UI or retain mixed-language frontends; port integrations, configuration compatibility, packaging and regression coverage | Highest risk; no demonstrated benefit for the current bugs |

Rust can call a C ABI, so libobs/libmpv can remain native dependencies. Existing
C++ plugins still need their current implementation or an explicit C boundary.
Keeping those engines means their GPU, driver, API and threading constraints
remain. Rewriting the application does not remove these constraints or establish
faster encoding. A pure Rust application would also require a UI decision: WPF is
a .NET/Windows framework, not a portable Rust frontend.

## Working with two branches

- Keep platform fixes in their platform branch; do not merge entire platform
  implementations just to synchronize one product feature.
- Define shared behavior with small examples/fixtures when parity matters:
  replay paths, settings defaults, media outputs and error handling.
- Keep release tags, dependencies and verification specific to each platform.
- If shared code becomes substantial, evaluate one tree with platform folders
  and a shared module. That repository change is independent of adopting Rust.

## A bounded future experiment

If duplicated replay-path rules become a maintenance problem, compare extracting
that one pure module to Rust against retaining two small implementations. Preserve
existing naming, organization and collision behavior using fixtures from both
platforms. Expose a narrow C ABI to C# and Qt; keep capture and UI untouched.
Compare packaging complexity, development effort and actual duplicated code
removed before adopting the prototype. Do not maintain a third full product
branch indefinitely.

No reliable rewrite timeline follows from this review. It would require a
feature inventory, chosen UI and a working vertical slice on both operating
systems. Capture validation still requires Windows hybrid-GPU hardware and real
Linux desktop sessions regardless of implementation language.

## Sources

- Local code: `src/Shadowless/Shadowless.csproj`, `ObsReplayEngine.cs`,
  `Interop/CaptureInterop.cs`, `native/ObsBridge/ShadowlessObsBridge.cpp`;
  Linux branch: `linux/app/ShadowlessLinux.pro`, `linux/capture/`,
  `linux/platform/`, `linux/README.md`.
- [Rust FFI and native-library boundaries](https://doc.rust-lang.org/nomicon/ffi.html).
- [Microsoft WPF overview](https://learn.microsoft.com/en-us/dotnet/desktop/wpf/overview/).
- [OBS hybrid-GPU capture guidance](https://obsproject.com/kb/laptop-troubleshooting).

The recommendation and migration costs above are engineering judgments based on
these sources, not promises made by those projects.
