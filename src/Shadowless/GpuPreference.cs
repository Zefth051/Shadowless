using System.Runtime.InteropServices;

namespace Shadowless;

internal static class GpuPreference
{
    [DllImport("ShadowlessObsBridge.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int shadowless_get_high_performance_adapter_index();

    [DllImport("ShadowlessObsBridge.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int shadowless_get_power_saving_adapter_index();

    [DllImport("ShadowlessObsBridge.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int shadowless_get_monitor_adapter_index(nint monitor);

    internal static int MonitorAdapterIndex(nint monitor)
    {
        try
        {
            return shadowless_get_monitor_adapter_index(monitor);
        }
        catch (Exception exception) when (exception is DllNotFoundException or
            EntryPointNotFoundException or BadImageFormatException)
        {
            Log.Write($"Display adapter detection unavailable; preserving capture selection: {exception.Message}");
            return -1;
        }
    }

    internal static uint AdapterIndex(string preference)
    {
        try
        {
            int index = preference == "power-saving"
                ? shadowless_get_power_saving_adapter_index()
                : shadowless_get_high_performance_adapter_index();
            if (index >= 0)
            {
                Log.Write($"GPU preference: {preference} adapter index={index}.");
                return (uint)index;
            }

            Log.Write($"GPU preference: {preference} adapter unavailable; using adapter 0.");
        }
        catch (Exception exception)
        {
            Log.Write(
                $"GPU preference {preference} detection failed; using adapter 0: " +
                exception.Message);
        }

        return 0;
    }
}
