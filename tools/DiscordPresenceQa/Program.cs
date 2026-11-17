using System.Buffers.Binary;
using System.Text;
using System.Text.Json;
using System.Threading.Channels;
using Shadowless;

internal static class Program
{
    private static async Task Main()
    {
        Check(DiscordPresence.StateFor(false, true, true) == DiscordActivity.Idle, "Inactive capture must be hidden.");
        Check(DiscordPresence.StateFor(true, false, true) == DiscordActivity.Idle, "Paused capture must be hidden.");
        Check(DiscordPresence.StateFor(true, true, false) == DiscordActivity.Recording, "Recording state missing.");
        Check(DiscordPresence.StateFor(true, true, true) == DiscordActivity.Idle, "Paused recording must be hidden.");
        Check(!DiscordPresence.ShouldPublish(false, true, false, "Rocket League"), "Disabled presence must be hidden.");
        Check(!DiscordPresence.ShouldPublish(true, false, false, "Rocket League"), "Stopped capture must be hidden.");
        Check(!DiscordPresence.ShouldPublish(true, true, true, "Rocket League"), "Paused capture must be hidden.");
        Check(!DiscordPresence.ShouldPublish(true, true, false, "  "), "Unknown games must be hidden.");
        Check(DiscordPresence.ShouldPublish(true, true, false, "Rocket League"), "Recognized capture must be visible.");
        foreach (var state in Enum.GetValues<DiscordActivity>())
        {
            using var payload = JsonDocument.Parse(DiscordPresence.ActivityPayload(new(state, "Rocket League"), 123, "fixture"));
            var root = payload.RootElement;
            Check(root.GetProperty("cmd").GetString() == "SET_ACTIVITY", "Wrong activity command.");
            Check(root.GetProperty("args").GetProperty("pid").GetInt32() == 123, "Process ID missing.");
            var activity = root.GetProperty("args").GetProperty("activity");
            if (state == DiscordActivity.Idle)
            {
                Check(activity.ValueKind == JsonValueKind.Null, "Inactive presence must be cleared.");
                continue;
            }
            Check(activity.EnumerateObject().Select(item => item.Name).Order().SequenceEqual(new[] { "assets", "details", "type" }),
                "Presence must contain only the safe activity fields.");
            Check(activity.GetProperty("details").GetString() == (state switch
            {
                DiscordActivity.Replay => "Clipping",
                DiscordActivity.Recording => "Recording",
                _ => throw new InvalidOperationException(),
            }) + " \"Rocket League\"", "Incorrect safe status.");
        }
        using (var unknown = JsonDocument.Parse(DiscordPresence.ActivityPayload(new(DiscordActivity.Replay), 123, "fixture")))
            Check(unknown.RootElement.GetProperty("args").GetProperty("activity").ValueKind == JsonValueKind.Null,
                "Unrecognized capture must clear presence.");
        using (var wire = new MemoryStream())
        {
            await DiscordPresence.WriteFrameAsync(wire, 3, [1, 2, 3], default);
            Check(wire.ToArray().SequenceEqual(new byte[] { 3, 0, 0, 0, 3, 0, 0, 0, 1, 2, 3 }), "Frame header must contain little-endian opcode and byte length.");
        }
        foreach (int length in new[] { -1, 65537 })
        {
            var header = new byte[8]; BinaryPrimitives.WriteInt32LittleEndian(header, 1);
            BinaryPrimitives.WriteInt32LittleEndian(header.AsSpan(4), length);
            await Throws<InvalidDataException>(async () => { await DiscordPresence.ReadFrameAsync(new MemoryStream(header), default); });
        }
        await Throws<EndOfStreamException>(async () => { await DiscordPresence.ReadFrameAsync(new MemoryStream(new byte[3]), default); });
        var icons = DiscordPresence.GameIconsFromJson(Encoding.UTF8.GetBytes("""
            [{"name":"Rocket League","id":"123456789012345678","icon":"0123456789abcdef0123456789abcdef"},
             {"name":"Invalid","id":"../bad","icon":"0123456789abcdef0123456789abcdef"}]
            """));
        Check(!icons.ContainsKey("Invalid"), "Invalid CDN identifier accepted.");
        var playing = new DiscordStatus(DiscordActivity.Replay, "Rocket League", icons["Rocket League"]);
        using (var payload = JsonDocument.Parse(DiscordPresence.ActivityPayload(playing, 123, "fixture")))
        {
            var activity = payload.RootElement.GetProperty("args").GetProperty("activity");
            Check(activity.GetProperty("details").GetString() == "Clipping \"Rocket League\"", "Game name missing.");
            Check(!activity.TryGetProperty("state", out _) && !activity.TryGetProperty("timestamps", out _), "Clock must be absent.");
            var assets = activity.GetProperty("assets");
            Check(assets.GetProperty("large_image").GetString()!.EndsWith("/linux/app/assets/shadowless-moon.png"), "Shadowless icon missing.");
            Check(assets.GetProperty("small_image").GetString() == "https://cdn.discordapp.com/app-icons/123456789012345678/0123456789abcdef0123456789abcdef.png", "Game badge missing.");
        }
        await CheckSession();
        await CheckImmediateClear();
        Console.WriteLine("DISCORD_PRESENCE_QA PASS (in-memory protocol only; no Discord connection)");
    }

