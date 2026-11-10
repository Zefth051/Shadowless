using System.Runtime.InteropServices;

namespace Shadowless;

internal static class ObsLogBridge
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void LogCallback(
        int level,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string message);

    private static readonly LogCallback Callback = Write;

    [DllImport("ShadowlessObsBridge.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool shadowless_install_obs_log_handler(LogCallback callback);

    [DllImport("ShadowlessObsBridge.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void shadowless_remove_obs_log_handler();

    internal static bool Install()
    {
        bool installed = shadowless_install_obs_log_handler(Callback);
        if (!installed)
            Log.Write("OBS log bridge could not be installed.");
        return installed;
    }

    internal static void Remove() => shadowless_remove_obs_log_handler();

    private static void Write(int level, string message) =>
        Log.Write($"libobs[{level}]: {message}");
}
