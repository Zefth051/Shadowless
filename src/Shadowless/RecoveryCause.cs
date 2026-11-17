namespace Shadowless;

internal static class RecoveryCause
{
    // Prefer the backend's real startup error over the generic watchdog text so
    // the recovery loop tells the user why capture cannot start.
    internal static string Describe(string? startupError, string genericReason) =>
        string.IsNullOrWhiteSpace(startupError)
            ? genericReason
            : startupError.Trim().TrimEnd('.') + ".";
}
