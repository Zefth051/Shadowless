using System.Buffers.Binary;
using System.Diagnostics;
using System.IO;
using System.IO.Pipes;
using System.Text.Json;
using System.Net.Http;
using System.Text.RegularExpressions;

namespace Shadowless;

internal enum DiscordActivity { Idle, Replay, Recording }
internal sealed record DiscordStatus(DiscordActivity State, string Game = "", string Icon = "");

internal sealed class DiscordPresence : IDisposable
{
    internal const string ApplicationId = "1571906955054948393";
    private const int MaximumPayloadBytes = 65536;
    private readonly CancellationTokenSource _shutdown = new();
    private readonly SemaphoreSlim _changes = new(0, 1);
    // Finish clearing the old session before a rapid re-enable can publish a new one.
    private static readonly SemaphoreSlim ConnectionGate = new(1, 1);
    private DiscordStatus _status = new(DiscordActivity.Idle);
    private int _disposed;
    private int _iconsRequested;
    private IReadOnlyDictionary<string, string> _icons = new Dictionary<string, string>();
    private static readonly HttpClient IconClient = new() { Timeout = TimeSpan.FromSeconds(10) };
    private const string GameIconSource = "https://raw.githubusercontent.com/markterence/discord-quest-completer/53191138e647304dc7dce6a6ace77ae19f8309c3/src/assets/gamelist.json";
    private const string ShadowlessIcon = "https://raw.githubusercontent.com/Zefth051/Shadowless/1af4b46fece262eac5446581db52d5f8dee14756/linux/app/assets/shadowless-moon.png";

    public DiscordPresence(string game, bool recording)
    {
        Update(true, recording, false, game);
        _ = Task.Run(ConnectLoopAsync);
    }

    public void Update(bool active, bool recording, bool paused, string? game)
    {
        if (Volatile.Read(ref _disposed) != 0) return;
        if (!ShouldPublish(true, active, paused, game))
        {
            Dispose();
            return;
        }
        string title = active ? (game ?? "").Trim() : "";
        if (title.Length > 100) title = title[..100];
        var icons = Volatile.Read(ref _icons);
        string icon = icons.TryGetValue(title, out var found) ? found : "";
        var latest = new DiscordStatus(StateFor(active, recording, paused), title, icon);
        if (Interlocked.Exchange(ref _status, latest) != latest) SignalChange();
        if (title.Length > 0 && Interlocked.Exchange(ref _iconsRequested, 1) == 0)
            _ = LoadGameIconsAsync(_shutdown.Token);
    }

    internal static IReadOnlyDictionary<string, string> GameIconsFromJson(byte[] bytes)
    {
        using var document = JsonDocument.Parse(bytes);
        var icons = new Dictionary<string, string>();
        if (document.RootElement.ValueKind != JsonValueKind.Array) return icons;
        foreach (var game in document.RootElement.EnumerateArray())
        {
            if (game.ValueKind != JsonValueKind.Object ||
                !game.TryGetProperty("name", out var name) || name.ValueKind != JsonValueKind.String ||
                !game.TryGetProperty("id", out var id) || id.ValueKind != JsonValueKind.String ||
                !game.TryGetProperty("icon", out var hash) || hash.ValueKind != JsonValueKind.String) continue;
            string title = name.GetString()!.Trim(), identifier = id.GetString()!, icon = hash.GetString()!;
            if (title.Length > 100) title = title[..100];
            if (title.Length == 0 || !Regex.IsMatch(identifier, @"\A[0-9]{16,22}\z") ||
                !Regex.IsMatch(icon, @"\A(a_)?[a-f0-9]{32}\z")) continue;
            string url = $"https://cdn.discordapp.com/app-icons/{identifier}/{icon}.png";
            if (!icons.TryGetValue(title, out var previous)) icons[title] = url;
            else if (previous != url) icons[title] = "";
        }
        return icons;
    }

    private async Task LoadGameIconsAsync(CancellationToken shutdownToken)
    {
        using var deadline = CancellationTokenSource.CreateLinkedTokenSource(shutdownToken);
        deadline.CancelAfter(TimeSpan.FromSeconds(10));
        var token = deadline.Token;
        try
        {
            using var response = await IconClient.GetAsync(GameIconSource, HttpCompletionOption.ResponseHeadersRead, token).ConfigureAwait(false);
            response.EnsureSuccessStatusCode();
            const int limit = 16 * 1024 * 1024;
            if (response.Content.Headers.ContentLength > limit) return;
            await using var source = await response.Content.ReadAsStreamAsync(token).ConfigureAwait(false);
            using var bytes = new MemoryStream();
            var buffer = new byte[65536];
            int count;
            while ((count = await source.ReadAsync(buffer, token).ConfigureAwait(false)) > 0)
            {
                if (bytes.Length + count > limit) return;
                bytes.Write(buffer, 0, count);
            }
            Volatile.Write(ref _icons, GameIconsFromJson(bytes.ToArray()));
            SignalChange();
        }
        catch (Exception) { /* Optional artwork must never interrupt capture. */ }
    }

