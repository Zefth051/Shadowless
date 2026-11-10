using System.Collections;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Windows;
using Shadowless;

internal static class Program
{
    private static readonly BindingFlags Private = BindingFlags.Instance | BindingFlags.NonPublic;

    [STAThread]
    private static int Main(string[] args)
    {
        try
        {
            CheckNormalization();
            var app = new Application { ShutdownMode = ShutdownMode.OnExplicitShutdown };
            app.Resources.MergedDictionaries.Add(new ResourceDictionary
            {
                Source = new Uri("pack://application:,,,/Shadowless;component/Themes/Theme.xaml"),
            });
            app.Resources.MergedDictionaries.Add(new ResourceDictionary
            {
                Source = new Uri("pack://application:,,,/Shadowless;component/Languages/Strings.es.xaml"),
            });
            CheckEditorTransforms();
            CheckDashboardOverlayCard(args.Length > 1 && args[0] == "--review" ? args[1] : null);
            if (args.Length > 0 && args[0] == "--review")
                return 0;
            if (args.Length == 0 || args[0] == "--unit")
                return 0;
            string root = Path.GetFullPath(args[1]);
            Directory.CreateDirectory(root);
            var config = new Config
            {
                CaptureSystemAudio = false, CaptureMicrophone = false,
                FrameRate = 60, RecordingResolution = "1080p", BufferSeconds = 15,
                OutputDirectory = root, DesktopCaptureMethod = "dxgi",
                CaptureMode = args[0] == "--recording" ? "recording" : "replay",
            };
            var layer = new CaptureOverlay { FilePath = Path.Combine(root, "overlay.png"), X = 0.1, Y = 0.1 };
            if (args[0] == "--ui")
            {
                config.CaptureOverlays = [layer];
                config.CaptureOverlaysEnabled = true;
                using var engine = new ObsReplayEngine(config, windowsGraphicsCaptureReady: false);
                engine.Start();
                var window = new CaptureOverlayEditorWindow(config, (enabled, layers) =>
                {
                    engine.ApplyCaptureOverlays(enabled, layers);
                    return Task.FromResult(engine.OverlayWarnings);
                }, (hwnd, width, height) =>
                {
                    typeof(ObsReplayEngine).GetMethod("UpdateOverlayPreview", Private)!
                        .Invoke(engine, [hwnd, width, height]);
                    var canvas = engine.CaptureCanvas;
                    return new CaptureOverlayPreviewState(engine.IsActive, canvas.Width, canvas.Height,
                        engine.TotalRenderedFrames, engine.LaggedRenderedFrames, engine.OverlayWarnings);
                }, engine.MoveCaptureOverlay);
                window.SourceInitialized += (_, _) => SetWindowDisplayAffinity(
                    new System.Windows.Interop.WindowInteropHelper(window).Handle, 0);
                window.Closed += (_, _) => app.Shutdown();
                app.Run(window);
                return 0;
            }

            string recording = "";
            using (var engine = new ObsReplayEngine(config, windowsGraphicsCaptureReady: false))
            {
                engine.Start();
                recording = engine.RecordingPath;
                nint output = (nint)Field(engine, "_output")!;
                nint encoder = (nint)Field(engine, "_videoEncoder")!;
                var results = new List<object>();
                foreach (string phase in new[] { "baseline", "image", "gif", "video", "disabled" })
                {
                    layer.FilePath = Path.Combine(root, phase switch
                    {
                        "gif" => "overlay.gif", "video" => "overlay.mp4", _ => "overlay.png",
                    });
                    bool enabled = phase is not ("baseline" or "disabled");
                    engine.ApplyCaptureOverlays(enabled, [layer]);
                    Require(string.IsNullOrEmpty(engine.OverlayWarnings), engine.OverlayWarnings);
                    Require((nint)Field(engine, "_output")! == output, "Overlay update recreated the output.");
                    Require((nint)Field(engine, "_videoEncoder")! == encoder, "Overlay update recreated the encoder.");
                    Thread.Sleep(1500);
                    if (enabled)
                    {
                        var sources = (IDictionary)Field(engine, "_overlaySources")!;
                        Require(sources.Count == 1, "Expected one active overlay source.");
                        object source = sources.Values.Cast<object>().Single();
                        nint handle = (nint)source.GetType().GetField("Source", Private)!.GetValue(source)!;
                        Require(obs_source_get_width(handle) > 0, $"{phase} did not decode.");
                        Require(obs_source_get_audio_mixers(handle) == 0, "Media audio leaked into the recording.");
                    }
                    else
                    {
                        Require((nint)Field(engine, "_overlayScene")! == 0, "Disabled overlays retained a composition scene.");
                        Require(((IDictionary)Field(engine, "_overlaySources")!).Count == 0, "Disabled media decoder retained.");
                    }
                    uint rendered = engine.TotalRenderedFrames, lagged = engine.LaggedRenderedFrames;
                    TimeSpan cpu = Process.GetCurrentProcess().TotalProcessorTime;
                    var timer = Stopwatch.StartNew();
                    Thread.Sleep(5000);
                    double seconds = timer.Elapsed.TotalSeconds;
                    uint frames = engine.TotalRenderedFrames - rendered;
                    uint lag = engine.LaggedRenderedFrames - lagged;
                    var result = new
                    {
                        Phase = phase, RenderedFps = Math.Round(frames / seconds, 2),
                        RenderLagPercent = Math.Round(100.0 * lag / Math.Max(1, frames), 3),
                        CpuPercent = Math.Round(100 * (Process.GetCurrentProcess().TotalProcessorTime - cpu).TotalSeconds /
                            (seconds * Environment.ProcessorCount), 2),
                    };
                    results.Add(result);
                    Console.WriteLine(JsonSerializer.Serialize(result));
                    if (!engine.IsContinuousRecording)
                    {
                        string saved = engine.SaveReplayAsync().GetAwaiter().GetResult();
                        Require(File.Exists(saved) && new FileInfo(saved).Length > 0, "Replay save failed.");
                        File.Move(saved, Path.Combine(root, $"{phase}.mp4"), overwrite: true);
                    }
                }

                layer.Opacity = 40;
                engine.ApplyCaptureOverlays(true, [layer]);
                Require(string.IsNullOrEmpty(engine.OverlayWarnings), "Opacity filter failed.");
                var activeSources = (IDictionary)Field(engine, "_overlaySources")!;
                object activeSource = activeSources[layer.Id]!;
                nint originalHandle = (nint)Field(activeSource, "Source")!;
                nint item = (nint)Field(activeSource, "Item")!;
                Require((nint)Field(activeSource, "OpacityFilter")! != 0, "Opacity filter was not attached.");
                CheckOpacity(activeSource, 0.4);
                layer.Opacity = 15;
                layer.Width = 0.6;
                layer.Height = 0.2;
                engine.ApplyCaptureOverlays(true, [layer]);
                CheckOpacity(activeSource, 0.15);
                obs_sceneitem_get_draw_transform(item, out Matrix4 transform);
                Require(Math.Abs(transform.X.X * obs_source_get_width(originalHandle) - engine.CaptureCanvas.Width * 0.6) < 1 &&
                    Math.Abs(transform.Y.Y * obs_source_get_height(originalHandle) - engine.CaptureCanvas.Height * 0.2) < 1,
                    "Overlay width and height do not independently resize the rendered media.");
                engine.MoveCaptureOverlay(layer.Id, 0.3, 0.4);
                obs_sceneitem_get_pos(item, out Vec2 position);
                Require(Math.Abs(position.X - engine.CaptureCanvas.Width * 0.3) < 1 &&
                    Math.Abs(position.Y - engine.CaptureCanvas.Height * 0.4) < 1,
                    "Live drag did not update the scene item immediately.");
                Require((nint)Field(activeSources[layer.Id]!, "Source")! == originalHandle,
                    "Live drag recreated the decoder.");
                engine.MoveCaptureOverlay(layer.Id, 0.95, 1);
                obs_sceneitem_get_pos(item, out position);
                Require(Math.Abs(position.X - engine.CaptureCanvas.Width * 0.4) < 1 &&
                    Math.Abs(position.Y - engine.CaptureCanvas.Height * 0.8) < 1,
                    "Live drag let the overlay cross the right or bottom edge.");
                engine.MoveCaptureOverlay(layer.Id, -1, -1);
                obs_sceneitem_get_pos(item, out position);
                Require(position.X == 0 && position.Y == 0, "Live drag crossed the top or left edge.");

                CaptureOverlay lower = layer.Clone();
                lower.Id = Guid.NewGuid().ToString("N");
                engine.ApplyCaptureOverlays(true, [layer, lower]);
                Require(activeSources.Count == 2, "Multiple layers did not load.");
                Require(obs_sceneitem_get_order_position((nint)Field(activeSources[layer.Id]!, "Item")!) >
                    obs_sceneitem_get_order_position((nint)Field(activeSources[lower.Id]!, "Item")!),
                    "The first UI layer was not topmost.");
                nint scene = (nint)Field(engine, "_overlayScene")!;
                MethodInfo switchCapture = typeof(ObsReplayEngine).GetMethod("SwitchAutomaticCapture", Private)!;
                switchCapture.Invoke(engine, [true, "overlay-qa.exe"]);
                switchCapture.Invoke(engine, [false, ""]);
                Require((nint)Field(engine, "_overlayScene")! == scene && activeSources.Count == 2,
                    "Capture source switching lost overlay layers.");
                engine.ApplyCaptureOverlays(true, [new CaptureOverlay { FilePath = Path.Combine(root, "missing.png") }]);
                Require(!string.IsNullOrEmpty(engine.OverlayWarnings) && engine.IsActive,
                    "Missing media interrupted capture or did not report an error.");
                layer.Opacity = 100;
                for (int i = 0; i < 12; i++)
                {
                    engine.ApplyCaptureOverlays(true, [layer]);
                    engine.ApplyCaptureOverlays(false, [layer]);
                }
                Require(engine.IsActive, "Toggle stress interrupted the output.");
                File.WriteAllText(Path.Combine(root, $"{config.CaptureMode}-metrics.json"),
                    JsonSerializer.Serialize(results, new JsonSerializerOptions { WriteIndented = true }));
            }
            if (!string.IsNullOrEmpty(recording))
                Require(File.Exists(recording) && new FileInfo(recording).Length > 0, "Recording did not finalize.");
            Console.WriteLine($"CAPTURE_OVERLAY_QA {config.CaptureMode} PASS");
            return 0;
        }
        catch (Exception exception)
        {
            Console.Error.WriteLine(exception);
            return 1;
        }
    }

