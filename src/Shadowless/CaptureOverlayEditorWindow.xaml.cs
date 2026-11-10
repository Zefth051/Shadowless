using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Threading;
using Microsoft.Win32;

namespace Shadowless;

public partial class CaptureOverlayEditorWindow : Window
{
    private readonly ObservableCollection<CaptureOverlay> _layers;
    private readonly Func<bool, List<CaptureOverlay>, Task<string>> _apply;
    private readonly Action<string, double, double> _moveLayer;
    private readonly DispatcherTimer _saveTimer = new() { Interval = TimeSpan.FromMilliseconds(150) };
    private readonly DispatcherTimer _previewTimer = new() { Interval = TimeSpan.FromSeconds(1) };
    private readonly CancellationTokenSource _lifetime = new();
    private bool _updating = true, _pending, _allowClose, _closing;
    private bool _dragging;
    private Task<bool>? _saveTask;
    private uint _lastRendered, _lastLagged;
    private bool _hasSample;
    private DateTime _lastSample = DateTime.UtcNow;

    public CaptureOverlayEditorWindow(Config config,
        Func<bool, List<CaptureOverlay>, Task<string>> apply,
        Func<nint, uint, uint, CaptureOverlayPreviewState> preview,
        Action<string, double, double> moveLayer)
    {
        _apply = apply;
        _moveLayer = moveLayer;
        _layers = new ObservableCollection<CaptureOverlay>(CaptureOverlay.Normalize(config.CaptureOverlays));
        InitializeComponent();
        Preview.RefreshPreview = preview;
        Preview.DragStarted += () =>
        {
            _dragging = true;
            _saveTimer.Stop();
        };
        Preview.DragFinished += () =>
        {
            _dragging = false;
            if (_pending)
                ScheduleSave();
        };
        Preview.Dragged += (x, y) =>
        {
            if (Selected is not { } layer)
                return;
            layer.SetPosition(layer.X + x, layer.Y + y);
            LoadTransform();
            // Position updates never rebuild media sources or write config files.
            _moveLayer(layer.Id, layer.X, layer.Y);
            ScheduleSave();
        };
        LayerList.ItemsSource = _layers;
        MasterToggle.IsChecked = config.CaptureOverlaysEnabled;
        EmptyText.Visibility = _layers.Count == 0 ? Visibility.Visible : Visibility.Collapsed;
        if (_layers.Count > 0)
            LayerList.SelectedIndex = 0;
        _updating = false;
        StatusText.Text = Localization.Text("L.Overlays.AutoSave");
        _saveTimer.Tick += async (_, _) =>
        {
            _saveTimer.Stop();
            await SavePendingAsync();
        };
        _previewTimer.Tick += (_, _) => RefreshPreview();
        Loaded += (_, _) =>
        {
            RefreshPreview();
            _previewTimer.Start();
        };
        StateChanged += (_, _) => RefreshPreview();
        IsVisibleChanged += (_, _) =>
        {
            if (IsLoaded)
                RefreshPreview();
        };
        SourceInitialized += (_, _) =>
        {
            // Keep the editor/preview out of desktop capture to avoid visual feedback.
            if (!SetWindowDisplayAffinity(new WindowInteropHelper(this).Handle, 0x11))
                Log.Write("Capture overlay editor exclusion is unavailable.");
        };
        Closing += OnClosing;
        Closed += (_, _) =>
        {
            _saveTimer.Stop();
            _previewTimer.Stop();
            _lifetime.Cancel();
            _lifetime.Dispose();
            Preview.Dispose();
        };
    }

    private CaptureOverlay? Selected => LayerList.SelectedItem as CaptureOverlay;

    internal bool CloseOwnerOnExit { get; set; }

    internal void PrepareForShutdown()
    {
        _saveTimer.Stop();
        _previewTimer.Stop();
        Preview.Refresh(false);
        Preview.RefreshPreview = null;
        _allowClose = true;
    }

