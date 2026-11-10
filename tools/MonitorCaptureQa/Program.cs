using Shadowless.Interop;

// Run in an interactive Windows session. This reads monitor metadata only.
var monitors = CaptureInterop.EnumerateMonitors();
if (monitors.Count == 0)
    throw new InvalidOperationException("No monitors available for capture QA.");

foreach (var monitor in monitors)
{
    if (string.IsNullOrWhiteSpace(monitor.DeviceId) ||
        monitor.Width <= 0 || monitor.Height <= 0)
    {
        throw new InvalidOperationException(
            $"Monitor {monitor.Index} has no usable OBS identifier or dimensions.");
    }

    Console.WriteLine(
        $"Monitor {monitor.Index}: {monitor.Width}x{monitor.Height}, OBS ID={monitor.DeviceId}");
}

Console.WriteLine("MONITOR_CAPTURE_QA PASS");