    private static void CheckNormalization()
    {
        string path = Path.Combine(Path.GetTempPath(), "overlay.png");
        var layer = new CaptureOverlay { FilePath = path, X = double.NaN, Width = 0.4, Y = 3, Height = 0.2, Opacity = 500 };
        List<CaptureOverlay> normalized = CaptureOverlay.Normalize([layer, layer, new CaptureOverlay { FilePath = "relative.png" }]);
        Require(normalized.Count == 2 && normalized[0].Id != normalized[1].Id, "Overlay IDs are not unique.");
        Require(normalized[0].X == 0 && normalized[0].Y == 0.8 && normalized[0].Opacity == 100, "Overlay bounds are invalid.");
        var original = new Config { CaptureOverlays = [layer], CaptureOverlaysEnabled = true };
        Config clone = original.Clone();
        clone.CaptureOverlays[0].Opacity = 10;
        Require(original.CaptureOverlays[0].Opacity == 500, "Config clone shares mutable overlay objects.");
        Require(original.PipelineEquals(clone), "Overlay-only changes restart the capture pipeline.");
        Config loaded = JsonSerializer.Deserialize<Config>(JsonSerializer.Serialize(clone))!;
        loaded.Normalize();
        Require(loaded.CaptureOverlaysEnabled && loaded.CaptureOverlays[0].Opacity == 10, "Overlay persistence failed.");
        Require(loaded.CaptureOverlays[0].Y == 0.8, "Overlay persistence did not keep the full layer inside the canvas.");
        CaptureOverlay fullSize = CaptureOverlay.Normalize([new CaptureOverlay
        {
            FilePath = path, X = 1, Y = 1, Width = 1, Height = 1,
        }]).Single();
        Require(fullSize.X == 0 && fullSize.Y == 0, "Full-canvas overlays must align to the origin.");
        Require(CaptureOverlay.Normalize(Enumerable.Repeat(layer, 20)).Count == 16, "Layer limit was not enforced.");
        Console.WriteLine("CAPTURE_OVERLAY_NORMALIZATION PASS");
    }

