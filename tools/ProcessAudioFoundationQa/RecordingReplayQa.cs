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
            Exercise(codec, directory);
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
        candidate.AccentColor = "#000000";
        Require(config.PipelineEquals(candidate), "Appearance edit restarted recording.");
    }

    private static void Exercise(string audioCodec, string directory)
    {
        var config = new Config
        {
            CaptureMode = "recording", CaptureSource = "desktop", Codec = "h264",
            RecordingResolution = "720p", FrameRate = 30, BufferSeconds = 15,
            CaptureSystemAudio = false, CaptureMicrophone = false,
            AudioCodec = audioCodec, SeparateAudioTracks = true,
            OutputDirectory = directory, OrganizeReplaysByGame = false,
        };
        config.Normalize();
        using var engine = new ObsReplayEngine(config);
        string? fault = null;
        engine.Faulted += message => fault = message;
        engine.Start();
        nint replay = Handle(engine, "_output"), recording = Handle(engine, "_recordingOutput");
        nint videoEncoder = Handle(engine, "_videoEncoder");
        Require(replay != 0 && recording != 0 && replay != recording, "Recording and replay outputs are not independent.");
        Require(obs_output_get_video_encoder(replay) == videoEncoder &&
            obs_output_get_video_encoder(recording) == videoEncoder, "Outputs created separate video encoders.");
        for (nuint track = 0; track < (nuint)ObsReplayEngine.AudioTrackCount(config); track++)
            Require(obs_output_get_audio_encoder(replay, track) != 0 &&
                obs_output_get_audio_encoder(replay, track) == obs_output_get_audio_encoder(recording, track),
                "Outputs did not share every audio encoder.");
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
    private static extern int obs_output_get_total_frames(nint output);

    [DllImport(ObsNative.Library, CallingConvention = CallingConvention.Cdecl)]
    private static extern void obs_output_force_stop(nint output);

    [DllImport(ObsNative.Library, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool obs_output_active(nint output);
}
