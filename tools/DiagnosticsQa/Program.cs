using System.IO.Compression;
using Shadowless;

string root = Path.GetDirectoryName(AppDataPaths.LogFile)!;
Directory.CreateDirectory(root);
try
{
    for (int index = 0; index < 7000; index++)
        Log.Write(new string('x', 2048));
    CrashDiagnostics.Start();
    CrashDiagnostics.Record("QA", new InvalidOperationException("Fixture crash"));
    Log.Write(@"ERROR fixture path=C:\Users\PrivateTester\secret.mp4 token=private-token");
    Log.Write("libobs[info]: private-native-line");
    Log.Flush();
    Check(File.Exists(Log.Path + ".4"), "Log history was not retained: " +
        string.Join(", ", Directory.GetFiles(root).Select(path =>
            $"{Path.GetFileName(path)}={new FileInfo(path).Length}")));
    Check(Directory.GetFiles(root).Length == 5, "Log rotation did not bound file count.");
    Check(Directory.GetFiles(root).All(path =>
        new FileInfo(path).Length < Log.MaxFileBytes + 128 * 1024), "Log rotation exceeded size limit.");

    string destination = Path.Combine(root, "diagnostics.zip");
    File.WriteAllText(destination, "Existing export");
    DiagnosticLogExporter.CreateArchive(destination, Log.Path);
    using (var archive = ZipFile.OpenRead(destination))
    {
        Check(archive.Entries.Count == 6, "Expected README and five logs.");
        string text = string.Join("\n", archive.Entries.Select(entry =>
        {
            using var reader = new StreamReader(entry.Open());
            return reader.ReadToEnd();
        }));
        Check(text.Contains("Fixture crash") && text.Contains("InvalidOperationException") &&
            text.Contains("Session started"), "Crash or session context missing.");
        Check(!text.Contains("PrivateTester") && !text.Contains("secret.mp4") &&
            !text.Contains("private-token") && !text.Contains("private-native-line"),
            "Sensitive fixture values survived export.");
    }
    DiagnosticLogExporter.CreateArchive(Path.Combine(root, "empty.zip"), Path.Combine(root, "missing.log"));
    Console.WriteLine("PASS: bounded log rotation, crash persistence, sanitized ZIP, overwrite, and missing logs.");
}
finally
{
    Log.Close();
    Directory.Delete(root, recursive: true);
}

static void Check(bool condition, string message)
{
    if (!condition)
        throw new InvalidOperationException(message);
}

namespace Shadowless
{
    internal static class AppDataPaths
    {
        internal static readonly string LogFile = Path.Combine(AppContext.BaseDirectory,
            "Shadowless-DiagnosticsQa-" + Guid.NewGuid().ToString("N"), "log.txt");
    }

    internal static class UpdateService
    {
        internal static string CurrentVersionText => "QA";
    }
}