    private static void CheckEditorTransforms()
    {
        var config = new Config
        {
            CaptureOverlays = [new CaptureOverlay
            {
                FilePath = Path.Combine(Path.GetTempPath(), "overlay.png"), X = 0.7234, Y = 0.7123,
            }],
        };
        double movedX = -1, movedY = -1;
        var window = new CaptureOverlayEditorWindow(config,
            (_, _) => Task.FromResult(""), (_, _, _) => new(false, 0, 0, 0, 0, ""),
            (_, x, y) => { movedX = x; movedY = y; });
        try
        {
            var layers = (IList)Field(window, "_layers")!;
            var layer = (CaptureOverlay)layers[0]!;
            void SetSlider(string name, double value) =>
                ((System.Windows.Controls.Slider)window.FindName(name)).Value = value;
            SetSlider("WidthSlider", 20);
            Require(layer.X == 0.7234 && layer.Y == 0.7123 && layer.Width == 0.2 && layer.Height == 0.25,
                "Width slider changed another transform value.");
            SetSlider("HeightSlider", 15);
            Require(layer.X == 0.7234 && layer.Y == 0.7123 && layer.Width == 0.2 && layer.Height == 0.15,
                "Height slider changed another transform value.");
            SetSlider("OpacitySlider", 40);
            Require(layer.X == 0.7234 && layer.Y == 0.7123 && layer.Opacity == 40,
                "Opacity slider moved the layer.");
            SetSlider("XSlider", 100);
            SetSlider("YSlider", 100);
            Require(layer.X == 0.8 && layer.Y == 0.85,
                "Position sliders did not align the full layer to the right and bottom edges.");
            SetSlider("WidthSlider", 40);
            SetSlider("HeightSlider", 30);
            Require(layer.X == 0.6 && layer.Y == 0.7,
                "Resizing let the overlay cross the right or bottom edge.");
            Require(((System.Windows.Controls.Slider)window.FindName("XSlider")).Value == 100 &&
                ((System.Windows.Controls.Slider)window.FindName("YSlider")).Value == 100,
                "Position sliders lost their full range after resizing.");
            var preview = (CaptureOverlayPreviewHost)window.FindName("Preview");
            var drag = (Action<double, double>)Field(preview, "Dragged")!;
            drag(-10, -10);
            Require(layer.X == 0 && layer.Y == 0 && movedX == 0 && movedY == 0,
                "Preview dragging crossed the top or left edge.");
            drag(10, 10);
            Require(layer.X == 0.6 && layer.Y == 0.7 && movedX == layer.X && movedY == layer.Y,
                "Preview dragging crossed the right or bottom edge.");
            SetSlider("WidthSlider", 100);
            SetSlider("HeightSlider", 100);
            Require(layer.X == 0 && layer.Y == 0 &&
                !((System.Windows.Controls.Slider)window.FindName("XSlider")).IsEnabled &&
                !((System.Windows.Controls.Slider)window.FindName("YSlider")).IsEnabled,
                "Full-canvas overlays must stay at the origin with position controls disabled.");
            SetSlider("WidthSlider", 1);
            SetSlider("HeightSlider", 1);
            SetSlider("XSlider", 100);
            SetSlider("YSlider", 100);
            Require(layer.X == 0.99 && layer.Y == 0.99, "Minimum-size overlay did not reach the edges.");
            SetSlider("XSlider", 0);
            SetSlider("YSlider", 0);
            Require(layer.X == 0 && layer.Y == 0, "Position sliders cannot reach the canvas origin.");
        }
        finally
        {
            typeof(CaptureOverlayEditorWindow).GetMethod("PrepareForShutdown", Private)!.Invoke(window, null);
            window.Close();
        }
        Console.WriteLine("CAPTURE_OVERLAY_EDITOR_TRANSFORMS PASS");
    }

