using Shadowless;
using System.Reflection;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Media;

namespace ProcessAudioFoundationQa;

internal static class AccentQa
{
    internal static void ConfigValues()
    {
        foreach (var (input, expected) in new[]
        {
            ("#12abEF", "#12ABEF"), (" #000000 ", "#000000"), ("#FFFFFF", "#FFFFFF"),
            ("mint", "mint"), ("BLUE", "blue"), ("violet", "violet"),
            ("rose", "rose"), ("amber", "amber"),
            ("#123", "violet"), ("#12345678", "violet"), ("#GG0000", "violet"),
            ("123456", "violet"), ("", "violet"), ("transparent", "violet"),
        })
        {
            var config = new Config { AccentColor = input };
            config.Normalize();
            Require(config.AccentColor == expected, $"Accent '{input}' normalized to '{config.AccentColor}'.");
            var restored = JsonSerializer.Deserialize<Config>(JsonSerializer.Serialize(config))!;
            restored.Normalize();
            Require(restored.AccentColor == expected, "Accent did not survive persistence.");
            Require(config.Clone().AccentColor == expected, "Cloning discarded the accent.");
            var changed = config.Clone();
            changed.AccentColor = "#123456";
            Require(config.PipelineEquals(changed), "Accent-only edits would restart capture.");
        }
    }

    internal static void PaletteContrast()
    {
        var application = Application.Current ?? new Application();
        foreach (string hex in new[] { "#000000", "#FFFFFF", "#123456", "#808080", "#FF0000", "#00FF00", "#0000FF" })
        {
            ThemeManager.ApplyAccent(hex);
            Color accent = ((SolidColorBrush)application.Resources["AccentBrush"]).Color;
            Color hover = ((SolidColorBrush)application.Resources["AccentHoverBrush"]).Color;
            Color foreground = ((SolidColorBrush)application.Resources["OnAccentBrush"]).Color;
            Color readable = ((SolidColorBrush)application.Resources["AccentReadableBrush"]).Color;
            Color readableHover = ((SolidColorBrush)application.Resources["AccentReadableHoverBrush"]).Color;
            Color dim = ((SolidColorBrush)application.Resources["AccentDimBrush"]).Color;
            Require(accent == (Color)ColorConverter.ConvertFromString(hex), "Custom accent was replaced by a preset.");
            Require(Contrast(accent, foreground) >= 4.5, $"Unreadable foreground for {hex}.");
            Require(Contrast(hover, foreground) >= 4.5, $"Unreadable hover foreground for {hex}.");
            foreach (string panel in new[] { "#141719", "#181D20", "#101416", "#1D2226", "#2B3236" })
            {
                Color background = (Color)ColorConverter.ConvertFromString(panel);
                Require(Contrast(readable, background) >= 4.5, $"Unreadable accent text for {hex} on {panel}.");
                Require(Contrast(readableHover, background) >= 4.5, $"Unreadable accent hover for {hex} on {panel}.");
                Require(Contrast(dim, background) >= 4.5, $"Unreadable accent indicator for {hex} on {panel}.");
            }
            Require(ThemeManager.IconAssetName(hex) == "ShadowlessViolet.ico", "Custom accent lost its icon fallback.");
        }
        foreach (var (name, accentHex, hoverHex, dimHex) in new[]
        {
            ("mint", "#63E0BD", "#7DE8CB", "#2EA184"),
            ("blue", "#64B5F6", "#82C4FA", "#357EBD"),
            ("violet", "#B69AF7", "#C8B2FA", "#785AC6"),
            ("rose", "#FF6666", "#FF8585", "#C54242"),
            ("amber", "#F5B94C", "#F8C96E", "#B97D1E"),
        })
        {
            ThemeManager.ApplyAccent(name);
            foreach (var (key, expected) in new[]
            {
                ("AccentReadableBrush", accentHex), ("AccentReadableHoverBrush", hoverHex), ("AccentDimBrush", dimHex),
            })
                Require(((SolidColorBrush)application.Resources[key]).Color == (Color)ColorConverter.ConvertFromString(expected),
                    $"Preset {name} changed its {key} color.");
        }
    }

