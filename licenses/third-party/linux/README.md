# Linux third-party notices

These original notices accompany Shadowless's native Linux AppImage. Component
copyrights and licenses remain with their respective authors. Shadowless retains
Captail's GPL-2.0-or-later source grant; the combined Linux distribution uses the
GPLv3-compatible route required by its GPLv3 and LGPLv3 dependencies.

`NOTICE_MANIFEST.json` identifies every preserved source file and its SHA-256.
The collection includes notices for upstream source, build and test files.

- Component directories preserve the notices from the exact Arch binary packages
  and their corresponding upstream source snapshots. Their package versions,
  binary build IDs, source recipes, patches and build configuration are recorded
  in the release's Linux source-materials supplement.
- `rust-crates/` preserves notices from the Cargo.lock-selected sources used by
  the Rust dependencies. The complete vendored sources and lock files accompany
  the supplement. Cargo package checksums and every vendored file hash were checked.
- `appimage-runtime/` preserves notices for the pinned AppImage runtime and its
  statically linked FUSE, squashfuse, musl, zlib, zstd and mimalloc dependencies.
  The runtime's original build log, build scripts, patches and complete sources
  accompany the supplement.
- `common/` contains the relevant GNU/SPDX license and exception texts from the
  build environment's exact `licenses` package. Component copyright statements
  remain in the component-specific notices and corresponding source.

Download the Linux source-materials archives and checksum file attached to the
same Shadowless release. They contain the complete dependency source snapshots,
Arch PKGBUILD/SRCINFO recipes, patches, package BUILDINFO/PKGINFO, Rust vendor
sources, runtime source/build inputs and provenance. The full Shadowless source
archive is provided for the exact application build commit, including the
maintained GSR engine source. Preserve all of these materials when redistributing.

The native dependency baseline was checked against the Linux 0.3.1 cloud build:
218 third-party ELF paths matched build IDs from 146 SHA-256-verified official
Arch packages, and all 145 distinct PKGBUILDs matched the hash embedded in their
binary package BUILDINFO. Later candidates must recheck that baseline against
their actual bundle and build-package list before reusing these materials.
