using Shadowless;

namespace ProcessAudioFoundationQa;

internal static class SettingsFailureQa
{
    internal static void Run()
    {
        var previous = new Config { BitrateMbps = 20 };
        var candidate = previous.Clone();
        candidate.BitrateMbps = 35;
        Require(SettingsFailureContext.CaptureOptionKeys(previous, candidate)
            .SequenceEqual(new[] { "L.Video.Bitrate" }), "Bitrate failure lost its option name.");
        candidate.Codec = "hevc";
        Require(SettingsFailureContext.CaptureOptionKeys(previous, candidate)
            .SequenceEqual(new[] { "L.Video.Bitrate", "L.Video.Codec" }), "Multiple capture changes were hidden.");
        Require(SettingsFailureContext.CaptureOptionKeys(previous, previous)
            .SequenceEqual(new[] { "L.Settings.Capture" }), "Unchanged capture needs a clear fallback.");
        previous.ProcessAudioRoutes = [new() { Executable = "game.exe", Track = 2 }];
        candidate = previous.Clone();
        Require(SettingsFailureContext.CaptureOptionKeys(previous, candidate)
            .SequenceEqual(new[] { "L.Settings.Capture" }), "Cloned routes were reported as changed.");
        candidate.ProcessAudioRoutes[0].Track = 3;
        Require(SettingsFailureContext.CaptureOptionKeys(previous, candidate)
            .SequenceEqual(new[] { "L.Audio.PerAppRouting" }), "Routing failure lost its option name.");
    }

    private static void Require(bool condition, string message)
    {
        if (!condition)
            throw new InvalidOperationException(message);
    }
}
