using Shadowless;

namespace ProcessAudioFoundationQa;

internal static class GameIdentityQa
{
    internal static void Run()
    {
        var identity = new GameIdentity(new (string, string)[]
        {
            ("game.exe", "Coloring Game"),
            ("game.exe", "Coloring Game 2"),
            ("abzugame-win64-shipping.exe", "ABZU"),
            ("ABZUGAME-WIN64-SHIPPING.EXE", "ABZU")
        });
        const string steam = @"D:\Library\steamapps\common\Coloring Game 2\bin\game.exe";
        Check("ABZUGAME-WIN64-SHIPPING", null, null, "ABZU");
        Check("GAME.EXE", steam, null, "Coloring Game 2");
        Check("game", steam, "coloring game 2", "Coloring Game 2");
        Check("game", steam, "Coloring Game", null);
        Check("game", "C:/Epic Games/Coloring Game/game.exe", null, "Coloring Game");
        Check("game", @"D:\GOG Games\Coloring Game 2\game.exe", null, "Coloring Game 2");
        Check("game", @"D:\XboxGames\Coloring Game\Content\game.exe", null, "Coloring Game");
        Check("game", @"D:\XboxGames\Coloring Game\game.exe", null, null);
        Check("game", null, " coloring game ", "Coloring Game");
        Check("game", null, "Coloring Game 20", null);
        Check("game", null, "ABZU", null);
        Check("game", null, null, null);
        Check("game", @"D:\Users\Coloring Game\game.exe", null, null);
        Check("game", @"D:\steamapps\common\CodeName\Coloring Game\game.exe", null, null);
        Check("game", @"D:\Epic Games\Coloring Game\..\Other\game.exe", null, null);
        Check("game", @"D:\Epic Games\Coloring Game\other.exe", "Coloring Game", null);
        Check("unknown", steam, "Coloring Game", null);
        Check(null, steam, "Coloring Game", null);

        void Check(string? executable, string? path, string? product, string? expected)
        {
            GameIdentity.Result result = identity.Resolve(executable, path, product);
            if (result.Title != expected)
                throw new InvalidOperationException(
                    $"Identity for ({executable}, {path}, {product}): expected {expected ?? "Ambiguous"}, got {result.Title ?? "Ambiguous"}.");
        }
    }
}
