using Shadowless;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;

namespace ProcessAudioFoundationQa;

internal static class RecordingControlsQa
{
    internal static void Run()
    {
        Application.ResourceAssembly = typeof(Config).Assembly;
        var app = new Application();
        foreach (string dictionary in new[] { "Themes/Theme.xaml", "Languages/Strings.en.xaml" })
            app.Resources.MergedDictionaries.Add(new ResourceDictionary
            {
                Source = new Uri($"pack://application:,,,/Shadowless;component/{dictionary}"),
            });
        var config = new Config { CaptureMode = "recording", OutputDirectory = System.IO.Path.GetTempPath() };
        int saves = 0, toggles = 0, pauses = 0;
        var window = new SettingsWindow(config, true, () => saves++,
            enabled => { toggles++; return Task.FromResult(enabled); },
            (_, _, _, _) => Task.FromResult(false), (_, _) => Task.FromResult(false),
            (_, _) => Task.FromResult(false), EncoderCapabilities.Preview(),
            AdvancedProcessAudioAvailability.Available,
            (_, _) => Task.FromResult<UpdateRelease?>(null), (_, _, _) => Task.CompletedTask);
        window.SetRecordingPaused = paused => { pauses++; return Task.FromResult(paused); };
        T Control<T>(string name) => (T)window.FindName(name);
        void Click(string name) => Control<ButtonBase>(name).RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
        try
        {
            window.UpdateRuntimeState(true, availableReplaySeconds: 5, recordingPaused: false);
            Require(Control<Button>("SaveReplayButton").IsEnabled, "Recording mode disabled clip saving.");
            Click("SaveReplayButton");
            Require(saves == 1 && toggles == 0, "Saving a recording clip toggled the continuous recording.");
            Click("PauseRecordingButton");
            Require(pauses == 1 && toggles == 0, "Pause rebuilt the capture pipeline.");
            Require(!Control<Button>("SaveReplayButton").IsEnabled, "Paused capture still offers clip saving.");
            Require(Control<TextBlock>("StatusTitleText").Text == Localization.Text("L.Status.RecordingPaused"),
                "Dashboard did not identify the paused recording.");
            Require(Control<Button>("PauseRecordingButton").Content?.ToString() == Localization.Text("L.Capture.ResumeRecording"),
                "Pause button did not become Resume.");
            Click("PauseRecordingButton");
            Require(pauses == 2 && Control<Button>("SaveReplayButton").IsEnabled, "Resume did not restore clip saving.");
            Click("SettingsButton");
            Require(Control<FrameworkElement>("ReplayOnlySettings").Visibility == Visibility.Visible,
                "Recording mode hides its replay-buffer settings.");
            Require(Control<FrameworkElement>("ReplayToggleHotkeySettings").Visibility == Visibility.Visible,
                "Recording mode hides the separate capture-toggle hotkey.");
            Require(Control<TextBlock>("PrimaryHotkeyLabel").Text == Localization.Text("L.Hotkey.SaveReplay") &&
                Control<TextBlock>("ToggleHotkeyLabel").Text == Localization.Text("L.Hotkey.ToggleRecording"),
                "Recording hotkey labels confuse clip saving and capture start/stop.");
            Click("CancelSettingsButton");
            window.UpdateRuntimeState(false);
            Require(!Control<Button>("PauseRecordingButton").IsEnabled && !Control<Button>("SaveReplayButton").IsEnabled,
                "Inactive capture offers pause or save.");
            config.CaptureMode = "replay";
            window.UpdateRuntimeState(true, availableReplaySeconds: 5);
            Require(Control<Button>("PauseRecordingButton").Visibility == Visibility.Collapsed,
                "Replay mode exposes continuous recording pause.");
        }
        finally
        {
            window.Close();
        }
    }

    private static void Require(bool condition, string message)
    {
        if (!condition)
            throw new InvalidOperationException(message);
    }
}
