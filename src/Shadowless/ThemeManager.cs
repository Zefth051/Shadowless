using System.IO;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace Shadowless;

internal static class ThemeManager
{
    private sealed record Palette(
        Color Accent,
        Color Hover,
        Color Dim,
        Color OnAccent);

    private static readonly IReadOnlyDictionary<string, Palette> Palettes =
        new Dictionary<string, Palette>(StringComparer.OrdinalIgnoreCase)
        {
            ["mint"] = New("#63E0BD", "#7DE8CB", "#2EA184", "#0B201A"),
            ["blue"] = New("#64B5F6", "#82C4FA", "#357EBD", "#081A28"),
            ["violet"] = New("#B69AF7", "#C8B2FA", "#785AC6", "#160F2B"),
            ["rose"] = New("#FF6666", "#FF8585", "#C54242", "#270B0B"),
            ["amber"] = New("#F5B94C", "#F8C96E", "#B97D1E", "#251702"),
        };

    private static readonly Dictionary<string, string> IconAssets =
        new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
        {
            ["mint"] = "Shadowless.ico",
            ["blue"] = "ShadowlessBlue.ico",
            ["violet"] = "ShadowlessViolet.ico",
            ["rose"] = "ShadowlessRose.ico",
            ["amber"] = "ShadowlessAmber.ico",
        };

    internal static void ApplyAccent(string? name)
    {
        if (Application.Current is null)
            return;
        bool custom = Config.TryNormalizeAccentColor(name, out string accent) && accent.StartsWith('#');
        if (!Palettes.TryGetValue(name ?? "", out Palette? palette))
            palette = custom
                ? CustomPalette(Parse(accent))
                : Palettes["violet"];

        ResourceDictionary resources = Application.Current.Resources;
        resources["AccentBrush"] = Brush(palette.Accent);
        resources["AccentHoverBrush"] = Brush(palette.Hover);
        resources["AccentReadableBrush"] = Brush(custom ? ReadableAccent(palette.Accent) : palette.Accent);
        resources["AccentReadableHoverBrush"] = Brush(custom ? ReadableAccent(palette.Hover) : palette.Hover);
        resources["AccentDimBrush"] = Brush(custom ? ReadableAccent(palette.Dim) : palette.Dim);
        resources["OnAccentBrush"] = Brush(palette.OnAccent);
        resources["AccentSubtleBrush"] = Brush(WithAlpha(palette.Accent, 0x24));
        resources["AccentChipBgBrush"] = Brush(WithAlpha(palette.Accent, 0x12));
        resources["AccentChipBorderBrush"] = Brush(WithAlpha(palette.Accent, 0x40));
        resources["ApplicationIcon"] = LoadIcon(name);
    }

    internal static string IconAssetName(string? name) =>
        IconAssets.TryGetValue(name ?? "", out string? assetName)
            ? assetName
            : IconAssets["violet"];

    private static BitmapFrame LoadIcon(string? name)
    {
        using Stream stream = Application.GetResourceStream(
            new Uri($"Assets/{IconAssetName(name)}", UriKind.Relative)).Stream;
        var decoder = new IconBitmapDecoder(
            stream,
            BitmapCreateOptions.PreservePixelFormat,
            BitmapCacheOption.OnLoad);
        BitmapFrame frame = decoder.Frames
            .OrderByDescending(candidate => candidate.PixelWidth)
            .First();
        frame.Freeze();
        return frame;
    }

    private static Palette CustomPalette(Color accent)
    {
        bool darkForeground = RelativeLuminance(accent) > 0.1791287847;
        // Move hover away from the foreground to preserve its contrast.
        Color hoverTarget = darkForeground ? Colors.White : Colors.Black;
        return new(accent, Blend(accent, hoverTarget, 0.18),
            Blend(accent, Colors.Black, 0.32), darkForeground ? Colors.Black : Colors.White);
    }

    private static Color ReadableAccent(Color accent)
    {
        // The brightest shared dark surface also covers controls and hover panels.
        double background = RelativeLuminance(Color.FromRgb(43, 50, 54));
        for (int step = 0; step <= 20; step++)
        {
            Color candidate = Blend(accent, Colors.White, step / 20d);
            if ((RelativeLuminance(candidate) + 0.05) / (background + 0.05) >= 4.5)
                return candidate;
        }
        return Colors.White;
    }

    private static double RelativeLuminance(Color color)
    {
        static double Linear(byte value) => value <= 10
            ? value / 3294.6
            : Math.Pow((value / 255d + 0.055) / 1.055, 2.4);
        return 0.2126 * Linear(color.R) + 0.7152 * Linear(color.G) + 0.0722 * Linear(color.B);
    }

    private static Color Blend(Color color, Color target, double amount) => Color.FromRgb(
        (byte)Math.Round(color.R + (target.R - color.R) * amount),
        (byte)Math.Round(color.G + (target.G - color.G) * amount),
        (byte)Math.Round(color.B + (target.B - color.B) * amount));

    private static Palette New(string accent, string hover, string dim, string onAccent) =>
        new(Parse(accent), Parse(hover), Parse(dim), Parse(onAccent));

    private static Color Parse(string value) =>
        (Color)ColorConverter.ConvertFromString(value);

    private static Color WithAlpha(Color color, byte alpha) =>
        Color.FromArgb(alpha, color.R, color.G, color.B);

    private static SolidColorBrush Brush(Color color) => new(color);
}
