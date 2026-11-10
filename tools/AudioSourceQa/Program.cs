using Shadowless;

int passed = 0;
Check(new Config().GpuPreference == "high-performance",
    "New configurations must prefer discrete graphics");
Check(System.Text.Json.JsonSerializer.Deserialize<Config>("{}")!.GpuPreference == "high-performance",
    "Existing configurations without a GPU setting must prefer discrete graphics");
foreach (string mode in new[] { "replay", "recording" })
{
    var before = new Config { CaptureMode = mode };
    var integrated = before.Clone();
    integrated.GpuPreference = "power-saving";
    integrated.Normalize();
    Check(!before.PipelineEquals(integrated) && !integrated.PipelineEquals(before),
        $"{mode}: selecting integrated graphics must rebuild the pipeline");
    Check(integrated.Clone().GpuPreference == "power-saving",
        "Cloning settings must preserve integrated graphics");
    var restored = System.Text.Json.JsonSerializer.Deserialize<Config>(
        System.Text.Json.JsonSerializer.Serialize(integrated))!;
    restored.Normalize();
    Check(restored.GpuPreference == "power-saving" && restored.PipelineEquals(integrated),
        "Saving and restoring settings must preserve the GPU preference");
}
foreach (string? preference in new string?[] { null, "", "missing-adapter" })
{
    var invalid = new Config { GpuPreference = preference! };
    invalid.Normalize();
    Check(invalid.GpuPreference == "high-performance",
        "Invalid GPU preferences must fall back to discrete graphics");
}
foreach (string mode in new[] { "replay", "recording" })
{
    foreach (bool system in new[] { false, true })
    foreach (bool microphone in new[] { false, true })
    {
        var before = new Config { CaptureMode = mode };
        Config after = before.Clone();
        after.CaptureSystemAudio = system;
        after.CaptureMicrophone = microphone;
        after.SystemAudioDeviceId = "speakers";
        after.MicrophoneDeviceId = "microphone";
        Check(before.PipelineEquals(after) && after.PipelineEquals(before),
            $"{mode}: mixed desktop audio changes must preserve the pipeline");
    }
}

foreach (string topology in new[] { "separate", "advanced", "game" })
{
    var before = new Config
    {
        SeparateAudioTracks = topology == "separate",
        AudioRoutingMode = topology == "advanced" ? "advanced" : "simple",
        CaptureSource = topology == "game" ? "game" : "desktop",
    };
    Config after = before.Clone();
    after.CaptureSystemAudio = false;
    Check(!before.PipelineEquals(after) && !after.PipelineEquals(before),
        $"{topology}: audio topology changes must still rebuild the pipeline");
}

foreach (Action<Config> change in new Action<Config>[]
{
    c => c.FrameRate = 120,
    c => c.Codec = "hevc",
    c => c.AudioCodec = "opus",
    c => c.SeparateAudioTracks = true,
    c => c.CaptureSource = "game",
})
{
    var before = new Config();
    Config after = before.Clone();
    change(after);
    Check(!before.PipelineEquals(after), "encoder/routing changes must still rebuild");
}
Console.WriteLine($"AUDIO_SOURCE_CONFIG PASS: {passed} cases");

void Check(bool condition, string message)
{
    if (!condition)
        throw new InvalidOperationException(message);
    passed++;
}

// File persistence and WPF localization are outside these pipeline classification tests.
namespace Shadowless
{
    internal static class AppDataPaths
    {
        internal static string ConfigFile => throw new NotSupportedException();
    }
    internal static class Localization
    {
        internal static string ResolveInitialLanguage(string? language) => language ?? "en";
        internal static string NormalizeLanguage(string? language) => language ?? "en";
    }
    internal static class HotkeyManager
    {
        internal static bool IsValid(string value) => true;
        internal static bool AreDistinct(string left, string right) => left != right;
    }
    internal static class Log
    {
        internal static void Write(string message) => throw new NotSupportedException(message);
    }
}
