# Windows 0.3.4 release candidate: 10-minute test

For testers on Windows 10 (or 11). Nothing here is sent anywhere automatically.

1. Install the 0.3.4 Setup over your current Shadowless (your settings are kept).
2. Open Shadowless, turn on **Instant Replay**, play or watch something with sound for
   30 seconds, press the save hotkey. Expected: a clip is saved and has sound.
3. Test the old failing setup. Close Shadowless, run `packaging\TestAudioFallback.ps1 -Mode Advanced`
   (right-click > Run with PowerShell), then start Shadowless again.
   Expected: Instant Replay starts, one orange notice says mixed audio is used, a saved clip has
   one audio track with sound, and the Audio settings still show Advanced.
   Repeat with `-Mode Separate`.
4. Run `packaging\DiagnoseStartup.ps1` and send the `Shadowless-startup-diagnostic.txt` it creates
   (it contains no paths, user or PC names).
5. Close Shadowless and run `packaging\TestAudioFallback.ps1 -Restore` to get your settings back.

If something fails, send the diagnostic file and say which step.
