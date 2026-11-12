# Dependency and redistribution audit

Status: technical review in progress, **not cleared for public binary distribution**.
Source/notice duties also apply when binaries are shared with private testers.

Shadowless derives from [Captail by FaulMit](https://github.com/FaulMit/captail).
Upstream history is retained (local upstream baseline `5f633f5`). Preserve the
original copyright, attribution and GPL-2.0-or-later grant. Renaming the application
does not replace its authorship or imply upstream endorsement.

| Component | Evidence / scope | Release requirement |
| --- | --- | --- |
| Captail/Shadowless | GPL-2.0-or-later; upstream history | Exact modified source and build scripts |
| Windows OBS 32.2.2 | Pinned acquisition; GPL-2.0-or-later | OBS sources, native modifications and transitive notices |
| Windows FFmpeg | Pinned LGPL shared/static build; upstream archive ships LGPL-3.0 | Exact build configuration, LGPL texts and corresponding sources |
| Windows libmpv | Pinned `git-bcde6b93cc`; exact upstream recipe enables FFmpeg GPL/version3 components | GPLv3-compatible distribution; obtain exact static dependency sources/configuration and notices |
| Qt 6 | Local 6.12.0 declares LGPL-3.0-only / GPL-3.0-only options | Compatible distribution route, Qt/third-party notices and library replacement as applicable |
| Linux FFmpeg | Local `2:9.0.2-2.1` declares **GPL-3.0-only** | Assess bundle under GPLv3-compatible terms; exact source/configuration |
| Linux libmpv | Local `1:0.41.0-6.1` declares GPL-2.0-or-later AND LGPL-2.1-or-later | Audit complete dependency closure |
| LayerShellQt | Local 6.8.2 library and Wayland plugin | Exact sources, build inputs and upstream notices |
| GSR | External 6.1.5, GPL-3.0-only; plugin header used | Review header/plugin boundary and ABI; not bundled or permission-configured |
| NAudio / H.NotifyIcon / .NET | NuGet lock file and published runtime | Dependency/runtime license and copyright texts |
| AppImage type2 runtime | SHA-256-pinned `runtime-x86_64`, embedded ahead of the bundle | Exact runtime/dependency copyright, licenses and corresponding source as applicable |
| Game catalog | Reduced MIT catalog with pinned source revision | Full MIT notice retained in `THIRD_PARTY_NOTICES.md` |

The GPL-2.0-or-later source grant permits choosing GPLv3 where required by the
dependency combination, including LGPLv3/GPLv3 components in either platform's bundle. Do not describe the Linux bundle as GPLv2-only or reuse
the Windows LGPL FFmpeg notice for the system's GPL Linux build.

Before sharing binaries:

- [ ] Enumerate shipped files, transitive libraries and Qt plugins.
- [ ] Record each component's source revision, patches, build configuration, archive hash and license.
- [ ] Include complete required license/copyright texts in each package.
- [ ] Provide exact corresponding source/build inputs to recipients; GitHub's automatic ZIP covers only this repository.
- [ ] Verify library replacement/relinking requirements where applicable.
- [ ] Check icons, screenshots, game catalog and notification sound provenance.
- [ ] Test native Windows upgrades and clean-host Hyprland/AppImage behavior.
- [ ] Record approved artifact hashes and explicitly clear this gate.

The AppImage's `BUNDLE_MANIFEST.txt` records file hashes and the glibc baseline.
It does not yet provide all third-party sources or certify license completeness;
these remain release blockers. This is a technical audit, not legal certification.

The [Windows 0.3.0 preflight](RELEASE_PREFLIGHT.md) records the exact
draft artifact hashes, recovered upstream source/build evidence, verified notice
additions and remaining concrete native-dependency gaps. The partial evidence
collection does not clear this gate or change existing draft binaries.

References: [GNU GPL FAQ](https://www.gnu.org/licenses/gpl-faq.en.html),
[Qt licensing](https://doc.qt.io/qt-6/licensing.html),
[FFmpeg legal checklist](https://ffmpeg.org/legal.html),
[AppImage deployment](https://docs.appimage.org/reference/best-practices.html).
