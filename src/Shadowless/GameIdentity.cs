using System.IO;

namespace Shadowless;

internal sealed class GameIdentity
{
    internal readonly record struct Result(string? Title)
    {
        internal static Result Ambiguous => new(null);
    }

    private static readonly StringComparer Names = StringComparer.OrdinalIgnoreCase;
    private readonly ILookup<string, string> titles;

    internal GameIdentity(IEnumerable<(string Executable, string Title)> catalog)
    {
        titles = catalog.Where(row => !string.IsNullOrWhiteSpace(row.Title))
            .ToLookup(row => Config.NormalizeExecutableName(row.Executable),
                row => row.Title.Trim(), Names);
    }

    internal Result Resolve(string? executable, string? fullPath = null, string? productName = null)
    {
        string key = Config.NormalizeExecutableName(executable);
        if (key.Length == 0)
            return Result.Ambiguous;

        string[] candidates = titles[key].Distinct(Names).ToArray();
        if (candidates.Length == 1)
            return new Result(candidates[0]);

        if (fullPath != null && !Names.Equals(Path.GetFileName(fullPath), key))
            return Result.Ambiguous;

        string? folder = InstallFolder(fullPath);
        string? match = null;
        foreach (string title in candidates)
        {
            if (!Names.Equals(title, folder) && !Names.Equals(title, productName?.Trim()))
                continue;
            if (match != null)
                return Result.Ambiguous;
            match = title;
        }
        return new Result(match);
    }

    private static string? InstallFolder(string? path)
    {
        if (string.IsNullOrEmpty(path))
            return null;
        string[] parts = path.Replace('/', '\\').Split('\\');
        // Limit the prototype to ordinary absolute drive paths.
        if (parts.Length < 4 || parts[0].Length != 2 || !char.IsAsciiLetter(parts[0][0])
            || parts[0][1] != ':' || parts.Any(p => p.Length == 0 || p == "." || p == ".."))
            return null;

        for (int i = 1; i < parts.Length - 2; i++)
        {
            if (Names.Equals(parts[i], "steamapps") && i + 3 < parts.Length
                && Names.Equals(parts[i + 1], "common"))
                return parts[i + 2];
            if (Names.Equals(parts[i], "Epic Games") || Names.Equals(parts[i], "GOG Games"))
                return parts[i + 1];
            if (Names.Equals(parts[i], "XboxGames") && i + 3 < parts.Length
                && Names.Equals(parts[i + 2], "Content"))
                return parts[i + 1];
        }
        return null;
    }
}