    private static object? Field(object instance, string name) => instance.GetType().GetField(name, Private)!.GetValue(instance);

    private static void CheckDashboardOverlayCard(string? reviewRoot)
    {
        var config = new Config
        {
            CaptureOverlaysEnabled = true,
            CaptureOverlays = [new CaptureOverlay { FilePath = Path.Combine(Path.GetTempPath(), "overlay.png") }],
        };
        string layerId = config.CaptureOverlays[0].Id;
        ConstructorInfo constructor = typeof(SettingsWindow).GetConstructors(Private).Single();
        var window = (SettingsWindow)constructor.Invoke([
            config, false, (Action)(() => { }),
            (Func<bool, Task<bool>>)(enabled => Task.FromResult(enabled)),
            (Func<bool, bool, string, string, Task<bool>>)((_, _, _, _) => Task.FromResult(false)),
            (Func<string?, bool, Task<bool>>)((_, _) => Task.FromResult(false)),
            (Func<Config, bool, Task<bool>>)((_, _) => Task.FromResult(true)),
            EncoderCapabilities.Preview(), Enum.ToObject(constructor.GetParameters()[8].ParameterType, 0),
            CreateUpdateCallback(constructor.GetParameters()[9].ParameterType, nameof(NoUpdate)),
            CreateUpdateCallback(constructor.GetParameters()[10].ParameterType, nameof(NoInstall)),
        ]);
        var toggle = (System.Windows.Controls.Primitives.ToggleButton)window.FindName("CaptureOverlayToggle");
        typeof(SettingsWindow).GetProperty("SetCaptureOverlaysEnabled", Private)!.SetValue(window,
            (Func<bool, Task<string>>)(enabled =>
            {
                config.CaptureOverlaysEnabled = enabled;
                return Task.FromResult("");
            }));
        try
        {
            Require(toggle.IsChecked == true, "Dashboard did not load the overlay master state.");
            foreach (bool enabled in new[] { false, true })
            {
                toggle.IsChecked = enabled;
                toggle.RaiseEvent(new RoutedEventArgs(System.Windows.Controls.Primitives.ButtonBase.ClickEvent));
                Require(config.CaptureOverlaysEnabled == enabled && toggle.IsChecked == enabled && toggle.IsEnabled,
                    "Dashboard overlay toggle did not apply and refresh its state.");
                Require(config.CaptureOverlays.Count == 1 && config.CaptureOverlays[0].Id == layerId,
                    "Dashboard toggle removed configured layers.");
            }
            if (reviewRoot is not null)
            {
                Directory.CreateDirectory(reviewRoot);
                window.Show();
                window.UpdateLayout();
                SaveWindow(window, Path.Combine(reviewRoot, "dashboard.png"));
                var editor = new CaptureOverlayEditorWindow(config, (_, _) => Task.FromResult(""),
                    (_, _, _) => new(), (_, _, _) => { }) { Owner = window };
                editor.ContentRendered += (_, _) =>
                {
                    Require(window.IsVisible && editor.Owner == window && !editor.ShowInTaskbar,
                        "Capture editor must stay over its visible owner without a taskbar entry.");
                    SaveWindow(editor, Path.Combine(reviewRoot, "editor.png"));
                    var escape = new System.Windows.Input.KeyEventArgs(System.Windows.Input.Keyboard.PrimaryDevice,
                        PresentationSource.FromVisual(editor), 0, System.Windows.Input.Key.Escape)
                    {
                        RoutedEvent = System.Windows.Input.Keyboard.PreviewKeyDownEvent,
                    };
                    typeof(CaptureOverlayEditorWindow).GetMethod("Window_PreviewKeyDown", Private)!
                        .Invoke(editor, [editor, escape]);
                    Require(escape.Handled, "Capture editor did not handle Escape.");
                };
                editor.ShowDialog();
                Require(window.IsVisible, "Closing the capture editor hid the dashboard.");
            }
        }
        finally { window.Close(); }
        Console.WriteLine("CAPTURE_OVERLAY_DASHBOARD PASS");
    }

