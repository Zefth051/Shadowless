using System.Net;
using System.Diagnostics;
using System.Reflection;
using System.Text.Json;
using Shadowless;

// Test transport only: production URL policy and PrepareAsync still validate these assets.
string directory = Path.GetFullPath(args[0]);
string version = args[1];
string resultPath = args[2];
string kind = args[3];
string[] names = [$"Shadowless-{version}-Setup-win-x64.exe", $"Shadowless-{version}-Portable-win-x64.zip", "SHA256SUMS.txt"];
string download = $"{UpdateService.RepositoryUrl}/releases/download/v{version}/";
var files = names.ToDictionary(name => download + name, name => Path.Combine(directory, name));
var transport = new PackageTransport(files, version, names, download);
using var client = new HttpClient(transport);
var updater = new UpdateService(client);
using var deadline = new CancellationTokenSource(TimeSpan.FromMinutes(3));
UpdateRelease release = await updater.CheckAsync(true, deadline.Token) ?? throw new Exception("Release discovery failed.");
PreparedUpdate prepared = await updater.PrepareAsync(release, null, deadline.Token);
if (prepared.Kind.ToString() != kind)
    throw new Exception($"Expected {kind}, got {prepared.Kind}.");
if (kind == "Portable" && !File.Exists(Path.Combine(prepared.PortablePayloadDirectory!, "runtime", "obs.dll")))
    throw new Exception("Prepared package is missing the real OBS payload.");
// An incorrect digest must reject even a previously cached package and leave no partial download.
UpdateAsset asset = kind == "Installer" ? release.SetupAsset : release.PortableAsset;
UpdateAsset invalid = asset with { Digest = "sha256:" + new string('0', 64) };
UpdateRelease badRelease = kind == "Installer" ? release with { SetupAsset = invalid } : release with { PortableAsset = invalid };
try
{
    await updater.PrepareAsync(badRelease, null, deadline.Token);
    throw new Exception("SHA-256 mismatch was accepted.");
}
catch (InvalidDataException e) when (e.Message.Contains("SHA-256 verification")) { }
if (File.Exists(prepared.PackagePath + ".download"))
    throw new Exception("Failed download left a partial file.");
// Re-download the valid package after the rejection so the installer can run it.
prepared = await updater.PrepareAsync(release, null, deadline.Token);
File.WriteAllText(resultPath, JsonSerializer.Serialize(prepared));
if (kind == "Portable" && args.Length == 5)
{
    string target = Path.GetFullPath(args[4]);
    string marker = resultPath + ".relaunch";
    var buildScript = typeof(UpdateService).GetMethod("BuildPortableUpdateScript", BindingFlags.Static | BindingFlags.NonPublic)!;
    string script = (string)buildScript.Invoke(null, [int.MaxValue, prepared.PortablePayloadDirectory!, target,
        Path.Combine(target, "Shadowless.exe"), resultPath + ".log"])!;
    // Observe the real process started by the production helper without changing the package.
    string quotedMarker = "'" + marker.Replace("'", "''") + "'";
    script = "function Start-Process { param([string]$FilePath) $p = Microsoft.PowerShell.Management\\Start-Process -FilePath $FilePath -PassThru; $p.Id | Set-Content -LiteralPath " + quotedMarker + " }\n" + script;
    string scriptPath = resultPath + ".ps1";
    File.WriteAllText(scriptPath, script);
    using var process = Process.Start(new ProcessStartInfo("powershell.exe") {
        UseShellExecute = false, ArgumentList = { "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", scriptPath }
    })!;
    using var helperDeadline = new CancellationTokenSource(TimeSpan.FromSeconds(45));
    try { await process.WaitForExitAsync(helperDeadline.Token); }
    catch (OperationCanceledException) { process.Kill(true); throw new Exception("Portable helper timed out."); }
    if (process.ExitCode != 0 || !File.Exists(marker))
        throw new Exception("Portable helper failed to relaunch the real launcher.");
    Console.WriteLine("PASS: production portable helper copied the real package and started its launcher.");
}
Console.WriteLine($"PASS: {kind} discovery, checksum-file download, real package preparation, cached hash rejection and recovery.");

sealed class PackageTransport(Dictionary<string, string> files, string version, string[] names, string download) : HttpMessageHandler
{
    protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
    {
        string url = request.RequestUri!.AbsoluteUri;
        HttpContent content;
        if (url.StartsWith("https://api.github.com/repos/Zefth051/Shadowless/releases?", StringComparison.Ordinal))
            content = new StringContent(JsonSerializer.Serialize(new[] { new {
                tag_name = "v" + version, html_url = UpdateService.RepositoryUrl + "/releases/tag/v" + version,
                assets = names.Select(name => new { name, browser_download_url = download + name, size = new FileInfo(files[download + name]).Length })
            }}));
        else if (files.TryGetValue(url, out string? path))
            content = new StreamContent(File.OpenRead(path));
        else throw new Exception("Unexpected update URL: " + url);
        return Task.FromResult(new HttpResponseMessage(HttpStatusCode.OK) { Content = content });
    }
}

namespace Shadowless
{
    internal static class AppDistribution { internal static bool IsMicrosoftStore => false; }
}
