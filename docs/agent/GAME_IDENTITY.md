# Game identity for replay folders

Keep the executable-stem folder when a catalog collision remains unresolved. With per-game organization enabled, a known `game.exe` belongs under `game` until evidence identifies a particular title. The bundled catalog has 190 distinct titles listing that basename, including Coloring Game and Coloring Game 2. A substring match would confuse even those two.

## Where identity is available

`GameCatalog.Load` builds a case-insensitive list of candidate names per normalized executable, then retains only single-title entries. Consequently `ExactNameForExecutable` returns null for both collisions and unknown names. `ReplayPaths.CaptureDirectory` already falls back to the sanitized executable stem. It selects the output root when organization is off or the executable is blank.

`App.SaveReplayGuardedAsync` starts `BeginSaveReplay` on the OBS thread and passes the operation's saved `GameExecutable` to `RouteSavedReplay` after mux completion. In automatic mode the engine takes the active executable only when a game was identified. In manual mode it takes the executable from a valid game hook. Continuous recording separately snapshots `RecordingGameExecutable` before engine disposal. Looking up the foreground process after completion could identify a different game.

## Evidence for a shared basename

Ranks are engineering judgments for a standard-user Windows session. The basename limits the candidate set before any ranking applies.

| Rank and signal | When it distinguishes candidates | Acquisition cost | Privacy exposure |
| --- | --- | --- | --- |
| 1. Install directory from the captured process image | Match the whole title directory. Try `steamapps/common/<title>`, `Epic Games/<title>`, `GOG Games/<title>`, or `XboxGames/<title>/Content`. Ignore subdirectories such as `Binaries/Win64`. Custom libraries, codenames and opaque WindowsApps package names can yield no match. | One limited-rights process query given a verified PID. Access denial or process exit can remove the signal. | Full paths may expose account names and private directory names. Discard the path after matching. |
| 2. PE ProductName | Compare the publisher-supplied value with each complete candidate title. Useful for a renamed install folder. Generic engine names, localization and shared edition metadata limit confidence. | One local version-resource read via `FileVersionInfo.GetVersionInfo` on a readable image. | Low for the product string. Keep the path used for the read local. |
| 3. PE FileDescription | An exact title can corroborate a candidate. Shared descriptions such as “Game Launcher” add no evidence. The two PE fields have a common author. | Available from the same version information. | Low, publisher-authored text. |
| 4. Hook window title | An exact title from the same hook can corroborate a candidate. Localized captions, level names and overlays make it unsuitable as the sole tie-breaker. | Low, one field in the existing OBS procedure. | High, captions can contain player or server names. Compare in memory and discard. |
| 5. Parent launcher | Steam, Galaxy or Xbox identifies a store shared by many titles. A verified game-specific parent could corroborate its child via a catalog match. A generic launcher cannot break the tie. | A process-tree lookup with PID and creation-time checks. Parents can exit and PIDs can be reused. | Process ancestry reveals unrelated application activity. Avoid command lines and whole-tree logging. |

`Interop/ProcessTopology.cs` already opens processes with limited query rights, obtains their image paths, and reduces them to basenames. Its nodes retain parent PID and process creation time. A future collector could reuse that query for a positively identified capture target. Matching the first running process named `game.exe` is insufficient. [Windows image-path API](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-queryfullprocessimagenamew)

Both PE fields are available in .NET. Upstream OBS exposes the title and executable together through `get_hooked`. Confirm the shipped plugin and capture mode before relying on the caption. [FileVersionInfo](https://learn.microsoft.com/en-us/dotnet/api/system.diagnostics.fileversioninfo) · [OBS game capture](https://github.com/obsproject/obs-studio/blob/master/plugins/win-capture/game-capture.c)

## Prototype and recommendation

`GameIdentity` accepts in-memory executable/title pairs, using the existing executable normalizer. A unique basename yields the canonical catalog title. For a collision, one exact, ordinal case-insensitive candidate match from the install directory or trimmed ProductName suffices. Conflicting matches return `Ambiguous`, as do missing candidates and inconclusive evidence. A supplied path with a different leaf invalidates the evidence for a collision. Folder matching covers ordinary drive-letter paths only. The resolver performs no discovery, file reads or routing.

For a future authorized integration, retain the candidate lists already built in `GameCatalog.Load`. Collect evidence once for the identified game process and carry the result with the save snapshot. If the collector cannot bind the evidence to that process, preserve the original executable for the existing folder fallback. Pass resolved catalog titles through the existing folder sanitizer. Raw paths, captions and process trees need no persistence.

Windows compilation and execution are pending. The QA cases use synthetic paths and metadata with catalog title pairs. They are intentionally unregistered, so a CI build can check compilation only. Before integration, measure exact-match coverage and false positives against actual store installs, particularly demos sharing retail metadata. Verify limited-rights failures and two simultaneous instances with the same basename across a hook or foreground switch.