    internal static void StagedSettings()
    {
        Application application = Application.Current ?? new Application();
        foreach (string dictionary in new[] { "Themes/Theme.xaml", "Languages/Strings.en.xaml" })
            application.Resources.MergedDictionaries.Add(new ResourceDictionary
            {
                Source = new Uri($"pack://application:,,,/Shadowless;component/{dictionary}"),
            });

        var config = new Config { AccentColor = "violet", OutputDirectory = System.IO.Path.GetTempPath() };
        int applies = 0;
        var window = new SettingsWindow(config, false, () => { },
            _ => Task.FromResult(false), (_, _, _, _) => Task.FromResult(false),
            (_, _) => Task.FromResult(false), (candidate, _) =>
            {
                config.CopyFrom(candidate);
                applies++;
                return Task.FromResult(true);
            }, EncoderCapabilities.Preview(), AdvancedProcessAudioAvailability.Available,
            (_, _) => Task.FromResult<UpdateRelease?>(null), (_, _, _) => Task.CompletedTask);
        const BindingFlags flags = BindingFlags.Instance | BindingFlags.NonPublic;
        void Invoke(string name) => typeof(SettingsWindow).GetMethod(name, flags)!.Invoke(window, null);
        T Control<T>(string name) => (T)window.FindName(name);
        void Click(string name) => Control<ButtonBase>(name).RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
        try
        {
            Click("SettingsButton");
            Control<RadioButton>("CustomAccentRadio").IsChecked = true;
            Control<TextBox>("CustomAccentBox").Text = "#123";
            Invoke("RefreshSettingsDirtyState");
            Require((bool)typeof(SettingsWindow).GetField("_settingsDirty", flags)!.GetValue(window)!,
                "Invalid custom color was not marked as unsaved.");
            Click("DoneButton");
            Require(applies == 0 && config.AccentColor == "violet", "Invalid color reached the settings save.");
            Require(Control<TextBox>("CustomAccentBox").Text == "#123", "Invalid input was discarded.");
            Control<TextBox>("CustomAccentBox").Text = "#12abEF";
            Click("CancelSettingsButton");
            Require(applies == 0 && config.AccentColor == "violet", "Cancel applied the pending accent.");

            Click("SettingsButton");
            Control<RadioButton>("CustomAccentRadio").IsChecked = true;
            Control<TextBox>("CustomAccentBox").Text = "#12abEF";
            Click("DoneButton");
            Require(applies == 1 && config.AccentColor == "#12ABEF", "Done did not apply the validated custom accent.");
            Click("SettingsButton");
            Require(Control<RadioButton>("CustomAccentRadio").IsChecked == true &&
                Control<TextBox>("CustomAccentBox").Text == "#12ABEF", "Reopening settings lost the saved custom accent.");
            Control<RadioButton>("CustomAccentRadio").IsChecked = false;
            var presets = Control<Panel>("AccentColorOptions").Children.OfType<RadioButton>();
            presets.Single(button => Equals(button.Tag, "blue")).IsChecked = true;
            Click("DoneButton");
            Require(applies == 2 && config.AccentColor == "blue", "Preset selection did not replace the custom accent.");
        }
        finally
        {
            Click("CancelSettingsButton");
            window.Close();
        }
    }

    private static double Contrast(Color left, Color right)
    {
        double Luminance(Color color)
        {
            double Linear(byte value) => value <= 10 ? value / 3294.6 : Math.Pow((value / 255d + 0.055) / 1.055, 2.4);
            return 0.2126 * Linear(color.R) + 0.7152 * Linear(color.G) + 0.0722 * Linear(color.B);
        }
        double a = Luminance(left), b = Luminance(right);
        return (Math.Max(a, b) + 0.05) / (Math.Min(a, b) + 0.05);
    }

    private static void Require(bool condition, string message)
    {
        if (!condition)
            throw new InvalidOperationException(message);
    }
}
