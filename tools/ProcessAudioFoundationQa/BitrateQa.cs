using Shadowless;
using System.Text.Json;

namespace ProcessAudioFoundationQa;

internal static class BitrateQa
{
    internal static void Run()
    {
        for (int value = 0; value <= 100; value++)
        {
            var config = new Config { BitrateMbps = value };
            config.Normalize();
            Require(config.BitrateMbps == value, $"Bitrate {value} was discarded.");
            var restored = JsonSerializer.Deserialize<Config>(JsonSerializer.Serialize(config))!;
            restored.Normalize();
            Require(restored.BitrateMbps == value, $"Bitrate {value} did not survive serialization.");
            Require(config.Clone().BitrateMbps == value, "Cloning discarded the bitrate.");
        }

        foreach (int value in new[] { -1, int.MinValue, 101, int.MaxValue })
        {
            var config = new Config { BitrateMbps = value };
            config.Normalize();
            Require(config.BitrateMbps == Math.Clamp(value, 0, 100), "Bitrate limit was not enforced.");
        }
    }

    private static void Require(bool condition, string message)
    {
        if (!condition)
            throw new InvalidOperationException(message);
    }
}
