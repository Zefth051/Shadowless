using Shadowless;

var cases = new (string Name, long Preferred, uint Renderer, int Owner, bool WgcReady, long Expected)[]
{
    ("Single GPU compatibility", 1, 0, 0, true, 1),
    ("External display on discrete GPU", 1, 1, 1, false, 1),
    ("Hybrid internal display with preflight", 1, 1, 0, true, 2),
    ("Hybrid internal display without borderless permission", 1, 1, 0, false, 0),
    ("Integrated renderer with discrete display", 1, 0, 1, true, 2),
    ("Nonadjacent adapter indices", 1, 3, 1, false, 0),
    ("Old bridge or disconnected monitor", 1, 1, -1, true, 1),
    ("Unknown owner without permission", 1, 0, -1, false, 1),
    ("OBS Auto retained", 0, 1, 0, true, 0),
    ("WGC retained", 2, 1, 0, false, 2),
};
foreach (var test in cases)
{
    long actual = CaptureAdapterPolicy.ResolveMonitorMethod(
        test.Preferred, test.Renderer, test.Owner, test.WgcReady);
    if (actual != test.Expected)
        throw new Exception($"{test.Name}: {actual} != {test.Expected}");
}
Console.WriteLine($"CAPTURE_SELECTION PASS ({cases.Length} cases; no Windows APIs or GPU required)");
