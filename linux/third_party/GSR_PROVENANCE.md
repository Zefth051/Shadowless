# Shadowless recording engine

Upstream: https://repo.dec05eba.com/gpu-screen-recorder
Version: 6.1.7
Commit: `1d4946a16139b446e6308f741e9ba99f2ed5b3a7`
Unmodified `git archive HEAD` SHA-256: `a610fb0e5d4119d7f6c30c5c38f19e4017fdc168f7244746de5b6cc6d38a067d`
License: GPL-3.0-only; see `gpu-screen-recorder/LICENSE` and upstream copyrights.

The full upstream source at that commit is kept in `gpu-screen-recorder/`.
`GSR_SOURCES.sha256` records the imported tree including the local patch below;
the build verifies every file before compilation. Update that manifest deliberately
when changing our engine source. Git history records the reviewed changes.
Shadowless builds only the recorder and KMS helper, not `gsr-cli`, services,
capability installation hooks, driver configuration or any graphical GSR project.
The overlay plugin uses the plugin header from this exact tree.

Local modification: `kms/client/kms_client.c` accepts the absolute
`SHADOWLESS_GSR_KMS_SERVER` path supplied by our launcher and refuses to search
PATH for an unrelated helper. The launcher stages the matching helper and private
libdrm outside the user-only AppImage mount for Polkit, preserves signal forwarding,
and removes staged files after the recorder exits. `src/cli/main.c` and
`kms/server/kms_server.c` call `include/shadowless_parent.h` to bind their lifetime
to their parent using Linux PR_SET_PDEATHSIG/SIGKILL with a parent-race check.
The launcher supplies its PID so the recorder also rejects an already-reparented
start under a subreaper.
This prevents a forcibly killed launcher from leaving recording or a privileged
helper running. Capture stages helper files inside its owned runtime directory;
the parent cleans them after process exit, including a hard stop. Standalone
engine invocations use `/tmp`; SIGKILL of the standalone launcher may leave that
temporary directory, but cannot leave its recorder/helper capturing.
No permanent capabilities are set.

Build inputs: `linux/engine/build.sh`, upstream Meson files, system development
dependencies recorded in release `BUILD_PACKAGES.txt`; shared library hashes appear
in the bundle manifest. Future engine patches belong in this source tree, with
this provenance updated and recording regressions rerun.
