using Shadowless;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;

namespace ProcessAudioFoundationQa;

internal static class DashboardAudioLevelsQa
{
    internal static int Run(bool preview)
    {
        var app = new Application();
        app.Resources.MergedDictionaries.Add(new ResourceDictionary
        {
            Source = new Uri("pack://application:,,,/Shadowless;component/Themes/Theme.xaml"),
        });
        app.Resources.MergedDictionaries.Add(new ResourceDictionary
        {
            Source = new Uri("pack://application:,,,/Shadowless;component/Languages/Strings.es.xaml"),
        });
        app.Resources["ApplicationIcon"] = System.Windows.Media.Imaging.BitmapFrame.Create(
            new Uri("pack://application:,,,/Shadowless;component/Assets/Shadowless.ico"));
        var config = new Config
        {
            CaptureMicrophone = true,
            SystemAudioVolume = 65,
            MicrophoneVolume = 80,
            OutputDirectory = Path.Combine(AppContext.BaseDirectory, "empty-library"),
        };
        int rebuilds = 0;
        var window = new SettingsWindow(config, false, () => { },
            _ => Task.FromResult(false), (_, _, _, _) => { rebuilds++; return Task.FromResult(false); },
            (_, _) => { rebuilds++; return Task.FromResult(false); }, (_, _) => Task.FromResult(false),
            EncoderCapabilities.Preview(), AdvancedProcessAudioAvailability.Available,
            (_, _) => Task.FromResult<UpdateRelease?>(null), (_, _, _) => Task.CompletedTask);
        int applies = 0;
        window.SetAudioLevels = async (system, microphone, systemMuted, microphoneMuted) =>
        {
            await Task.Delay(40);
            config.SystemAudioVolume = system;
            config.MicrophoneVolume = microphone;
            config.SystemAudioMuted = systemMuted;
            config.MicrophoneMuted = microphoneMuted;
            applies++;
            return true;
        };
        T Control<T>(string name) => (T)window.FindName(name);
        void Click(string name) => Control<ButtonBase>(name).RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
        async Task Settled()
        {
            await Task.Delay(300);
            Require(!((bool)typeof(SettingsWindow).GetField("_applyingAudioLevels",
                System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic)!.GetValue(window)!),
                "Audio level updates did not settle.");
        }
        int result = 0;
        window.Loaded += async (_, _) =>
        {
            try
            {
                if (preview)
                    return;
                await Task.Delay(250);
                SaveVisual(window, "audio-dashboard.png");
                Click("SystemVolumeButton");
                Require(Control<Popup>("DashboardVolumePopup").IsOpen, "Volume button did not open the slider.");
                await Task.Delay(200);
                SaveVisual((FrameworkElement)Control<Popup>("DashboardVolumePopup").Child, "audio-volume.png");
                Control<Slider>("DashboardVolumeSlider").Value = 42;
                await Task.Delay(90);
                Control<Slider>("DashboardVolumeSlider").Value = 37;
                await Settled();
                Require(config.SystemAudioVolume == 37 && config.MicrophoneVolume == 80,
                    $"Slider lost the final value or changed the other source: system={config.SystemAudioVolume}, microphone={config.MicrophoneVolume}, popup={Control<Popup>("DashboardVolumePopup").IsOpen}.");
                Control<Popup>("DashboardVolumePopup").IsOpen = false;
                Control<ToggleButton>("SystemSourceChip").IsChecked = false;
                Click("SystemSourceChip");
                await Settled();
                Require(config.SystemAudioMuted && config.SystemAudioVolume == 37 && config.CaptureSystemAudio,
                    "Mute disabled the source or lost the selected volume.");
                SaveVisual(window, "audio-muted.png");
                Control<ToggleButton>("SystemSourceChip").IsChecked = true;
                Click("SystemSourceChip");
                await Settled();
                Require(!config.SystemAudioMuted && config.SystemAudioVolume == 37, "Unmute lost the selected volume.");
                Click("MicrophoneVolumeButton");
                Control<Slider>("DashboardVolumeSlider").Value = 53;
                Control<Popup>("DashboardVolumePopup").IsOpen = false;
                await Settled();
                Require(config.MicrophoneVolume == 53 && config.SystemAudioVolume == 37,
                    "Closing the popup discarded the last microphone volume.");
                config.AudioRoutingMode = "advanced";
                window.UpdateRuntimeState(false);
                Require(Control<Button>("MicrophoneVolumeButton").IsVisible,
                    "Advanced mode hid the microphone volume control.");
                Control<ToggleButton>("MicSourceChip").IsChecked = false;
                Click("MicSourceChip");
                await Settled();
                Require(config.MicrophoneMuted && config.CaptureMicrophone && config.MicrophoneVolume == 53,
                    "Advanced microphone mute changed the capture topology.");
                Require(rebuilds == 0 && applies >= 4, "Audio level controls rebuilt the capture pipeline.");
                window.Width = 1280;
                window.Height = 720;
                window.UpdateLayout();
                SaveVisual(window, "audio-dashboard-small.png");
                Console.WriteLine("DASHBOARD_AUDIO_LEVELS PASS: slider, coalescing, mute, restore, popup close, advanced microphone");
            }
            catch (Exception exception)
            {
                Console.Error.WriteLine(exception);
                result = 1;
            }
            finally
            {
                if (!preview)
                    window.Close();
            }
        };
        app.Run(window);
        return result;
    }

    private static void Require(bool condition, string message)
    {
        if (!condition)
            throw new InvalidOperationException(message);
    }

    private static void SaveVisual(FrameworkElement visual, string filename)
    {
        visual.UpdateLayout();
        var bitmap = new System.Windows.Media.Imaging.RenderTargetBitmap(
            (int)Math.Ceiling(visual.ActualWidth), (int)Math.Ceiling(visual.ActualHeight),
            96, 96, System.Windows.Media.PixelFormats.Pbgra32);
        bitmap.Render(visual);
        var encoder = new System.Windows.Media.Imaging.PngBitmapEncoder();
        encoder.Frames.Add(System.Windows.Media.Imaging.BitmapFrame.Create(bitmap));
        using var stream = File.Create(Path.Combine(AppContext.BaseDirectory, filename));
        encoder.Save(stream);
    }
}
