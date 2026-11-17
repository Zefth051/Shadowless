using NAudio.CoreAudioApi;

namespace Shadowless;

internal static class AudioDevices
{
    internal static IReadOnlyList<(string Id, string Name)> ListRenderDevices() =>
        List(DataFlow.Render);

    internal static IReadOnlyList<(string Id, string Name)> ListCaptureDevices() =>
        List(DataFlow.Capture);

    // Resolve again on endpoint notifications; never overwrite the saved preference.
    internal static string ResolveMicrophone(string preferredId)
    {
        try
        {
            using var enumerator = new MMDeviceEnumerator();
            if (!string.IsNullOrWhiteSpace(preferredId) && preferredId != "default")
            {
                try
                {
                    using var preferred = enumerator.GetDevice(preferredId);
                    if (preferred.State == DeviceState.Active && preferred.DataFlow == DataFlow.Capture)
                        return preferred.ID;
                }
                catch (System.Runtime.InteropServices.COMException) { }
            }
            using var fallback = enumerator.GetDefaultAudioEndpoint(DataFlow.Capture, Role.Console);
            return fallback.ID;
        }
        catch
        {
            // WASAPI can wait for a default endpoint when no inputs are connected.
            return "";
        }
    }

    private static IReadOnlyList<(string Id, string Name)> List(DataFlow flow)
    {
        using var enumerator = new MMDeviceEnumerator();
        return enumerator
            .EnumerateAudioEndPoints(flow, DeviceState.Active)
            .Select(device => (device.ID, device.FriendlyName))
            .ToList();
    }
}