    private void RefreshPreview()
    {
        try
        {
            bool visible = IsVisible && WindowState != WindowState.Minimized;
            CaptureOverlayPreviewState state = Preview.Refresh(visible);
            DateTime now = DateTime.UtcNow;
            double seconds = Math.Max(0.001, (now - _lastSample).TotalSeconds);
            uint frames = _hasSample && state.Rendered >= _lastRendered ? state.Rendered - _lastRendered : 0;
            uint lag = _hasSample && state.Lagged >= _lastLagged ? state.Lagged - _lastLagged : 0;
            PerformanceText.Text = state.Running
                ? Localization.Format("L.Overlays.Metrics", state.Width, state.Height,
                    (frames / seconds).ToString("0.0"), frames == 0 ? "0.0" : (100.0 * lag / frames).ToString("0.0"))
                : Localization.Text("L.Overlays.StartCapture");
            _lastSample = now;
            _lastRendered = state.Rendered;
            _lastLagged = state.Lagged;
            _hasSample = state.Running;
            if (!string.IsNullOrEmpty(state.Warnings))
                StatusText.Text = state.Warnings;
        }
        catch (Exception exception)
        {
            _previewTimer.Stop();
            StatusText.Text = Localization.Text("L.Overlays.PreviewFailed");
            Log.Write($"Capture overlay preview failed: {exception}");
        }
    }

    private void ScheduleSave()
    {
        if (_updating || _closing)
            return;
        _pending = true;
        if (_dragging)
            return;
        if (!_saveTimer.IsEnabled)
            _saveTimer.Start();
    }

    private Task<bool> SavePendingAsync()
    {
        if (_saveTask is { IsCompleted: false })
            return _saveTask;
        _saveTask = PersistPendingAsync();
        return _saveTask;
    }

    private async Task<bool> PersistPendingAsync()
    {
        try
        {
            while (_pending)
            {
                _pending = false;
                string warnings = await _apply(MasterToggle.IsChecked == true, _layers.Select(layer => layer.Clone()).ToList());
                StatusText.Text = string.IsNullOrEmpty(warnings)
                    ? Localization.Text("L.Overlays.AutoSave") : warnings;
            }
            return true;
        }
        catch (Exception exception)
        {
            _pending = true;
            StatusText.Text = Localization.Format("L.Overlays.SaveFailed", exception.Message);
            Log.Write($"Capture overlay settings failed: {exception}");
            return false;
        }
    }

    private void LoadTransform()
    {
        bool wasUpdating = _updating;
        _updating = true;
        TransformPanel.IsEnabled = Selected is not null;
        if (Selected is { } layer)
        {
            LayerToggle.IsChecked = layer.Enabled;
            XSlider.IsEnabled = layer.Width < 1;
            YSlider.IsEnabled = layer.Height < 1;
            XSlider.Value = layer.Width < 1 ? layer.X / (1 - layer.Width) * 100 : 0;
            YSlider.Value = layer.Height < 1 ? layer.Y / (1 - layer.Height) * 100 : 0;
            WidthSlider.Value = layer.Width * 100;
            HeightSlider.Value = layer.Height * 100;
            OpacitySlider.Value = layer.Opacity;
        }
        _updating = wasUpdating;
    }

    private async void Add_Click(object sender, RoutedEventArgs e)
    {
        if (_layers.Count >= 16)
        {
            StatusText.Text = Localization.Text("L.Overlays.Limit");
            return;
        }
        var dialog = new OpenFileDialog
        {
            Title = Localization.Text("L.Overlays.Add"), CheckFileExists = true,
            Filter = Localization.Text("L.Overlays.FileFilter"),
        };
        if (dialog.ShowDialog(this) != true)
            return;
        if (!File.Exists(dialog.FileName) || !CaptureOverlay.Supports(dialog.FileName))
            return;
        AddMediaButton.IsEnabled = false;
        try
        {
            VideoStreamInfo? media = await new FfmpegAdapter().ReadVideoInfoAsync(dialog.FileName, _lifetime.Token);
            if (media is null || media.Width <= 0 || media.Height <= 0)
                throw new InvalidOperationException(Localization.Text("L.Overlays.LoadFailed"));
            if (_closing)
                return;
            var layer = new CaptureOverlay { FilePath = dialog.FileName };
            _layers.Add(layer);
            EmptyText.Visibility = Visibility.Collapsed;
            LayerList.SelectedItem = layer;
            MasterToggle.IsChecked = true;
            ScheduleSave();
        }
        catch (OperationCanceledException) when (_lifetime.IsCancellationRequested) { }
        catch (Exception exception)
        {
            StatusText.Text = $"{Localization.Text("L.Overlays.LoadFailed")} {exception.Message}";
        }
        finally { AddMediaButton.IsEnabled = true; }
    }

