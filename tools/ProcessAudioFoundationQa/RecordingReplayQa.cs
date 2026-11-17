using Shadowless;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

namespace ProcessAudioFoundationQa;

internal static class RecordingReplayQa
{
    internal static void Run()
    {
        string directory = Path.Combine(Path.GetTempPath(), $"shadowless-recording-replay-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        foreach (string codec in new[] { "aac", "opus" })
            foreach (string routing in new[] { "mixed", "separate", "advanced" })
                Exercise(codec, routing, directory);
        IndependentOutputs(directory);
        UnexpectedRecordingStop(directory);
        Console.WriteLine($"Recording/replay QA files: {directory}");
    }

    internal static void ConfigChanges()
    {
        var config = new Config { CaptureMode = "recording" };
        var candidate = config.Clone();
        candidate.BufferSeconds = 15;
        Require(!config.PipelineEquals(candidate), "Recording replay-buffer duration changes were ignored.");
        candidate = config.Clone();
        candidate.MaxReplaySizeMb = 500;
        Require(!config.PipelineEquals(candidate), "Recording replay-buffer size changes were ignored.");
        candidate = config.Clone();
        candidate.PauseRecordingHotkey = "Ctrl+Shift+F7";
        Require(config.PipelineEquals(candidate), "Pause hotkey edit restarted recording.");
        Require(candidate.Clone().PauseRecordingHotkey == candidate.PauseRecordingHotkey,
            "Config cloning lost the pause hotkey.");
        candidate.PauseRecordingHotkey = candidate.Hotkey;
        candidate.Normalize();
        Require(new[] { candidate.Hotkey, candidate.ToggleReplayHotkey, candidate.OverlayHotkey }
            .All(binding => HotkeyManager.AreDistinct(binding, candidate.PauseRecordingHotkey)),
            "Normalization retained a conflicting pause hotkey.");
        candidate = config.Clone();
        candidate.InstantClipEnabled = false;
        Require(!config.PipelineEquals(candidate) && !candidate.Clone().InstantClipEnabled,
            "Config lost the independent Instant Clip state.");
        candidate = config.Clone();
        candidate.AccentColor = "#000000";
        Require(config.PipelineEquals(candidate), "Appearance edit restarted recording.");
    }

    private static void Exercise(string audioCodec, string routing, string directory)
    {
        var config = new Config
        {
            CaptureMode = "recording", CaptureSource = "desktop", Codec = "h264",
            RecordingResolution = "720p", FrameRate = 30, BufferSeconds = 15,
            CaptureSystemAudio = routing is "separate" or "advanced", CaptureMicrophone = true,
            MicrophoneDeviceId = "shadowless-qa-unavailable-microphone",
            AudioCodec = audioCodec, SeparateAudioTracks = routing == "separate",
            AudioRoutingMode = routing == "advanced" ? "advanced" : "simple",
            AdvancedMicrophoneTrack = 6,
            OutputDirectory = directory, OrganizeReplaysByGame = false,
        };
        config.Normalize();
        using var engine = new ObsReplayEngine(config);
        string? fault = null;
        engine.Faulted += message => fault = message;
        engine.Start();
        if (routing == "advanced")
        {
            engine.ReconcileProcessAudio(Shadowless.Interop.ProcessSnapshot.Capture());
            Thread.Sleep(1000);
            engine.ReconcileProcessAudio(Shadowless.Interop.ProcessSnapshot.Capture());
            Require(Handle(engine, "_separateSystemSource") != 0 &&
                obs_source_get_audio_mixers(Handle(engine, "_separateSystemSource")) ==
                    ObsReplayEngine.MixerBit(config.AdvancedSystemAudioTrack),
                "Advanced System did not receive its own mixer track.");
        }
        Require(config.MicrophoneDeviceId == "shadowless-qa-unavailable-microphone",
            "Microphone fallback overwrote the saved preference.");
        Require((string)typeof(ObsReplayEngine).GetField("_microphoneDeviceId", BindingFlags.NonPublic | BindingFlags.Instance)!
            .GetValue(engine)! == AudioDevices.ResolveMicrophone(config.MicrophoneDeviceId),
            "Missing microphone did not resolve to the current default input.");
        nint replay = Handle(engine, "_output"), recording = Handle(engine, "_recordingOutput");
        nint videoEncoder = Handle(engine, "_videoEncoder");
        Require(replay != 0 && recording != 0 && replay != recording, "Recording and replay outputs are not independent.");
        Require(obs_output_get_video_encoder(replay) == videoEncoder &&
            obs_output_get_video_encoder(recording) == videoEncoder, "Outputs created separate video encoders.");
        for (nuint track = 0; track < (nuint)ObsReplayEngine.AudioTrackCount(config); track++)
            Require(obs_output_get_audio_encoder(replay, track) != 0 &&
                obs_output_get_audio_encoder(replay, track) == obs_output_get_audio_encoder(recording, track),
                "Outputs did not share every audio encoder.");
        engine.RefreshMicrophoneDevice();
        Require(Handle(engine, "_output") == replay && Handle(engine, "_recordingOutput") == recording,
            "Microphone reconciliation rebuilt the outputs.");
        uint micMixers = routing switch { "advanced" => 32u, "separate" => 4u, _ => 1u };
        Require(obs_source_get_audio_mixers(Handle(engine, "_microphoneSource")) == micMixers,
            "Microphone fallback changed its assigned audio track.");
        Thread.Sleep(3000);
        Require(engine.AvailableReplaySeconds > 0, "Recording mode has no replay history.");
        string recordingPath = engine.RecordingPath;
        string first = engine.SaveReplayAsync().GetAwaiter().GetResult();
        Require(File.Exists(first) && new FileInfo(first).Length > 0, "Clip was not saved during recording.");
        engine.ResetReplayWindow();
        Require(engine.IsActive && engine.RecordingPath == recordingPath && Handle(engine, "_recordingOutput") == recording,
            "Saving/resetting replay interrupted the continuous recording.");

        engine.SetRecordingPaused(true);
        Thread.Sleep(500);
        int pausedFrames = engine.EncodedFrameCount;
        int pausedSeconds = engine.AvailableReplaySeconds;
        Thread.Sleep(1500);
        Require(engine.IsRecordingPaused && engine.IsHealthy, "Pause was treated as a pipeline fault.");
        Require(engine.EncodedFrameCount == pausedFrames && engine.AvailableReplaySeconds == pausedSeconds,
            "The shared replay timeline advanced while paused.");
        bool rejected = false;
        try { engine.BeginSaveReplay(); }
        catch (InvalidOperationException) { rejected = true; }
        Require(rejected, "Paused clip save must fail promptly instead of waiting for a packet.");
        engine.SetRecordingPaused(true);
        engine.SetRecordingPaused(false);
        engine.SetRecordingPaused(false);
        Thread.Sleep(2500);
        Require(!engine.IsRecordingPaused && engine.EncodedFrameCount > pausedFrames,
            "Resume did not restart packet delivery.");
        Require(engine.RecordingPath == recordingPath, "Resume created a different recording file.");
        string second = engine.SaveReplayAsync().GetAwaiter().GetResult();
        Require(second != first && File.Exists(second), "Clip after resume was lost or overwrote the previous clip.");
        engine.SetRecordingPaused(true);
        Thread.Sleep(300);
        int recordingFrames = obs_output_get_total_frames(recording);
        engine.Dispose();
        Require(fault is null, $"Expected pause/stop raised a capture fault: {fault}");
        Require(string.IsNullOrEmpty(engine.RecordingStopError), $"Paused recording failed to finalize: {engine.RecordingStopError}");
        Require(File.Exists(recordingPath) && new FileInfo(recordingPath).Length > 0,
            "Stopping while paused lost the recording.");
        var ffmpeg = new FfmpegAdapter();
        foreach (string file in new[] { recordingPath, first, second })
            Require(ffmpeg.ReadAudioTracksAsync(file).GetAwaiter().GetResult().Count == ObsReplayEngine.AudioTrackCount(config),
                $"{audioCodec}/{routing}: saved file has the wrong audio track count: {file}");
        TimeSpan duration = ffmpeg.ReadDurationAsync(recordingPath).GetAwaiter().GetResult();
        Require(Math.Abs(duration.TotalSeconds - recordingFrames / (double)config.FrameRate) < 1.25,
            $"Paused recording duration includes the pause or lost active footage: {duration}.");
        foreach (string clip in new[] { first, second })
            Require(ffmpeg.ReadDurationAsync(clip).GetAwaiter().GetResult().TotalSeconds > 0,
                "Saved clip has no playable timeline.");
        Require(Handle(engine, "_output") == 0 && Handle(engine, "_recordingOutput") == 0,
            "Disposal retained an output.");
    }

    private static nint Handle(ObsReplayEngine engine, string name) =>
        (nint)typeof(ObsReplayEngine).GetField(name, BindingFlags.NonPublic | BindingFlags.Instance)!.GetValue(engine)!;

    private static void IndependentOutputs(string directory)
    {
        var config = new Config
        {
            CaptureSource = "desktop", Codec = "h264", RecordingResolution = "720p",
            FrameRate = 30, BufferSeconds = 15, CaptureSystemAudio = false,
            CaptureMicrophone = false, OutputDirectory = directory,
        };
        using var engine = new ObsReplayEngine(config);
        string? fault = null;
        engine.Faulted += reason => fault = reason;
        engine.Start();
        Thread.Sleep(2200);
        nint replay = Handle(engine, "_output");
        nint encoder = Handle(engine, "_videoEncoder");
        int frames = engine.EncodedFrameCount;
        engine.StartRecording();
        Require(Handle(engine, "_output") == replay && Handle(engine, "_videoEncoder") == encoder &&
            engine.EncodedFrameCount >= frames && engine.AvailableReplaySeconds > 0,
            "Starting recording cleared the existing replay buffer or replaced its encoder.");
        nint recording = Handle(engine, "_recordingOutput");
        string path = engine.RecordingPath;
        engine.SetReplayBufferEnabled(false);
        Require(engine.IsActive && engine.IsContinuousRecording && engine.AvailableReplaySeconds == 0 &&
            Handle(engine, "_output") == 0 && Handle(engine, "_recordingOutput") == recording,
            "Disabling Instant Clip interrupted recording or retained its buffer.");
        bool rejected = false;
        try { engine.BeginSaveReplay(); }
        catch (InvalidOperationException) { rejected = true; }
        Require(rejected, "Disabled Instant Clip still accepts saves.");
        engine.SetRecordingPaused(true);
        Require(engine.IsHealthy, "Recording-only pause was treated as a fault.");
        engine.SetRecordingPaused(false);
        engine.SetReplayBufferEnabled(true);
        Thread.Sleep(2200);
        Require(engine.IsActive && engine.AvailableReplaySeconds > 0 &&
            Handle(engine, "_recordingOutput") == recording && Handle(engine, "_videoEncoder") == encoder,
            "Enabling Instant Clip replaced recording or its encoder.");
        string clip = engine.SaveReplayAsync().GetAwaiter().GetResult();
        Require(File.Exists(clip), "Concurrent clip saving failed after enabling Instant Clip.");
        replay = Handle(engine, "_output");
        engine.SetRecordingPaused(true);
        engine.StopRecording();
        Thread.Sleep(500);
        Require(engine.IsActive && !engine.IsContinuousRecording && !engine.IsRecordingPaused &&
            Handle(engine, "_output") == replay && Handle(engine, "_videoEncoder") == encoder,
            "Stopping paused recording interrupted Instant Clip.");
        Require(File.Exists(path) && new FileInfo(path).Length > 0 && fault is null,
            $"Independent recording stop lost its file or faulted: {fault}");
    }

    private static void UnexpectedRecordingStop(string directory)
    {
        var config = new Config
        {
            CaptureMode = "recording", CaptureSource = "desktop", Codec = "h264",
            RecordingResolution = "720p", FrameRate = 30,
            CaptureSystemAudio = false, CaptureMicrophone = false, OutputDirectory = directory,
        };
        using var engine = new ObsReplayEngine(config);
        using var stopped = new ManualResetEventSlim();
        string? fault = null;
        engine.Faulted += message => { fault = message; stopped.Set(); };
        engine.Start();
        Thread.Sleep(1000);
        obs_output_force_stop(Handle(engine, "_recordingOutput"));
        Require(stopped.Wait(TimeSpan.FromSeconds(3)) && !string.IsNullOrWhiteSpace(fault),
            "Recording output stop did not report a capture fault.");
        Require(!engine.IsHealthy, "Health check ignored a stopped recording output.");
        Require(obs_output_active(Handle(engine, "_output")), "Recording output stop also stopped replay unexpectedly.");
        engine.Dispose();
        Require(!string.IsNullOrWhiteSpace(engine.RecordingStopError),
            "Disposal discarded the recording failure and could report a clean save.");
    }

    private static void Require(bool condition, string message)
    {
        if (!condition)
            throw new InvalidOperationException(message);
    }

    [DllImport(ObsNative.Library, CallingConvention = CallingConvention.Cdecl)]
    private static extern nint obs_output_get_video_encoder(nint output);

    [DllImport(ObsNative.Library, CallingConvention = CallingConvention.Cdecl)]
    private static extern nint obs_output_get_audio_encoder(nint output, nuint index);

    [DllImport(ObsNative.Library, CallingConvention = CallingConvention.Cdecl)]
    private static extern uint obs_source_get_audio_mixers(nint source);

    [DllImport(ObsNative.Library, CallingConvention = CallingConvention.Cdecl)]
    private static extern int obs_output_get_total_frames(nint output);

    [DllImport(ObsNative.Library, CallingConvention = CallingConvention.Cdecl)]
    private static extern void obs_output_force_stop(nint output);

    [DllImport(ObsNative.Library, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool obs_output_active(nint output);
}
