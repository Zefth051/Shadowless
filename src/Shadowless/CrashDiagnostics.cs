using System.Runtime.InteropServices;

namespace Shadowless;

internal static class CrashDiagnostics
{
    internal static void Start()
    {
        AppDomain.CurrentDomain.UnhandledException += (_, args) =>
            Record("Unhandled exception", args.ExceptionObject as Exception);
        TaskScheduler.UnobservedTaskException += (_, args) =>
            Record("Unobserved task exception", args.Exception);
        Log.Write($"Session started: UTC={DateTime.UtcNow:O}, " +
            $"version={UpdateService.CurrentVersionText}, " +
            $"OS={RuntimeInformation.OSDescription}, runtime={RuntimeInformation.FrameworkDescription}, " +
            $"architecture={RuntimeInformation.ProcessArchitecture}");
    }

    internal static void Record(string source, Exception? exception)
    {
        Log.Write($"ERROR {source}: UTC={DateTime.UtcNow:O}, " +
            $"version={UpdateService.CurrentVersionText}{Environment.NewLine}{exception}");
    }
}
