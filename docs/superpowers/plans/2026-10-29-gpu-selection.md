# Capture GPU selection

**Goal:** Allow discrete (default) or integrated/power-saving capture graphics in Video settings for both replay and recording.

**Architecture:** Persist a `GpuPreference` value (`high-performance` or `power-saving`) in Config. Reuse the native DXGI preference-to-adapter-index mapping with minimum-power selection for integrated graphics. Windows chooses the available adapter when a preferred GPU class is absent. Preserve the existing encoder capability detection on the selected OBS device.

**Scope:** Existing settings layout, localization dictionaries, Config copy/normalization/pipeline comparisons, managed GPU selection, native OBS bridge. No version or release changes. Work in the existing dirty checkout to preserve the user's current implementation.

**Checks:** Missing/invalid preferences default to high performance; integrated selection survives clone and serialization; GPU changes require a restart in replay and recording; settings stage/cancel/apply consistently; native preference mapping skips software adapters and maps adapter LUIDs back to enumeration indices.

- [x] Add GPU configuration cases to AudioSourceQa and wiring checks in TestGpuSelection.ps1; observe the missing-feature failure.
- [x] Add native minimum-power preference and managed selection.
- [x] Wire persistent GPU settings, localized controls and restart comparison.
- [x] Run available checks and review the scoped diff.
- [ ] Run .NET QA, the Release build and capture on a Windows hybrid-GPU laptop.

**Environment:** Linux without dotnet or pwsh in PATH; SDK download is blocked by DNS. Static script assertions can be checked with Python, but this does not replace the .NET QA, Windows build or hybrid-laptop capture tests.

**Verification:** Ten wiring assertions from TestGpuSelection checked using Python; selector XAML and all eleven localization dictionaries parsed successfully. The actual native GPU-selection section compiled with clang++ against simulated DXGI and passed eleven assertions covering both preference orders, a single available GPU class, software adapters, failures and unmatched LUIDs. `git diff --check` passed. Native build/publish targets already rebuild and copy the bridge DLL.
