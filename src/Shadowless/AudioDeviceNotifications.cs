using NAudio.CoreAudioApi;
using NAudio.CoreAudioApi.Interfaces;

namespace Shadowless;

internal sealed class AudioDeviceNotifications : IMMNotificationClient, IDisposable
{
    private readonly MMDeviceEnumerator _enumerator = new();
    private readonly Action _changed;
    private int _disposed;

    internal AudioDeviceNotifications(Action changed)
    {
        _changed = changed;
        try
        {
            _enumerator.RegisterEndpointNotificationCallback(this);
        }
        catch
        {
            _enumerator.Dispose();
            throw;
        }
    }

    private void Changed()
    {
        if (Volatile.Read(ref _disposed) == 0)
            _changed();
    }

    public void OnDeviceStateChanged(string deviceId, DeviceState newState) => Changed();
    public void OnDeviceAdded(string deviceId) => Changed();
    public void OnDeviceRemoved(string deviceId) => Changed();
    public void OnDefaultDeviceChanged(DataFlow flow, Role role, string defaultDeviceId)
    {
        if (flow == DataFlow.Capture && role == Role.Console)
            Changed();
    }
    public void OnPropertyValueChanged(string deviceId, PropertyKey key) { }

    public void Dispose()
    {
        if (Interlocked.Exchange(ref _disposed, 1) != 0)
            return;
        try
        {
            _enumerator.UnregisterEndpointNotificationCallback(this);
        }
        finally
        {
            _enumerator.Dispose();
        }
    }
}
