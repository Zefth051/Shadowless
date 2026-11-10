namespace Shadowless;

internal static class SettingsFailureContext
{
    internal static IReadOnlyList<string> CaptureOptionKeys(Config previous, Config candidate)
    {
        var options = new List<string>();
        Add(previous.CaptureMode != candidate.CaptureMode, "L.Capture.Mode");
        Add(previous.ReplayEnabled != candidate.ReplayEnabled, "L.Replay.Instant");
        Add(previous.BufferSeconds != candidate.BufferSeconds, "L.Replay.BufferLength");
        Add(previous.MaxReplaySizeMb != candidate.MaxReplaySizeMb, "L.Replay.BufferLimit");
        Add(previous.BitrateMbps != candidate.BitrateMbps, "L.Video.Bitrate");
        Add(previous.Codec != candidate.Codec, "L.Video.Codec");
        Add(previous.FrameRate != candidate.FrameRate, "L.Video.FrameRate");
        Add(previous.RecordingResolution != candidate.RecordingResolution, "L.Video.Resolution");
        Add(previous.CaptureSource != candidate.CaptureSource, "L.Video.Source");
        Add(previous.DesktopCaptureMethod != candidate.DesktopCaptureMethod, "L.Video.DesktopMethod");
        Add(previous.GpuPreference != candidate.GpuPreference, "L.Video.Gpu");
        Add(previous.MonitorIndex != candidate.MonitorIndex, "L.Video.Monitor");
        Add(previous.CaptureSystemAudio != candidate.CaptureSystemAudio ||
            previous.SystemAudioDeviceId != candidate.SystemAudioDeviceId ||
            previous.SystemAudioVolume != candidate.SystemAudioVolume, "L.Audio.SystemAudio");
        Add(previous.CaptureMicrophone != candidate.CaptureMicrophone ||
            previous.MicrophoneDeviceId != candidate.MicrophoneDeviceId ||
            previous.MicrophoneVolume != candidate.MicrophoneVolume ||
            previous.MicrophoneBoostDb != candidate.MicrophoneBoostDb, "L.Audio.Microphone");
        Add(previous.AudioCodec != candidate.AudioCodec ||
            previous.AudioBitrateKbps != candidate.AudioBitrateKbps, "L.Audio.Codec");
        Add(previous.SeparateAudioTracks != candidate.SeparateAudioTracks, "L.Audio.Tracks");
        Add(previous.AudioRoutingMode != candidate.AudioRoutingMode ||
            previous.AdvancedMicrophoneTrack != candidate.AdvancedMicrophoneTrack ||
            !previous.ProcessAudioRoutes.Select(route => (route.Executable, route.Track, route.Enabled))
                .SequenceEqual(candidate.ProcessAudioRoutes.Select(route => (route.Executable, route.Track, route.Enabled))),
            "L.Audio.PerAppRouting");
        return options.Count > 0 ? options : ["L.Settings.Capture"];

        void Add(bool changed, string key)
        {
            if (changed)
                options.Add(key);
        }
    }
}