    private static async Task CheckSession()
    {
        using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(15));
        var (client, server) = MemoryDuplex.Pair();
        using (client) using (server)
        {
            int state = (int)DiscordActivity.Replay;
            using var changes = new SemaphoreSlim(0, 1);
            var session = DiscordPresence.RunSessionAsync(client, () => new((DiscordActivity)Volatile.Read(ref state), "Rocket League"), changes, timeout.Token);
            try
            {
                var handshake = await DiscordPresence.ReadFrameAsync(server, timeout.Token);
                Check(handshake.Opcode == 0, "Handshake opcode missing.");
                using (var json = JsonDocument.Parse(handshake.Payload))
                {
                    Check(json.RootElement.GetProperty("client_id").GetString() == "1571906955054948393", "Wrong public application ID.");
                    Check(json.RootElement.GetProperty("v").GetInt32() == 1, "Wrong RPC version.");
                }
                Check(!server.HasBufferedInput, "Activity was sent before READY.");
                byte[] ping = [0, 255, 17, 42];
                await DiscordPresence.WriteFrameAsync(server, 3, ping, timeout.Token);
                var pong = await DiscordPresence.ReadFrameAsync(server, timeout.Token);
                Check(pong.Opcode == 4 && pong.Payload.SequenceEqual(ping), "PING must echo its exact bytes as PONG.");
                await DiscordPresence.WriteFrameAsync(server, 1, Encoding.UTF8.GetBytes("{\"cmd\":\"DISPATCH\",\"evt\":\"READY\"}"), timeout.Token);
                var initial = await DiscordPresence.ReadFrameAsync(server, timeout.Token);
                using (var json = JsonDocument.Parse(initial.Payload))
                    Check(json.RootElement.GetProperty("args").GetProperty("activity").GetProperty("details").GetString() == "Clipping \"Rocket League\"", "Initial activity missing.");
                Volatile.Write(ref state, (int)DiscordActivity.Replay);
                Volatile.Write(ref state, (int)DiscordActivity.Recording);
                changes.Release();
                await Task.Delay(150, timeout.Token);
                Check(!server.HasBufferedInput, "Rapid changes bypassed the update throttle.");
                var latest = await DiscordPresence.ReadFrameAsync(server, timeout.Token);
                using (var json = JsonDocument.Parse(latest.Payload))
                    Check(json.RootElement.GetProperty("args").GetProperty("activity").GetProperty("details").GetString() == "Recording \"Rocket League\"", "Latest state was not coalesced.");
                changes.Release();
                await Task.Delay(5300, timeout.Token);
                Check(!server.HasBufferedInput, "Unchanged activity was published repeatedly.");
                await DiscordPresence.WriteFrameAsync(server, 2, Encoding.UTF8.GetBytes("{}"), timeout.Token);
                await Throws<IOException>(async () => await session);
            }
            finally
            {
                timeout.Cancel();
                try { await session; } catch (IOException) { } catch (OperationCanceledException) { }
            }
        }
        var (blockedClient, blockedServer) = MemoryDuplex.Pair();
        using (blockedClient) using (blockedServer) using (var cancel = new CancellationTokenSource())
        {
            using var changes = new SemaphoreSlim(0, 1);
            var session = DiscordPresence.RunSessionAsync(blockedClient, () => new(DiscordActivity.Replay, "Rocket League"), changes, cancel.Token);
            await DiscordPresence.ReadFrameAsync(blockedServer, default);
            cancel.Cancel();
            await Throws<OperationCanceledException>(async () => await session.WaitAsync(TimeSpan.FromSeconds(1)));
        }
    }

    private static async Task CheckImmediateClear()
    {
        using var cancel = new CancellationTokenSource(TimeSpan.FromSeconds(3));
        using var changes = new SemaphoreSlim(0, 1);
        var (client, server) = MemoryDuplex.Pair();
        using (client) using (server)
        {
            var session = DiscordPresence.RunSessionAsync(client,
                () => new(DiscordActivity.Replay, "Rocket League"), changes, cancel.Token);
            try
            {
                await DiscordPresence.ReadFrameAsync(server, cancel.Token);
                await DiscordPresence.WriteFrameAsync(server, 1,
                    Encoding.UTF8.GetBytes("{\"cmd\":\"DISPATCH\",\"evt\":\"READY\"}"), cancel.Token);
                await DiscordPresence.ReadFrameAsync(server, cancel.Token);
                // Cancel inside the five-second publication window, with an update pending.
                changes.Release();
                cancel.Cancel();
                await Throws<OperationCanceledException>(async () => await session.WaitAsync(TimeSpan.FromSeconds(1)));
                using var clearTimeout = new CancellationTokenSource(TimeSpan.FromSeconds(1));
                var clear = await DiscordPresence.ReadFrameAsync(server, clearTimeout.Token);
                using var json = JsonDocument.Parse(clear.Payload);
                Check(json.RootElement.GetProperty("args").GetProperty("activity").ValueKind == JsonValueKind.Null,
                    "Disconnect must clear presence without waiting for the update throttle.");
            }
            finally
            {
                cancel.Cancel();
                try { await session; } catch (OperationCanceledException) { }
            }
        }
    }

    private static async Task Throws<T>(Func<Task> operation) where T : Exception
    {
        try { await operation(); } catch (T) { return; }
        throw new InvalidOperationException($"Expected {typeof(T).Name}.");
    }
    private static void Check(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }
}

