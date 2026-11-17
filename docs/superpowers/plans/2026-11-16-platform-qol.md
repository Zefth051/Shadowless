# Platform QOL implementation plan

**Goal:** Add useful roadmap work to Windows and improve Linux editor readability and navigation.

**Design:** Keep WPF/Qt, current capture engines and release versions. Windows gains a validated, persistent #RRGGBB accent through staged settings, with readable foregrounds and preset fallback. Linux fixes the inherited light waveform background, removes heavy audio-row borders, lightens timeline outlines, and adds sidebar filters by game folder and recording mode. Preserve independent tracks and existing trim/export behavior. No new runtime dependency or publication.

**Choice:** A focused editor/appearance update is smaller and more immediately useful than adding Discord RPC. Roadmap selection is an engineering choice delegated by the user's request; the user can still steer the package.

## Tasks

- [x] Windows: extend accent normalization, palette generation, settings fields and localization; add QA for valid/invalid values, persistence and contrast. Preserve Cancel/Done behavior and avoid capture restart. Executable WPF QA remains pending.
- [x] Linux: reproduce waveform rendering on a light desktop palette; use the editor's theme palette; preserve visible waveform/silence and track selection.
- [x] Linux: remove decorative audio frames and reduce timeline stroke while preserving trim handles, focus indicators and drag behavior.
- [x] Linux: add game-folder and recording-mode sidebar filters, retain active playback and thumbnail mapping, navigate only through visible clips, test empty/reset cases.
- [x] Validate available scoped checks, review both diffs, update platform roadmaps with implementation and verification limits.

## Verification

Windows: relevant QA and Release build if SDK/PowerShell are available; otherwise report the missing tools and keep runtime checks pending. Linux: qmake6/Make UI and media suites with isolated desktop services; inspect rendered editor captures. Do not equate headless checks with real desktop capture validation.

## Progress

- Linux waveform regression reproduced: local widget Base palette paints white under the themed dashboard. Background now uses the window palette. Qt UI target compiles; media tests encounter a sandbox PulseAudio wakeup failure and require isolated audio handling.
- Rendering was isolated in the existing waveform component to avoid requiring an audio device for palette tests. The UI regression and FFmpeg media suite pass.
- Review caught the dashboard's 20-clip/filter limit leaking into editor filters. A production integration regression reproduced it; the full metadata scan now feeds the editor independently while dashboard rendering/thumbnails remain bounded.
- Windows: 20 XAML files parse, feature localization keys cover all 11 dictionaries, and 167 extracted source assertions pass using Python regex. These are not executable PowerShell or WPF tests. Both platform diffs received independent review.
- Added report: KDE Plasma crashes when scrolling Settings. An isolated harness and integrated scroll smoke test do not reproduce it. No speculative production change made; real-session backtrace/runtime reproduction remains required.
