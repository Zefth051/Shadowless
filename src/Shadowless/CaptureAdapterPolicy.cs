namespace Shadowless;

internal static class CaptureAdapterPolicy
{
    // OBS method values: Auto=0, DXGI=1, WGC=2. No frame polling or GPU migration.
    internal static long ResolveMonitorMethod(long preferred, uint renderer, int owner, bool wgcReady) =>
        preferred == 1 && owner >= 0 && (uint)owner != renderer
            ? (wgcReady ? 2 : 0)
            : preferred;
}
