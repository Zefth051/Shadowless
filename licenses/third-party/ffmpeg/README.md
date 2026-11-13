# BtbN FFmpeg Windows source and notice evidence

Scope: the pinned LGPL-shared FFmpeg build used by Shadowless's Windows clip
tools, separate from OBS's FFmpeg and libmpv's embedded FFmpeg. All nine shipped
executables/DLLs match the SHA256-pinned upstream archive byte-for-byte.
BUILD_PROVENANCE.json records the exact release run, source pins, enabled build
recipes, original cache mount names, binary hashes and source delivery records.

Recovered source inputs comprise the exact FFmpeg source, 82 distinct pinned
top-level dependency trees, 27 nested trees, LAME SVN revision 7113 and 601
checksum-verified Rust crate archives. All 87 crateversions identified in actual
binary paths match the recorded lockfiles. The Rust standard-library source
commit is identified directly by those binaries. Exact upstream patch/build
scripts are supplied with the sources; no bit-for-bit rebuild is claimed.

The AMF recipe only installs headers: source delivery preserves those 58
header/license files unchanged and omits unrelated SDK binaries/docs. The
derivation records original and delivered hashes plus each file's hash.

Original notice texts are preserved byte-for-byte inside combined component
files. SOURCES.json records each original source path/digest and its byte offset
and length in the combined file, allowing independent extraction/verification.
Cargo manifests are preserved for author/SPDX metadata. Several upstream crates
omit license documents: available repository-specific texts were recovered,
and canonical MIT/MPL 2.0 wording is included beside the original package
metadata. No copyright owner or year is invented. Older crates without VCS
metadata are explicitly identified when a current upstream notice is used.

The notice/source set includes optional platform/test/build dependencies as a
superset. General-purpose toolchain acquisition and cargo-update-cc are recorded
reproducibility limits; they are not presented as proof of a native rebuild.
This evidence addresses the standalone BtbN FFmpeg subset and does not certify
the whole product or resolve the separate libmpv audit. Existing uploaded
binaries are unchanged; a reviewed notice/source supplement or rebuild must
accompany distribution.
