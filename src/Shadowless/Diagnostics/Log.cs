using System.IO;

namespace Shadowless;

public static class Log
{
    private static readonly Lock _lock = new();
    private static StreamWriter? _writer;
    private static int _pendingLines;
    internal const long MaxFileBytes = 2 * 1024 * 1024;
    internal const int ArchiveCount = 4;
    public static readonly string Path = AppDataPaths.LogFile;

    static Log() => AppDomain.CurrentDomain.ProcessExit += (_, _) => Close();

    public static void Write(string message)
    {
        lock (_lock)
        {
            try
            {
                _writer ??= CreateWriter();
                if (_writer.BaseStream.Position >= MaxFileBytes)
                {
                    _writer.Dispose();
                    _writer = null;
                    Rotate();
                    _writer = CreateWriter();
                    _pendingLines = 0;
                }
                _writer.WriteLine($"{DateTime.Now:HH:mm:ss.fff} {message}");
                _pendingLines++;
                if (_pendingLines >= 32 || IsUrgent(message))
                {
                    _writer.Flush();
                    _pendingLines = 0;
                }
            }
            catch
            {
                _writer?.Dispose();
                _writer = null;
                _pendingLines = 0;
            }
        }
    }

    public static void Close()
    {
        lock (_lock)
        {
            _writer?.Dispose();
            _writer = null;
            _pendingLines = 0;
        }
    }

    internal static void Flush()
    {
        lock (_lock)
        {
            _writer?.Flush();
            _pendingLines = 0;
        }
    }

    private static StreamWriter CreateWriter()
    {
        Directory.CreateDirectory(System.IO.Path.GetDirectoryName(Path)!);
        return new StreamWriter(new FileStream(
            Path,
            FileMode.Append,
            FileAccess.Write,
            FileShare.ReadWrite,
            16 * 1024,
            FileOptions.SequentialScan))
        {
            AutoFlush = false,
        };
    }

    private static void Rotate()
    {
        File.Delete(Path + "." + ArchiveCount);
        for (int index = ArchiveCount - 1; index >= 1; index--)
        {
            if (File.Exists(Path + "." + index))
                File.Move(Path + "." + index, Path + "." + (index + 1));
        }
        if (File.Exists(Path))
            File.Move(Path, Path + ".1");
    }

    private static bool IsUrgent(string message) =>
        message.Contains("error", StringComparison.OrdinalIgnoreCase) ||
        message.Contains("fail", StringComparison.OrdinalIgnoreCase) ||
        message.Contains("exception", StringComparison.OrdinalIgnoreCase) ||
        message.Contains("crash", StringComparison.OrdinalIgnoreCase);
}
