using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using Windows.ApplicationModel.Activation;

namespace Shadowless;

internal static class AppDistribution
{
#if MICROSOFT_STORE
    internal static bool IsMicrosoftStore { get; } = true;
#else
    internal static bool IsMicrosoftStore { get; } = HasPackageIdentity();
#endif

    internal static string? DesktopLauncherPath
    {
        get
        {
            string? executable = Environment.ProcessPath;
            if (executable is null)
                return null;

            DirectoryInfo? directory = Directory.GetParent(executable);
            if (directory?.Parent is not { } root ||
                !directory.Name.Equals("runtime", StringComparison.OrdinalIgnoreCase))
            {
                return executable;
            }

            string launcher = Path.Combine(root.FullName, Path.GetFileName(executable));
            return File.Exists(launcher) ? launcher : executable;
        }
    }

    internal static bool IsStartupTaskActivation()
    {
        if (!IsMicrosoftStore)
            return false;

        try
        {
            return Windows.ApplicationModel.AppInstance
                .GetActivatedEventArgs()?.Kind == ActivationKind.StartupTask;
        }
        catch (Exception exception)
        {
            Log.Write($"Could not read package activation: {exception.Message}");
            return false;
        }
    }

    private static bool HasPackageIdentity()
    {
        int length = 0;
        return GetCurrentPackageFullName(ref length, null) ==
            ErrorInsufficientBuffer;
    }

    private const int ErrorInsufficientBuffer = 122;

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetCurrentPackageFullName(
        ref int packageFullNameLength,
        StringBuilder? packageFullName);
}