    private void Remove_Click(object sender, RoutedEventArgs e)
    {
        if (Selected is not { } layer)
            return;
        int index = LayerList.SelectedIndex;
        _layers.Remove(layer);
        LayerList.SelectedIndex = Math.Min(index, _layers.Count - 1);
        EmptyText.Visibility = _layers.Count == 0 ? Visibility.Visible : Visibility.Collapsed;
        ScheduleSave();
    }

    private void MoveLayer(int delta)
    {
        int index = LayerList.SelectedIndex;
        int target = index + delta;
        if (index < 0 || target < 0 || target >= _layers.Count)
            return;
        _layers.Move(index, target);
        LayerList.SelectedIndex = target;
        ScheduleSave();
    }

    private void Raise_Click(object sender, RoutedEventArgs e) => MoveLayer(-1);
    private void Lower_Click(object sender, RoutedEventArgs e) => MoveLayer(1);
    private void Layer_Changed(object sender, SelectionChangedEventArgs e) => LoadTransform();
    private void Master_Click(object sender, RoutedEventArgs e) => ScheduleSave();
    private void LayerToggle_Click(object sender, RoutedEventArgs e)
    {
        if (Selected is { } layer)
            layer.Enabled = LayerToggle.IsChecked == true;
        ScheduleSave();
    }

    private void Transform_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        if (_updating || Selected is not { } layer)
            return;
        if (sender == WidthSlider)
            layer.Width = WidthSlider.Value / 100;
        else if (sender == HeightSlider)
            layer.Height = HeightSlider.Value / 100;
        else if (sender == XSlider)
            layer.X = XSlider.Value / 100 * (1 - layer.Width);
        else if (sender == YSlider)
            layer.Y = YSlider.Value / 100 * (1 - layer.Height);
        else if (sender == OpacitySlider)
            layer.Opacity = (int)OpacitySlider.Value;
        layer.SetPosition(layer.X, layer.Y);
        LoadTransform();
        ScheduleSave();
    }

    private async void Apply_Click(object sender, RoutedEventArgs e)
    {
        _pending = true;
        _saveTimer.Stop();
        await SavePendingAsync();
    }

    private async void OnClosing(object? sender, CancelEventArgs e)
    {
        if (_allowClose)
            return;
        e.Cancel = true;
        if (_closing)
            return;
        _closing = true;
        IsEnabled = false;
        _saveTimer.Stop();
        // Finish the current Closing event before issuing the final Close call.
        await Dispatcher.Yield(DispatcherPriority.Background);
        bool saved = await SavePendingAsync();
        if (saved)
        {
            Preview.Refresh(false);
            _allowClose = true;
            Close();
        }
        else
        {
            CloseOwnerOnExit = false;
            IsEnabled = true;
            _closing = false;
        }
    }

    private void Header_Drag(object sender, MouseButtonEventArgs e)
    {
        if (e.OriginalSource is TextBlock && e.ButtonState == MouseButtonState.Pressed)
            DragMove();
    }
    private void Close_Click(object sender, RoutedEventArgs e) => Close();

    private void Window_PreviewKeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key != Key.Escape)
            return;
        e.Handled = true;
        Close();
    }

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool SetWindowDisplayAffinity(nint window, uint affinity);
}
