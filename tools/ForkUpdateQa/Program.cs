using Shadowless;

if (UpdateService.SelfUpdateEnabled)
    throw new Exception("Fork self-updates must remain disabled.");

var service = new UpdateService();
foreach (bool force in new[] { false, true })
{
    if (await service.CheckAsync(force, CancellationToken.None) is not null)
        throw new Exception("Disabled checks must return no release.");
}

// Null payloads prove the guard runs before any download or process launch.
await RequireBlockedAsync(() => service.PrepareAsync(null!, null, CancellationToken.None));
await RequireBlockedAsync(() =>
{
    UpdateService.Launch(null!);
    return Task.CompletedTask;
});
Console.WriteLine("PASS: Fork checks, downloads, and update launches are disabled.");

static async Task RequireBlockedAsync(Func<Task> action)
{
    try
    {
        await action();
    }
    catch (InvalidOperationException exception)
        when (exception.Message == "Self-updates are disabled in this fork.")
    {
        return;
    }
    throw new Exception("The fork update guard did not block the operation.");
}

namespace Shadowless
{
    internal static class AppDistribution
    {
        internal static bool IsMicrosoftStore =>
            throw new Exception("Fork checks must stop before distribution detection.");
    }
}