    internal static DiscordActivity StateFor(bool active, bool recording, bool paused) =>
        !active || paused ? DiscordActivity.Idle : recording ? DiscordActivity.Recording : DiscordActivity.Replay;

    internal static bool ShouldPublish(bool enabled, bool active, bool paused, string? game) =>
        enabled && active && !paused && !string.IsNullOrWhiteSpace(game);

    private void SignalChange()
    {
        try { _changes.Release(); } catch (SemaphoreFullException) { }
    }

    private async Task ConnectLoopAsync()
    {
        var token = _shutdown.Token;
        int retrySeconds = 5;
        bool ownsConnection = false;
        try
        {
            await ConnectionGate.WaitAsync(token).ConfigureAwait(false);
            ownsConnection = true;
            while (!token.IsCancellationRequested)
            {
                try
                {
                    using var pipe = await ConnectAsync(token).ConfigureAwait(false);
                    if (pipe is not null)
                    {
                        retrySeconds = 5;
                        await RunSessionAsync(pipe, CurrentStatus, _changes, token).ConfigureAwait(false);
                    }
                }
                catch (Exception) when (!token.IsCancellationRequested)
                {
                    // Discord is optional; a missing or restarted client must not affect capture.
                }
                await Task.Delay(TimeSpan.FromSeconds(retrySeconds), token).ConfigureAwait(false);
                retrySeconds = Math.Min(30, retrySeconds * 2);
            }
        }
        catch (OperationCanceledException) { }
        catch (Exception)
        {
            // Keep optional background work from becoming an unobserved task failure.
        }
        finally
        {
            if (ownsConnection) ConnectionGate.Release();
            _shutdown.Dispose();
        }
    }

    private DiscordStatus CurrentStatus()
    {
        var status = Volatile.Read(ref _status);
        return Volatile.Read(ref _icons).TryGetValue(status.Game, out var icon)
            ? status with { Icon = icon } : status;
    }

    private static async Task<NamedPipeClientStream?> ConnectAsync(CancellationToken token)
    {
        for (int index = 0; index < 10; index++)
        {
            var pipe = new NamedPipeClientStream(".", $"discord-ipc-{index}", PipeDirection.InOut, PipeOptions.Asynchronous);
            try
            {
                await pipe.ConnectAsync(150, token).ConfigureAwait(false);
                return pipe;
            }
            catch (Exception exception) when (exception is TimeoutException or IOException or UnauthorizedAccessException)
            {
                pipe.Dispose();
            }
            catch
            {
                pipe.Dispose();
                throw;
            }
        }
        return null;
    }

    internal static async Task RunSessionAsync(Stream pipe, Func<DiscordStatus> currentState, SemaphoreSlim changes, CancellationToken cancellationToken)
    {
        using var session = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        var token = session.Token;
        Task<(int Opcode, byte[] Payload)>? read = null;
        Task? change = null;
        bool ready = false;
        try
        {
            await WriteFrameAsync(pipe, 0, JsonSerializer.SerializeToUtf8Bytes(new { v = 1, client_id = ApplicationId }), token).ConfigureAwait(false);
            using (var readyTimeout = CancellationTokenSource.CreateLinkedTokenSource(token))
            {
                readyTimeout.CancelAfter(TimeSpan.FromSeconds(5));
                while (!ready)
                {
                    var frame = await ReadFrameAsync(pipe, readyTimeout.Token).ConfigureAwait(false);
                    ready = await HandleFrameAsync(pipe, frame, readyTimeout.Token).ConfigureAwait(false);
                }
            }
            var sent = currentState();
            await WriteFrameAsync(pipe, 1, ActivityPayload(sent, Environment.ProcessId, Guid.NewGuid().ToString("N")), token).ConfigureAwait(false);
            long sentAt = Stopwatch.GetTimestamp();
            read = ReadFrameAsync(pipe, token);
            change = WaitForChangeAsync(changes, sentAt, token);
            while (true)
            {
                await Task.WhenAny(read, change).ConfigureAwait(false);
                if (read.IsCompleted)
                {
                    await HandleFrameAsync(pipe, await read.ConfigureAwait(false), token).ConfigureAwait(false);
                    read = ReadFrameAsync(pipe, token);
                }
                if (!change.IsCompleted) continue;
                await change.ConfigureAwait(false);
                var latest = currentState();
                // Coalesce rapid capture changes and publish at most once per five seconds.
                if (latest != sent)
                {
                    await WriteFrameAsync(pipe, 1, ActivityPayload(latest, Environment.ProcessId, Guid.NewGuid().ToString("N")), token).ConfigureAwait(false);
                    sent = latest; sentAt = Stopwatch.GetTimestamp();
                }
                change = WaitForChangeAsync(changes, sentAt, token);
            }
        }
        finally
        {
            session.Cancel();
            await ObserveAsync(read).ConfigureAwait(false);
            await ObserveAsync(change).ConfigureAwait(false);
            if (ready)
            {
                try
                {
                    // Clearing must bypass both the update throttle and the canceled session token.
                    await WriteFrameAsync(pipe, 1, ActivityPayload(new(DiscordActivity.Idle),
                        Environment.ProcessId, Guid.NewGuid().ToString("N")), CancellationToken.None).ConfigureAwait(false);
                }
                catch (Exception) { /* Disconnect also removes activity when the client is unavailable. */ }
            }
        }
    }

