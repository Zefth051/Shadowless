# GitHub publication and Linux updates

The user requests publication of both platform branches, Windows Setup and Linux
AppImage releases, platform feature descriptions, a sensitive-data/license review,
in-app updates, and disclosure of AI development.

Reuse the existing single-repository Windows/Linux layout and Windows updater.
For Linux, use Qt Network to discover `linux-vX.Y.Z` releases, then offer an
explicitly confirmed download and atomic replacement of the running AppImage.
Apply the new version on the next launch; do not interrupt recording or restart
the application automatically. Check repository-scoped HTTPS URLs, SHA-256,
download limits and writable regular AppImage files. Development/private builds
disable network update checks. No private repository credentials enter binaries.

A link-only notification would not install updates; a private update service
would require credential management. Neither is needed for this request.

Retain upstream GPL attribution and history. Document unresolved dependency
source/provenance requirements honestly; do not certify unverified binaries.
Publish only the reviewed `windows` and `linux` branches, never archives/runtime.
Repository visibility follows the user's answer; absent one, stage privately.
