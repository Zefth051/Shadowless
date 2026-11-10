# Local diagnostics

Portable builds include `ExportDiagnostics.cmd`. Double-click it, choose a ZIP
destination, and share that ZIP with the developer after reproducing a problem.
Export works while Shadowless is running or after a crash. Closing Shadowless normally
before exporting flushes the latest buffered lines.

Ordinary desktop builds keep their local logs in `%LOCALAPPDATA%\Shadowless`:
`log.txt` and up to four older files (`log.txt.1` through `log.txt.4`). Each file
rotates at approximately 2 MiB, keeping roughly 10 MiB in total. Existing oversized
logs rotate on the next write. Older history is eventually removed.

Logs record existing capture diagnostics, session version, Windows and .NET
versions, architecture, and exception details. Startup failures, unhandled UI and
background exceptions, and unobserved task exceptions are recorded. Unobserved
task exceptions are reported only when .NET raises its event. Exception handling
does not suppress fatal errors or attempt to continue an unsafe session.

The ZIP includes the latest 128 KiB from each available log. It reuses Shadowless's
existing filtering for paths, identifiers, device names, window titles, common
secrets, and network addresses, and excludes native libobs lines. No recordings,
settings, or memory dumps are included. Filtering is best effort: review the ZIP
before sharing. Original local logs can contain private details. Nothing is
uploaded automatically.

Native crashes, forced termination, power loss, and hangs cannot reliably run
.NET exception handlers. They may leave only their last flushed log lines; this
feature does not create Windows crash dumps or detect every bug automatically.

Validation: `dotnet run --project tools/DiagnosticsQa -c Release` exercises log
rotation, crash persistence, sanitized ZIP export, overwriting an export, and
export when logs are missing.