    private static void SaveWindow(Window window, string path)
    {
        var bitmap = new System.Windows.Media.Imaging.RenderTargetBitmap(
            (int)window.ActualWidth, (int)window.ActualHeight, 96, 96, System.Windows.Media.PixelFormats.Pbgra32);
        bitmap.Render(window);
        var encoder = new System.Windows.Media.Imaging.PngBitmapEncoder();
        encoder.Frames.Add(System.Windows.Media.Imaging.BitmapFrame.Create(bitmap));
        using var stream = File.Create(path);
        encoder.Save(stream);
    }

    private static Delegate CreateUpdateCallback(Type delegateType, string method)
    {
        Type releaseType = typeof(SettingsWindow).Assembly.GetType("Shadowless.UpdateRelease")!;
        return typeof(Program).GetMethod(method, BindingFlags.Static | BindingFlags.NonPublic)!
            .MakeGenericMethod(releaseType).CreateDelegate(delegateType);
    }

    private static Task<T?> NoUpdate<T>(bool force, CancellationToken token) => Task.FromResult(default(T));
    private static Task NoInstall<T>(T release, IProgress<int> progress, CancellationToken token) => Task.CompletedTask;
    private static void CheckOpacity(object source, double expected)
    {
        nint settings = obs_source_get_settings((nint)Field(source, "OpacityFilter")!);
        try
        {
            Require(Math.Abs(obs_data_get_double(settings, "opacity") - expected) < 0.0001,
                "Overlay filter opacity is not normalized to the OBS 0..1 range.");
        }
        finally { obs_data_release(settings); }
    }
    private static void Require(bool condition, string message)
    {
        if (!condition)
            throw new InvalidOperationException(message);
    }

    [DllImport("obs.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern uint obs_source_get_width(nint source);
    [DllImport("obs.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern uint obs_source_get_height(nint source);
    [DllImport("obs.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern nint obs_source_get_settings(nint source);
    [DllImport("obs.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern double obs_data_get_double(nint data, [MarshalAs(UnmanagedType.LPUTF8Str)] string name);
    [DllImport("obs.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void obs_data_release(nint data);
    [DllImport("obs.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern uint obs_source_get_audio_mixers(nint source);
    [StructLayout(LayoutKind.Sequential)]
    private struct Vec2 { internal float X, Y; }
    [StructLayout(LayoutKind.Sequential)]
    private struct Vec4 { internal float X, Y, Z, W; }
    [StructLayout(LayoutKind.Sequential)]
    private struct Matrix4 { internal Vec4 X, Y, Z, T; }
    [DllImport("obs.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void obs_sceneitem_get_pos(nint item, out Vec2 position);
    [DllImport("obs.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void obs_sceneitem_get_draw_transform(nint item, out Matrix4 transform);
    [DllImport("obs.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int obs_sceneitem_get_order_position(nint item);
    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool SetWindowDisplayAffinity(nint window, uint affinity);
}