internal sealed class MemoryDuplex(Channel<byte[]> incoming, Channel<byte[]> outgoing) : Stream
{
    private byte[] _current = [];
    private int _offset;
    public bool HasBufferedInput => _offset < _current.Length || incoming.Reader.TryPeek(out _);
    public static (MemoryDuplex, MemoryDuplex) Pair()
    {
        var left = Channel.CreateUnbounded<byte[]>(); var right = Channel.CreateUnbounded<byte[]>();
        return (new(left, right), new(right, left));
    }
    public override async ValueTask<int> ReadAsync(Memory<byte> buffer, CancellationToken cancellationToken = default)
    {
        if (_offset == _current.Length) { _current = await incoming.Reader.ReadAsync(cancellationToken); _offset = 0; }
        int count = Math.Min(3, Math.Min(buffer.Length, _current.Length - _offset));
        _current.AsMemory(_offset, count).CopyTo(buffer); _offset += count; return count;
    }
    public override ValueTask WriteAsync(ReadOnlyMemory<byte> buffer, CancellationToken cancellationToken = default) =>
        outgoing.Writer.WriteAsync(buffer.ToArray(), cancellationToken);
    public override bool CanRead => true;
    public override bool CanWrite => true;
    public override bool CanSeek => false;
    public override long Length => throw new NotSupportedException();
    public override long Position { get => throw new NotSupportedException(); set => throw new NotSupportedException(); }
    public override void Flush() { }
    public override int Read(byte[] buffer, int offset, int count) => throw new NotSupportedException();
    public override void Write(byte[] buffer, int offset, int count) => throw new NotSupportedException();
    public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException();
    public override void SetLength(long value) => throw new NotSupportedException();
}
