namespace Shadowless;

public partial class App
{
    private CaptureOverlayEditorWindow? _captureOverlayEditor;

    private void OpenCaptureOverlayEditor()
    {
        if (_captureOverlayEditor is not null)
        {
            _captureOverlayEditor.Activate();
            return;
        }
        _captureOverlayEditor = new CaptureOverlayEditorWindow(
            _config!, ApplyCaptureOverlaysAsync, RefreshOverlayPreview, MoveCaptureOverlay)
        {
            Owner = _settingsWindow,
        };
        CaptureOverlayEditorWindow editor = _captureOverlayEditor;
        editor.Closed += (_, _) =>
        {
            _captureOverlayEditor = null;
            if (_exiting == 0 && _settingsWindow is not null)
            {
                _settingsWindow.UpdateCaptureOverlayState();
                if (editor.CloseOwnerOnExit)
                    _settingsWindow.Close();
                else
                    _settingsWindow.Activate();
            }
        };
        _captureOverlayEditor.ShowDialog();
    }

    private async Task<string> ApplyCaptureOverlaysAsync(bool enabled, List<CaptureOverlay> layers)
    {
        await _pipelineGate.WaitAsync();
        bool previousEnabled = _config!.CaptureOverlaysEnabled;
        List<CaptureOverlay> previous = CaptureOverlay.Normalize(_config.CaptureOverlays);
        try
        {
            List<CaptureOverlay> normalized = CaptureOverlay.Normalize(layers);
            string warnings = await RunOnObsThreadAsync(() =>
            {
                _obs?.ApplyCaptureOverlays(enabled, normalized);
                return _obs?.OverlayWarnings ?? "";
            });
            _config.CaptureOverlaysEnabled = enabled;
            _config.CaptureOverlays = normalized;
            _config.Save();
            return warnings;
        }
        catch
        {
            _config.CaptureOverlaysEnabled = previousEnabled;
            _config.CaptureOverlays = previous;
            await RunOnObsThreadAsync(() => _obs?.ApplyCaptureOverlays(previousEnabled, previous));
            throw;
        }
        finally { _pipelineGate.Release(); }
    }

    private void MoveCaptureOverlay(string id, double x, double y) =>
        RunOnObsThreadAsync(() => _obs?.MoveCaptureOverlay(id, x, y)).GetAwaiter().GetResult();

    // Synchronous so the display is detached before HwndHost destroys its HWND.
    // The OBS worker does not wait for the UI dispatcher or the pipeline gate.
    private CaptureOverlayPreviewState RefreshOverlayPreview(nint window, uint width, uint height) =>
        RunOnObsThreadAsync(() =>
        {
            ObsReplayEngine? engine = _obs;
            if (engine is null)
                return new CaptureOverlayPreviewState();
            engine.UpdateOverlayPreview(window, width, height);
            var canvas = engine.CaptureCanvas;
            return new CaptureOverlayPreviewState(engine.IsActive, canvas.Width, canvas.Height,
                engine.TotalRenderedFrames, engine.LaggedRenderedFrames, engine.OverlayWarnings);
        }).GetAwaiter().GetResult();
}
