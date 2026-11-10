using System.IO;

namespace Shadowless;

public sealed partial class ObsReplayEngine
{
    private sealed class OverlaySource(CaptureOverlay layer, nint source, nint item)
    {
        internal CaptureOverlay Layer = layer;
        internal nint Source = source;
        internal nint Item = item;
        internal nint OpacityFilter;
    }

    private readonly Dictionary<string, OverlaySource> _overlaySources = [];
    private nint _overlayScene;
    private nint _overlayBaseItem;
    private nint _overlayBaseSource;
    private nint _overlayDisplay;
    private nint _overlayPreviewWindow;
    private uint _previewWidth, _previewHeight;
    private ObsNative.DrawCallback? _overlayDrawCallback;
    private string _overlayWarnings = "";

    public (uint Width, uint Height) CaptureCanvas => (_baseWidth, _baseHeight);
    public string OverlayWarnings => _overlayWarnings;

    public void MoveCaptureOverlay(string id, double x, double y)
    {
        if (_disposing || !_overlaySources.TryGetValue(id, out OverlaySource? overlay) ||
            !double.IsFinite(x) || !double.IsFinite(y))
            return;
        overlay.Layer.SetPosition(x, y);
        var position = new ObsNative.Vec2
        {
            X = (float)(overlay.Layer.X * _baseWidth),
            Y = (float)(overlay.Layer.Y * _baseHeight),
        };
        ObsNative.obs_sceneitem_set_pos(overlay.Item, ref position);
    }

    // Called only through the application's OBS scheduler. No output/encoder restart.
    public void ApplyCaptureOverlays(bool enabled, IEnumerable<CaptureOverlay> layers)
    {
        if (_disposing || !_obsStarted)
            return;
        List<CaptureOverlay> active = enabled
            ? CaptureOverlay.Normalize(layers).Where(layer => layer.Enabled && layer.Opacity > 0).ToList()
            : [];
        var warnings = new List<string>();
        var desired = active.Select(layer => layer.Id).ToHashSet(StringComparer.Ordinal);
        foreach (string id in _overlaySources.Keys.Where(id => !desired.Contains(id)).ToArray())
            RemoveCaptureOverlay(id);

        foreach (CaptureOverlay layer in active)
        {
            try
            {
                if (_overlaySources.TryGetValue(layer.Id, out OverlaySource? current) &&
                    current.Layer.FilePath != layer.FilePath)
                {
                    RemoveCaptureOverlay(layer.Id);
                    current = null;
                }
                if (current is null)
                {
                    if (!File.Exists(layer.FilePath))
                        throw new FileNotFoundException(Localization.Text("L.Overlays.Missing"), layer.FilePath);
                    EnsureCaptureOverlayScene();
                    current = CreateCaptureOverlay(layer);
                    _overlaySources.Add(layer.Id, current);
                }
                SetOverlayTransform(current, layer);
                current.Layer = layer.Clone();
            }
            catch (Exception exception)
            {
                RemoveCaptureOverlay(layer.Id);
                warnings.Add($"{layer.Name}: {exception.Message}");
                Log.Write($"Capture overlay skipped ({layer.Name}): {exception.Message}");
            }
        }

        // UI lists the topmost layer first. Establish ordering after all additions.
        int order = 1;
        foreach (CaptureOverlay layer in active.AsEnumerable().Reverse())
            if (_overlaySources.TryGetValue(layer.Id, out OverlaySource? overlay))
                ObsNative.obs_sceneitem_set_order_position(overlay.Item, order++);

        // Bypass composition entirely when disabled, and release animated decoders.
        if (_overlaySources.Count == 0)
        {
            ObsNative.obs_set_output_source(0, _videoSource);
            DestroyCaptureOverlayScene();
        }
        else
        {
            UpdateOverlayBaseSource();
        }
        _overlayWarnings = string.Join(Environment.NewLine, warnings);
    }

