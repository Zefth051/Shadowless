using System.IO;
using Windows.Storage;

namespace Shadowless;

internal static class AppDataPaths
{
    private static readonly string DesktopCacheRoot = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "Shadowless");
    private static readonly string DesktopStateRoot = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
        "Shadowless");

    private static string StateRoot => AppDistribution.IsMicrosoftStore
        ? ApplicationData.Current.LocalFolder.Path
        : DesktopStateRoot;

    private static string CacheRoot => AppDistribution.IsMicrosoftStore
        ? ApplicationData.Current.LocalCacheFolder.Path
        : DesktopCacheRoot;

    internal static string ConfigFile => Path.Combine(StateRoot, "config.json");
    internal static string LogFile => Path.Combine(CacheRoot, "log.txt");
    internal static string ObsConfigDirectory => Path.Combine(CacheRoot, "obs");
    internal static string ObsPluginCacheDirectory =>
        Path.Combine(CacheRoot, "obs-plugin-cache");
    internal static string ThumbnailDirectory =>
        Path.Combine(CacheRoot, "thumbnails");

    internal static void PrepareData()
    {
        if (!AppDistribution.IsMicrosoftStore)
        {
            CopyPreviousConfig("config.json");
            CopyPreviousConfig("config.json.bak");
            return;
        }

        Directory.CreateDirectory(StateRoot);
        Directory.CreateDirectory(CacheRoot);
        MigrateLegacyFile(
            Path.Combine(DesktopStateRoot, "config.json"),
            ConfigFile);
        MigrateLegacyFile(
            Path.Combine(DesktopStateRoot, "config.json.bak"),
            ConfigFile + ".bak");

        TryDeleteLegacyFile(Path.Combine(DesktopCacheRoot, "log.txt"));
        foreach (string directory in new[]
                 {
                     "obs",
                     "obs-plugin-cache",
                     "thumbnails",
                 })
        {
            TryDeleteLegacyDirectory(Path.Combine(DesktopCacheRoot, directory));
        }

        TryDeleteIfEmpty(DesktopStateRoot);
        TryDeleteIfEmpty(DesktopCacheRoot);
    }

    private static void CopyPreviousConfig(string fileName)
    {
        string source = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "Captail", fileName);
        string destination = Path.Combine(DesktopStateRoot, fileName);
        if (!File.Exists(source) || File.Exists(destination))
            return;

        try
        {
            Directory.CreateDirectory(DesktopStateRoot);
            File.Copy(source, destination);
        }
        catch (IOException)
        {
            // A previous process can briefly retain its settings file.
        }
        catch (UnauthorizedAccessException)
        {
            // Keep legacy data intact when migration is not permitted.
        }
    }

    private static void MigrateLegacyFile(string source, string destination)
    {
        if (!File.Exists(source))
            return;

        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
            if (!File.Exists(destination))
                File.Move(source, destination);
            else
                File.Delete(source);
        }
        catch (IOException)
        {
            // A previous process can briefly retain its settings file.
        }
        catch (UnauthorizedAccessException)
        {
            // Keep legacy data intact when migration is not permitted.
        }
    }

    private static void TryDeleteLegacyFile(string path)
    {
        try
        {
            File.Delete(path);
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
    }

    private static void TryDeleteLegacyDirectory(string path)
    {
        try
        {
            if (Directory.Exists(path))
                Directory.Delete(path, recursive: true);
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
    }

    private static void TryDeleteIfEmpty(string path)
    {
        try
        {
            if (Directory.Exists(path) &&
                !Directory.EnumerateFileSystemEntries(path).Any())
            {
                Directory.Delete(path);
            }
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
    }
}
