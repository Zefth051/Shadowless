# OBS 32.2.2 Windows dependency notices and build evidence

These unmodified notice files were recovered from checksum-verified upstream
OBS dependency and source archives. `SOURCES.json` identifies their original
paths and hashes. `BUILD_PROVENANCE.json` records the exact source pins, recipe
patches and binary mappings reviewed for the OBS-derived subset of the Windows
0.3.0 Portable candidate. This directory is copied into future release packages
by the existing recursive `licenses/` packaging step.

All 34 upstream OBS native executables/libraries in that Portable match the
official OBS 32.2.2 Windows archive byte-for-byte. Twelve dependency DLLs also
match the official 2026-07-15 prebuilt after removing only the Authenticode
certificate table and zeroing PE CheckSum/security-directory fields. The
additional `shadowless-process-audio.dll` is our source-built module; its source
and build scripts are in the matching Shadowless source archive.

The local ignored source collection contains 27 pinned upstream source archives,
the full OBS source and repository snapshots, obs-deps recipes and patches, and
the matched notice set. Librist's nested mbedTLS and cJSON notices are included;
OBS's bundled pthreads, libcaption, blake2 and json11 notices are preserved.
The cJSON header is retained unchanged because it carries the complete notice.
Some collected headers/test dependencies and their notices are a superset of
the shipped subset. The unmodified full upstream recipes identify build options.

This evidence addresses the OBS subset. It does not establish complete sources
or notices for the separate libmpv or BtbN FFmpeg builds and does not clear the
whole-product distribution gate. Existing uploaded binaries are unchanged; this
notice collection must accompany a reviewed distribution or rebuilt candidate.
