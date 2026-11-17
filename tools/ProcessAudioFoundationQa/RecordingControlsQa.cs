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
        int saves = 0, toggles = 0, pauses = 0, recordings = 0;
        var window = new SettingsWindow(config, true, () => saves++,
            enabled => { toggles++; config.InstantClipEnabled = enabled; return Task.FromResult(true); },
            (_, _, _, _) => Task.FromResult(false), (_, _) => Task.FromResult(false),
            (_, _) => Task.FromResult(false), EncoderCapabilities.Preview(),
            AdvancedProcessAudioAvailability.Available,
            (_, _) => Task.FromResult<UpdateRelease?>(null), (_, _, _) => Task.CompletedTask);
        window.SetRecordingEnabled = enabled =>
        {
            recordings++;
            config.CaptureMode = enabled ? "recording" : "replay";
            config.ReplayEnabled = true;
            return Task.FromResult(enabled);
        };
        window.SetRecordingPaused = paused => { pauses++; return Task.FromResult(paused); };
        T Control<T>(string name) => (T)window.FindName(name);
        void Click(string name)
        {
            if (Control<Control>(name) is MenuItem item)
                item.RaiseEvent(new RoutedEventArgs(MenuItem.ClickEvent));
            else
                Control<ButtonBase>(name).RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
        }
        try
        {
            window.UpdateRuntimeState(true, availableReplaySeconds: 5, recordingPaused: false);
            Require(Control<Button>("SaveReplayButton").IsEnabled, "Recording mode disabled clip saving.");
            Click("DashboardRecordingModeButton");
            Require(Control<Button>("DashboardRecordingModeButton").ContextMenu.IsOpen && recordings == 0,
                "Recording tile must open a menu without changing recording.");
            Control<Button>("DashboardRecordingModeButton").ContextMenu.IsOpen = false;
            Click("DashboardReplayModeButton");
            Require(Control<Button>("DashboardReplayModeButton").ContextMenu.IsOpen && toggles == 0,
                "Instant Clip tile must open a menu without toggling capture.");
            Control<Button>("DashboardReplayModeButton").ContextMenu.IsOpen = false;
            Click("SaveReplayMenuItem");
            Require(saves == 1, "Instant Clip menu did not save a replay.");
            Click("SaveReplayButton");
            Require(saves == 2 && toggles == 0, "Saving a recording clip toggled the continuous recording.");
            Click("PauseRecordingMenuItem");
            Require(pauses == 1 && toggles == 0, "Pause rebuilt the capture pipeline.");
            Require(!Control<Button>("SaveReplayButton").IsEnabled, "Paused capture still offers clip saving.");
            Require(Control<TextBlock>("StatusTitleText").Text == Localization.Text("L.Status.RecordingPaused"),
                "Dashboard did not identify the paused recording.");
            Require(Control<MenuItem>("ResumeRecordingMenuItem").IsEnabled && !Control<MenuItem>("PauseRecordingMenuItem").IsEnabled,
                "Paused recording did not offer Resume exclusively.");
            Click("ResumeRecordingMenuItem");
            Require(pauses == 2 && Control<Button>("SaveReplayButton").IsEnabled, "Resume did not restore clip saving.");
            Click("DisableInstantClipMenuItem");
            Require(toggles == 1 && !Control<Button>("SaveReplayButton").IsEnabled &&
                Control<MenuItem>("PauseRecordingMenuItem").IsEnabled,
                "Disabling Instant Clip stopped recording or retained clip saving.");
            Click("EnableInstantClipMenuItem");
            Require(toggles == 2 && Control<Button>("SaveReplayButton").IsEnabled,
                "Enabling Instant Clip did not restore saving during recording.");
            Click("StopRecordingMenuItem");
            Require(recordings == 1 && toggles == 2 && Control<Button>("SaveReplayButton").IsEnabled,
                "Stopping recording stopped Instant Clip.");
            Click("StartRecordingMenuItem");
            Require(recordings == 2 && toggles == 2, "Starting recording toggled Instant Clip.");
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
            Require(!Control<MenuItem>("PauseRecordingMenuItem").IsEnabled && !Control<Button>("SaveReplayButton").IsEnabled,
                "Inactive capture offers pause or save.");
            config.CaptureMode = "replay";
            window.UpdateRuntimeState(true, availableReplaySeconds: 5);
            Require(!Control<MenuItem>("PauseRecordingMenuItem").IsEnabled,
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
