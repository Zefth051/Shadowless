# License texts

The root `LICENSE` preserves Captail/Shadowless's GPL-2.0-or-later grant.
These unmodified GNU texts were retrieved from the Free Software Foundation:

- `GPL-3.0.txt`: https://www.gnu.org/licenses/gpl-3.0.txt
- `LGPL-3.0.txt`: https://www.gnu.org/licenses/lgpl-3.0.txt
- `LGPL-2.1.txt`: https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt

They cover relevant license terms; they do not replace component copyright
notices or exact corresponding source. Linux's GPLv3 dependencies require a
compatible distribution of the combined bundle. Review `docs/DEPENDENCY_AUDIT.md`.

Combined Linux binary distributions select the GPLv3 route permitted by the
project's GPL-2.0-or-later source grant and required by the selected GPLv3
dependencies. Component-specific grants and attribution remain unchanged.
`third-party/linux/` preserves exact component notices; the release's separate
source-materials archive provides matching dependency sources and build inputs.
Extracting an AppImage allows replacing its shared libraries before running
`AppRun`; the source and packaging scripts also permit rebuilding the bundle.
