# libmpv Windows source and notice materials

These materials accompany Shadowless's existing shinchiro Windows libmpv DLL.
The DLL directly identifies mpv bcde6b93cca4d128d84e1a60d1a6db2cd81c889e
and embedded FFmpeg cd4e96eb314630788b7ff2f9820c395788c023e2.

The upstream build is https://github.com/shinchiro/mpv-winbuild-cmake/actions/runs/42927840447
at builder bda10a2816ce9908d419b3f4b564227330457e76. Its build controls,
patches, configuration evidence and logs accompany the source archives.
SOURCE_MANIFEST.json identifies the 88 selected source components and archive hashes.
Exact upstream archive hashes are checked for libiconv and libopenmpt.
Source preparation preserves original source bytes; LuaJIT and fontconfig snapshots
also apply the original builder patches. All other original patches remain in the
builder source. AMF is limited to the recipe's amf/public/include subtree plus its
full license; unused proprietary SDK binaries and samples are excluded.

COMPILED_SOURCE_MD5_VALIDATION.json records matching source bytes for all 5,853
PDB source references across 69 packages, including linked subcomponents.
The PDB comes from the standalone mpv executable debug artifact in the same
workflow and source-cache build. It is NOT a GUID-matching PDB for the DLL;
that DLL's libmpv-2.pdb was not published. This evidence corroborates shared
compiled source inputs without claiming direct PDB identity for the DLL.
The matching cached CRT/winpthreads tree is 5ea8e9facd013b815f5f29f20ef26a566319a3de;
matching LLVM runtime source is provided by llvmorg-22.1.8, rather than assuming
the builder's newer toolchain recipe was rebuilt for this cached artifact.

The source ZIP contains full selected source archives, the complete original
builder archive, all preserved copyright/license notices, configuration evidence,
and the per-file validation. Extract individual source archives to inspect or
adapt the original builder recipes to these resolved source snapshots. The
upstream recipes originally used mutable Git checkouts and cached toolchains;
this collection does not claim a bit-for-bit reproducible rebuild or legal certification.

The notice files reproduce upstream copyright and license material; upstream
components retain their own terms. Full source archives retain their additional
per-file notices. SHA256SUMS.txt covers source inputs and preserved build evidence.