    private static async Task WaitForChangeAsync(SemaphoreSlim changes, long sentAt, CancellationToken token)
    {
        await changes.WaitAsync(token).ConfigureAwait(false);
        var remaining = TimeSpan.FromSeconds(5) - Stopwatch.GetElapsedTime(sentAt);
        if (remaining > TimeSpan.Zero) await Task.Delay(remaining, token).ConfigureAwait(false);
        while (changes.Wait(0)) { }
    }

    private static async Task ObserveAsync(Task? pending)
    {
        if (pending is null) return;
        try { await pending.ConfigureAwait(false); } catch (Exception) { }
    }

    private static async Task<bool> HandleFrameAsync(Stream pipe, (int Opcode, byte[] Payload) frame, CancellationToken token)
    {
        if (frame.Opcode == 2) throw new IOException("Discord closed the IPC connection.");
        if (frame.Opcode == 3)
        {
            await WriteFrameAsync(pipe, 4, frame.Payload, token).ConfigureAwait(false);
            return false;
        }
        if (frame.Opcode == 4) return false;
        if (frame.Opcode != 1) throw new InvalidDataException("Unexpected Discord IPC opcode.");
        using var json = JsonDocument.Parse(frame.Payload);
        var root = json.RootElement;
        if (root.ValueKind != JsonValueKind.Object) throw new InvalidDataException("Invalid Discord RPC event.");
        string? eventName = root.TryGetProperty("evt", out var evt) ? evt.GetString() : null;
        if (eventName == "ERROR") throw new IOException("Discord rejected the RPC activity.");
        return eventName == "READY" && root.TryGetProperty("cmd", out var command) && command.GetString() == "DISPATCH";
    }

    internal static byte[] ActivityPayload(DiscordStatus status, int pid, string nonce)
    {
        if (status.State == DiscordActivity.Idle || string.IsNullOrWhiteSpace(status.Game))
            return JsonSerializer.SerializeToUtf8Bytes(new { cmd = "SET_ACTIVITY", args = new { pid, activity = (object?)null }, nonce });
        string details = status.State switch
        {
            DiscordActivity.Replay => "Clipping",
            DiscordActivity.Recording => "Recording",
            _ => throw new ArgumentOutOfRangeException(nameof(status)),
        };
        if (status.Game.Length > 0) details += $" \"{status.Game}\"";
        var assets = new Dictionary<string, string> { ["large_image"] = ShadowlessIcon, ["large_text"] = "Shadowless" };
        if (status.Icon.Length > 0 && status.Game.Length > 0)
        {
            assets["small_image"] = status.Icon;
            assets["small_text"] = status.Game;
        }
        var activity = new { type = 0, details, assets };
        return JsonSerializer.SerializeToUtf8Bytes(new { cmd = "SET_ACTIVITY", args = new { pid, activity }, nonce });
    }

    internal static async Task<(int Opcode, byte[] Payload)> ReadFrameAsync(Stream stream, CancellationToken token)
    {
        var header = new byte[8];
        await stream.ReadExactlyAsync(header, token).ConfigureAwait(false);
        int opcode = BinaryPrimitives.ReadInt32LittleEndian(header);
        int length = BinaryPrimitives.ReadInt32LittleEndian(header.AsSpan(4));
        if (length < 0 || length > MaximumPayloadBytes) throw new InvalidDataException("Invalid Discord IPC frame length.");
        var payload = new byte[length];
        await stream.ReadExactlyAsync(payload, token).ConfigureAwait(false);
        return (opcode, payload);
    }

    internal static async Task WriteFrameAsync(Stream stream, int opcode, byte[] payload, CancellationToken token)
    {
        if (payload.Length > MaximumPayloadBytes) throw new InvalidDataException("Discord IPC payload is too large.");
        var frame = new byte[8 + payload.Length];
        BinaryPrimitives.WriteInt32LittleEndian(frame, opcode);
        BinaryPrimitives.WriteInt32LittleEndian(frame.AsSpan(4), payload.Length);
        payload.CopyTo(frame, 8);
        using var timeout = CancellationTokenSource.CreateLinkedTokenSource(token);
        timeout.CancelAfter(TimeSpan.FromSeconds(3));
        await stream.WriteAsync(frame, timeout.Token).ConfigureAwait(false);
    }

    public void Dispose()
    {
        if (Interlocked.Exchange(ref _disposed, 1) != 0) return;
        try { _shutdown.Cancel(); } catch (ObjectDisposedException) { }
    }
}