    private void EnsureCaptureOverlayScene()
    {
        if (_overlayScene != 0)
            return;
        _overlayScene = ObsNative.obs_scene_create_private("Shadowless Capture Overlays");
        if (_overlayScene == 0)
            throw new InvalidOperationException(Localization.Text("L.Overlays.LoadFailed"));
        _overlayBaseItem = ObsNative.obs_scene_add(_overlayScene, _videoSource);
        if (_overlayBaseItem == 0)
        {
            DestroyCaptureOverlayScene();
            throw new InvalidOperationException(Localization.Text("L.Overlays.LoadFailed"));
        }
        _overlayBaseSource = _videoSource;
    }

    private void UpdateOverlayBaseSource()
    {
        if (_overlayScene == 0)
        {
            ObsNative.obs_set_output_source(0, _videoSource);
            return;
        }
        if (_overlayBaseSource != _videoSource)
        {
            nint replacement = ObsNative.obs_scene_add(_overlayScene, _videoSource);
            if (replacement == 0)
                throw new InvalidOperationException(Localization.Text("L.Overlays.LoadFailed"));
            ObsNative.obs_sceneitem_set_order_position(replacement, 0);
            ObsNative.obs_sceneitem_remove(_overlayBaseItem);
            _overlayBaseItem = replacement;
            _overlayBaseSource = _videoSource;
        }
        ObsNative.obs_set_output_source(0, ObsNative.obs_scene_get_source(_overlayScene));
    }

    private OverlaySource CreateCaptureOverlay(CaptureOverlay layer)
    {
        nint settings = ObsNative.obs_data_create();
        nint source = 0;
        try
        {
            ObsNative.obs_data_set_string(settings, layer.IsImage ? "file" : "local_file", layer.FilePath);
            ObsNative.obs_data_set_bool(settings, "is_local_file", true);
            ObsNative.obs_data_set_bool(settings, "looping", true);
            ObsNative.obs_data_set_bool(settings, "hw_decode", true);
            ObsNative.obs_data_set_bool(settings, "close_when_inactive", true);
            ObsNative.obs_data_set_bool(settings, "restart_on_activate", true);
            ObsNative.obs_data_set_bool(settings, "linear_alpha", true);
            source = ObsNative.obs_source_create(
                layer.IsImage ? "image_source" : "ffmpeg_source", $"Shadowless Overlay {layer.Id}", settings, 0);
            if (source == 0)
                throw new InvalidOperationException(Localization.Text("L.Overlays.LoadFailed"));
            // Media overlays must never contribute an additional audio track.
            ObsNative.obs_source_set_audio_mixers(source, 0);
            ObsNative.obs_source_set_volume(source, 0);
            nint item = ObsNative.obs_scene_add(_overlayScene, source);
            if (item == 0)
                throw new InvalidOperationException(Localization.Text("L.Overlays.LoadFailed"));
            var result = new OverlaySource(layer.Clone(), source, item);
            source = 0;
            return result;
        }
        finally
        {
            if (source != 0)
                ObsNative.obs_source_release(source);
            ObsNative.obs_data_release(settings);
        }
    }

