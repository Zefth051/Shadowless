using System.Runtime.InteropServices;
using System.Windows.Interop;
using System.Windows.Media;

namespace Shadowless;

public sealed record CaptureOverlayPreviewState(
    bool Running = false, uint Width = 1920, uint Height = 1080,
    uint Rendered = 0, uint Lagged = 0, string Warnings = "");

public sealed class CaptureOverlayPreviewHost : HwndHost
{
    private nint _window;
    private bool _dragging;
    private double _lastX, _lastY;
    internal Func<nint, uint, uint, CaptureOverlayPreviewState>? RefreshPreview;
    internal event Action<double, double>? Dragged;
    internal event Action? DragStarted;
    internal event Action? DragFinished;
    internal CaptureOverlayPreviewState State { get; private set; } = new();

    internal CaptureOverlayPreviewState Refresh(bool visible)
    {
        if (RefreshPreview is null || _window == 0)
            return State;
        var dpi = VisualTreeHelper.GetDpi(this);
        State = RefreshPreview(visible ? _window : 0,
            (uint)Math.Max(1, ActualWidth * dpi.DpiScaleX),
            (uint)Math.Max(1, ActualHeight * dpi.DpiScaleY));
        return State;
    }

    protected override HandleRef BuildWindowCore(HandleRef hwndParent)
    {
        // SS_NOTIFY keeps mouse hit testing on this child instead of the WPF parent.
        _window = CreateWindowEx(0, "STATIC", "", 0x56000100,
            0, 0, 1, 1, hwndParent.Handle, 0, 0, 0);
        if (_window == 0)
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
        return new HandleRef(this, _window);
    }

    protected override void DestroyWindowCore(HandleRef hwnd)
    {
        try { Refresh(false); }
        finally
        {
            DestroyWindow(hwnd.Handle);
            _window = 0;
        }
    }

    protected override nint WndProc(nint hwnd, int msg, nint wParam, nint lParam, ref bool handled)
    {
        if (msg is 0x0201 or 0x0200)
        {
            double x = (short)(lParam.ToInt64() & 0xFFFF);
            double y = (short)((lParam.ToInt64() >> 16) & 0xFFFF);
            if (msg == 0x0201)
            {
                _dragging = true;
                SetCapture(hwnd);
                DragStarted?.Invoke();
            }
            else if (_dragging)
            {
                var dpi = VisualTreeHelper.GetDpi(this);
                double scale = Math.Min(ActualWidth * dpi.DpiScaleX / State.Width,
                    ActualHeight * dpi.DpiScaleY / State.Height);
                if (scale > 0)
                    Dragged?.Invoke((x - _lastX) / (State.Width * scale), (y - _lastY) / (State.Height * scale));
            }
            _lastX = x;
            _lastY = y;
            handled = true;
        }
        else if (msg is 0x0202 or 0x0215)
        {
            bool wasDragging = _dragging;
            _dragging = false;
            if (msg == 0x0202)
                ReleaseCapture();
            if (wasDragging)
                DragFinished?.Invoke();
            handled = true;
        }
        return base.WndProc(hwnd, msg, wParam, lParam, ref handled);
    }

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern nint CreateWindowEx(uint exStyle, string className, string name,
        uint style, int x, int y, int width, int height, nint parent, nint menu, nint instance, nint param);
    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool DestroyWindow(nint window);
    [DllImport("user32.dll")]
    private static extern nint SetCapture(nint window);
    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool ReleaseCapture();
}
