using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace Shadowless;

public partial class SettingsWindow
{
    internal Func<int, int, bool, bool, Task<bool>>? SetAudioLevels { get; set; }

    private int _dashboardSystemVolume;
    private int _dashboardMicrophoneVolume;
    private bool _dashboardSystemMuted;
    private bool _dashboardMicrophoneMuted;
    private bool _dashboardVolumeMicrophone;
    private bool _updatingDashboardAudio;
    private bool _audioLevelsPending;
    private bool _applyingAudioLevels;

    private void RefreshDashboardAudioLevels()
    {
        if (!_applyingAudioLevels)
        {
            _dashboardSystemVolume = _config.SystemAudioVolume;
            _dashboardMicrophoneVolume = _config.MicrophoneVolume;
            _dashboardSystemMuted = _config.SystemAudioMuted;
            _dashboardMicrophoneMuted = _config.MicrophoneMuted;
        }
        RenderDashboardAudioLevels();
    }

    private void RenderDashboardAudioLevels()
    {
        SystemSourceChip.IsChecked = _config.CaptureSystemAudio && !_dashboardSystemMuted;
        MicSourceChip.IsChecked = _config.CaptureMicrophone && !_dashboardMicrophoneMuted;
        SystemMuteSlash.Visibility = SystemSourceChip.IsChecked == true ? Visibility.Collapsed : Visibility.Visible;
        MicrophoneMuteSlash.Visibility = MicSourceChip.IsChecked == true ? Visibility.Collapsed : Visibility.Visible;
        SystemSourceChip.ToolTip = Localization.Text(!_config.CaptureSystemAudio
            ? "L.Audio.EnableSource" : _dashboardSystemMuted ? "L.Audio.Unmute" : "L.Audio.Mute");
        MicSourceChip.ToolTip = Localization.Text(!_config.CaptureMicrophone
            ? "L.Audio.EnableSource" : _dashboardMicrophoneMuted ? "L.Audio.Unmute" : "L.Audio.Mute");
        SystemVolumeText.Text = $"{_dashboardSystemVolume}% ▾";
        MicrophoneVolumeText.Text = $"{_dashboardMicrophoneVolume}% ▾";

        if (!DashboardVolumePopup.IsOpen)
            return;
        int volume = _dashboardVolumeMicrophone ? _dashboardMicrophoneVolume : _dashboardSystemVolume;
        bool muted = _dashboardVolumeMicrophone ? _dashboardMicrophoneMuted : _dashboardSystemMuted;
        bool captured = _dashboardVolumeMicrophone ? _config.CaptureMicrophone : _config.CaptureSystemAudio;
        DashboardVolumeTitle.Text = Localization.Text(_dashboardVolumeMicrophone
            ? "L.Audio.Microphone" : _config.CaptureSource == "game" ? "L.Audio.GameAudio" : "L.Audio.SystemAudio");
        DashboardVolumePercent.Text = $"{volume}%";
        DashboardVolumeStatus.Text = Localization.Text(!captured
            ? "L.Audio.EnableSource" : muted || volume == 0 ? "L.Audio.Muted" : "L.Audio.LiveVolume");
        _updatingDashboardAudio = true;
        DashboardVolumeSlider.Value = volume;
        _updatingDashboardAudio = false;
    }

    private void DashboardVolume_Click(object sender, RoutedEventArgs e)
    {
        var button = (Button)sender;
        bool close = DashboardVolumePopup.IsOpen && ReferenceEquals(DashboardVolumePopup.PlacementTarget, button);
        DashboardVolumePopup.IsOpen = false;
        if (close)
            return;
        _dashboardVolumeMicrophone = button.Tag?.ToString() == "microphone";
        DashboardVolumePopup.PlacementTarget = button;
        DashboardVolumePopup.IsOpen = true;
        RenderDashboardAudioLevels();
        DashboardVolumeSlider.Focus();
    }

    private async void DashboardVolume_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        if (_updatingDashboardAudio || !DashboardVolumePopup.IsOpen)
            return;
        int volume = (int)Math.Round(e.NewValue);
        if (_dashboardVolumeMicrophone)
            _dashboardMicrophoneVolume = volume;
        else
            _dashboardSystemVolume = volume;
        RenderDashboardAudioLevels();
        await QueueDashboardAudioLevelsAsync();
    }

    private void DashboardVolumePopup_PreviewKeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key != Key.Escape)
            return;
        DashboardVolumePopup.IsOpen = false;
        (DashboardVolumePopup.PlacementTarget as UIElement)?.Focus();
        e.Handled = true;
    }

    private async Task QueueDashboardAudioLevelsAsync()
    {
        if (SetAudioLevels is null)
            return;
        _audioLevelsPending = true;
        if (_applyingAudioLevels)
            return;
        _applyingAudioLevels = true;
        try
        {
            while (_audioLevelsPending)
            {
                // Coalesce slider input while keeping the last value, including when the overlay closes.
                await Task.Delay(80);
                _audioLevelsPending = false;
                bool applied = await SetAudioLevels(_dashboardSystemVolume, _dashboardMicrophoneVolume,
                    _dashboardSystemMuted, _dashboardMicrophoneMuted);
                if (!applied)
                {
                    _audioLevelsPending = false;
                    ShowError(Localization.Text("L.Error.SourceTitle"), Localization.Text("L.Error.AudioSourceMessage"));
                    break;
                }
            }
        }
        catch (Exception exception)
        {
            _audioLevelsPending = false;
            HandleUiActionError("Dashboard audio levels", exception);
        }
        finally
        {
            _applyingAudioLevels = false;
            RefreshDashboardAudioLevels();
            _updatingUi = true;
            SystemVolumeSlider.Value = _config.SystemAudioVolume;
            MicVolumeSlider.Value = _config.MicrophoneVolume;
            _updatingUi = false;
        }
    }
}
