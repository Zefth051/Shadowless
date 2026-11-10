using System.IO;
using System.Text.Json.Serialization;

namespace Shadowless;

public sealed class CaptureOverlay
{
    public string Id { get; set; } = Guid.NewGuid().ToString("N");
    public string FilePath { get; set; } = "";
    public bool Enabled { get; set; } = true;
    public double X { get; set; } = 0.7;
    public double Y { get; set; } = 0.7;
    public double Width { get; set; } = 0.25;
    public double Height { get; set; } = 0.25;
    public int Opacity { get; set; } = 100;

    [JsonIgnore] public string Name => Path.GetFileName(FilePath);
    [JsonIgnore] public bool IsImage => Path.GetExtension(FilePath).ToLowerInvariant()
        is ".png" or ".jpg" or ".jpeg" or ".bmp" or ".webp";

    public CaptureOverlay Clone() => (CaptureOverlay)MemberwiseClone();

    internal void SetPosition(double x, double y)
    {
        X = Finite(x, 0, 0, 1 - Width);
        Y = Finite(y, 0, 0, 1 - Height);
    }

    public static bool Supports(string path) => Path.GetExtension(path).ToLowerInvariant()
        is ".png" or ".jpg" or ".jpeg" or ".bmp" or ".webp" or ".gif"
        or ".mp4" or ".webm" or ".mov" or ".mkv";

    public static List<CaptureOverlay> Normalize(IEnumerable<CaptureOverlay>? layers)
    {
        var result = new List<CaptureOverlay>();
        var ids = new HashSet<string>(StringComparer.Ordinal);
        foreach (CaptureOverlay? layer in layers ?? [])
        {
            if (layer is null || string.IsNullOrWhiteSpace(layer.FilePath))
                continue;
            try
            {
                if (!Path.IsPathFullyQualified(layer.FilePath) || !Supports(layer.FilePath))
                    continue;
                var copy = layer.Clone();
                copy.FilePath = Path.GetFullPath(copy.FilePath);
                if (string.IsNullOrWhiteSpace(copy.Id) || !ids.Add(copy.Id))
                {
                    copy.Id = Guid.NewGuid().ToString("N");
                    ids.Add(copy.Id);
                }
                copy.Width = Finite(copy.Width, 0.25, 0.01, 1);
                copy.Height = Finite(copy.Height, 0.25, 0.01, 1);
                copy.SetPosition(copy.X, copy.Y);
                copy.Opacity = Math.Clamp(copy.Opacity, 0, 100);
                result.Add(copy);
                if (result.Count == 16)
                    break;
            }
            catch (ArgumentException) { }
            catch (NotSupportedException) { }
            catch (PathTooLongException) { }
        }
        return result;
    }

    private static double Finite(double value, double fallback, double min, double max) =>
        double.IsFinite(value) ? Math.Clamp(value, min, max) : fallback;
}