    private void SetOverlayTransform(OverlaySource overlay, CaptureOverlay layer)
    {
        var position = new ObsNative.Vec2 { X = (float)(layer.X * _baseWidth), Y = (float)(layer.Y * _baseHeight) };
        var bounds = new ObsNative.Vec2 { X = (float)(layer.Width * _baseWidth), Y = (float)(layer.Height * _baseHeight) };
        ObsNative.obs_sceneitem_set_alignment(overlay.Item, 5); // OBS_ALIGN_LEFT | OBS_ALIGN_TOP
        ObsNative.obs_sceneitem_set_bounds_alignment(overlay.Item, 5);
        ObsNative.obs_sceneitem_set_bounds_type(overlay.Item, ObsNative.BoundsType.Stretch);
        ObsNative.obs_sceneitem_set_bounds(overlay.Item, ref bounds);
        ObsNative.obs_sceneitem_set_pos(overlay.Item, ref position);

        // Avoid the extra filter render pass at full opacity.
        if (layer.Opacity == 100 && overlay.OpacityFilter != 0)
        {
            ObsNative.obs_source_filter_remove(overlay.Source, overlay.OpacityFilter);
            ObsNative.obs_source_release(overlay.OpacityFilter);
            overlay.OpacityFilter = 0;
        }
        if (layer.Opacity < 100)
        {
            nint settings = ObsNative.obs_data_create();
            try
            {
                ObsNative.obs_data_set_double(settings, "opacity", layer.Opacity / 100.0);
                if (overlay.OpacityFilter == 0)
                {
                    overlay.OpacityFilter = ObsNative.obs_source_create("color_filter_v2", "Shadowless Overlay Opacity", settings, 0);
                    if (overlay.OpacityFilter == 0)
                        throw new InvalidOperationException(Localization.Text("L.Overlays.LoadFailed"));
                    ObsNative.obs_source_filter_add(overlay.Source, overlay.OpacityFilter);
                }
                else if (overlay.Layer.Opacity != layer.Opacity)
                    ObsNative.obs_source_update(overlay.OpacityFilter, settings);
            }
            finally { ObsNative.obs_data_release(settings); }
        }
    }

    private void RemoveCaptureOverlay(string id)
    {
        if (!_overlaySources.Remove(id, out OverlaySource? overlay))
            return;
        ObsNative.obs_sceneitem_remove(overlay.Item);
        if (overlay.OpacityFilter != 0)
        {
            ObsNative.obs_source_filter_remove(overlay.Source, overlay.OpacityFilter);
            ObsNative.obs_source_release(overlay.OpacityFilter);
        }
        ObsNative.obs_source_remove(overlay.Source);
        ObsNative.obs_source_release(overlay.Source);
    }

    private void DestroyCaptureOverlayScene()
    {
        foreach (string id in _overlaySources.Keys.ToArray())
            RemoveCaptureOverlay(id);
        if (_overlayScene != 0)
            ObsNative.obs_scene_release(_overlayScene);
        _overlayScene = _overlayBaseItem = _overlayBaseSource = 0;
    }

    internal void UpdateOverlayPreview(nint window, uint width, uint height)
    {
        if (_disposing || !_started || window == 0)
        {
            DestroyOverlayPreview();
            return;
        }
        if (window != _overlayPreviewWindow)
        {
            DestroyOverlayPreview();
            var info = new ObsNative.DisplayInfo { Window = window, Width = width, Height = height, Format = 3 };
            _overlayDisplay = ObsNative.obs_display_create(ref info, 0xFF000000);
            if (_overlayDisplay == 0)
                throw new InvalidOperationException(Localization.Text("L.Overlays.PreviewFailed"));
            _overlayPreviewWindow = window;
            _overlayDrawCallback = DrawOverlayPreview;
            ObsNative.obs_display_add_draw_callback(_overlayDisplay, _overlayDrawCallback, 0);
            _previewWidth = width;
            _previewHeight = height;
        }
        else if (width != _previewWidth || height != _previewHeight)
        {
            ObsNative.obs_display_resize(_overlayDisplay, width, height);
            _previewWidth = width;
            _previewHeight = height;
        }
    }

    private void DrawOverlayPreview(nint data, uint width, uint height)
    {
        double scale = Math.Min((double)width / _baseWidth, (double)height / _baseHeight);
        int w = Math.Max(1, (int)(_baseWidth * scale));
        int h = Math.Max(1, (int)(_baseHeight * scale));
        ObsNative.gs_set_viewport(((int)width - w) / 2, ((int)height - h) / 2, w, h);
        ObsNative.gs_ortho(0, _baseWidth, 0, _baseHeight, -100, 100);
        ObsNative.obs_render_main_texture();
    }

    private void DestroyOverlayPreview()
    {
        if (_overlayDisplay != 0)
            ObsNative.obs_display_destroy(_overlayDisplay);
        _overlayDisplay = _overlayPreviewWindow = 0;
        _overlayDrawCallback = null;
    }
}
