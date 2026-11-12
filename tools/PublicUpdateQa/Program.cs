using System.Reflection;
using System.IO.Compression;
using System.Net;
using System.Diagnostics;
using System.Text.Json;
using Shadowless;

if (!UpdateService.SelfUpdateEnabled)
    throw new Exception("Public release builds must enable the updater.");

string root = Path.Combine(Path.GetTempPath(), "shadowless update qa's-" + Guid.NewGuid());
Directory.CreateDirectory(root);
try
{
    Directory.CreateDirectory(Path.Combine(root, "runtime"));
    File.WriteAllText(Path.Combine(root, "Shadowless.exe"), "launcher");
    File.WriteAllText(Path.Combine(root, "runtime", "Shadowless.exe"), "app");
    var resolver = typeof(UpdateService).GetMethod("GetPackageDirectory", BindingFlags.Static | BindingFlags.NonPublic)
        ?? throw new Exception("Package directory resolver is missing.");
    foreach (string directory in new[] { root, Path.Combine(root, "runtime") })
        if ((string?)resolver.Invoke(null, new object[] { directory }) != root)
            throw new Exception("An organized update must replace the package root.");
    var flat = Path.Combine(root, "flat");
    Directory.CreateDirectory(flat);
    if ((string?)resolver.Invoke(null, new object[] { flat }) != flat)
        throw new Exception("Flat builds must retain their application directory.");

    var releaseType = typeof(UpdateService).GetNestedType("GitHubRelease", BindingFlags.NonPublic)!;
    var mapper = typeof(UpdateService).GetMethod("TryMapRelease", BindingFlags.Static | BindingFlags.NonPublic)!;
    string ReleaseJson(string repository, string tag = "v9.0.0", bool draft = false, bool prerelease = false) => JsonSerializer.Serialize(new
    {
        tag_name = tag,
        draft,
        prerelease,
        html_url = repository + "/releases/tag/" + tag,
        assets = new[]
        {
            new { name = "Shadowless-9.0.0-Setup-win-x64.exe", browser_download_url = repository + "/releases/download/" + tag + "/Shadowless-9.0.0-Setup-win-x64.exe", size = 123 },
            new { name = "Shadowless-9.0.0-Portable-win-x64.zip", browser_download_url = repository + "/releases/download/" + tag + "/Shadowless-9.0.0-Portable-win-x64.zip", size = 456 }
        }
    });
    object Release(string repository, string tag = "v9.0.0") => JsonSerializer.Deserialize(ReleaseJson(repository, tag), releaseType)!;
    if (mapper.Invoke(null, new[] { Release(UpdateService.RepositoryUrl) }) is null)
        throw new Exception("A Windows release with both packages must be accepted.");
    foreach (string repository in new[] { "https://github.com/other/project", "https://github.com.evil.test/Zefth051/Shadowless", "http://github.com/Zefth051/Shadowless", "https://" + "user:pass@github.com/Zefth051/Shadowless" })
        if (mapper.Invoke(null, new[] { Release(repository) }) is not null)
            throw new Exception("Foreign or credential-bearing release URLs must be rejected.");
    if (mapper.Invoke(null, new[] { Release(UpdateService.RepositoryUrl, "linux-v9.0.0") }) is not null)
        throw new Exception("Linux releases must not enter the Windows updater.");

    var extract = typeof(UpdateService).GetMethod("ExtractPortablePackage", BindingFlags.Static | BindingFlags.NonPublic)!;
    string archivePath = Path.Combine(root, "payload.zip");
    using (var zip = ZipFile.Open(archivePath, ZipArchiveMode.Create))
    using (var writer = new StreamWriter(zip.CreateEntry("../outside.txt").Open()))
        writer.Write("unsafe");
    string extracted = Path.Combine(root, "extracted");
    Directory.CreateDirectory(extracted);
    try
    {
        extract.Invoke(null, new object[] { archivePath, extracted });
        throw new Exception("Archive traversal was accepted.");
    }
    catch (TargetInvocationException e) when (e.InnerException is InvalidDataException) { }
    if (File.Exists(Path.Combine(root, "outside.txt")))
        throw new Exception("Archive traversal wrote outside its destination.");

    using (var client = new HttpClient(new ReleasePages(
        "[" + ReleaseJson(UpdateService.RepositoryUrl, prerelease: true) + "]", "[]")))
    {
        if (await new UpdateService(client).CheckAsync(true, CancellationToken.None) is not null)
            throw new Exception("Prereleases must never be offered as Windows updates, including on 0.x builds.");
    }

    using (var client = new HttpClient(new ReleasePages(
        "[" + ReleaseJson(UpdateService.RepositoryUrl, draft: true) + "]", "[]")))
    {
        if (await new UpdateService(client).CheckAsync(true, CancellationToken.None) is not null)
            throw new Exception("Draft releases must never be offered as Windows updates.");
    }

    using (var client = new HttpClient(new ReleasePages(
        "[" + ReleaseJson(UpdateService.RepositoryUrl, "v10.0.0", prerelease: true) + "," + string.Join(",", Enumerable.Range(0, 10).Select(i => ReleaseJson(UpdateService.RepositoryUrl, $"linux-v9.0.{i}"))) + "]",
        "[" + ReleaseJson(UpdateService.RepositoryUrl) + "]")))
    {
        var found = await new UpdateService(client).CheckAsync(true, CancellationToken.None);
        if (found?.Version != new Version(9, 0, 0))
            throw new Exception("Linux releases hid a Windows update on the next API page.");
    }

    string payload = Path.Combine(root, "payload");
    string target = Path.Combine(root, "target");
    Directory.CreateDirectory(payload);
    Directory.CreateDirectory(target);
    File.WriteAllText(Path.Combine(payload, "Shadowless.exe"), "updated");
    File.WriteAllText(Path.Combine(target, "Shadowless.exe"), "original");
    var buildScript = typeof(UpdateService).GetMethod("BuildPortableUpdateScript", BindingFlags.Static | BindingFlags.NonPublic)!;
    string script = (string)buildScript.Invoke(null, new object[] { int.MaxValue, payload, target,
        OperatingSystem.IsWindows() ? Path.Combine(Environment.SystemDirectory, "whoami.exe") : "/usr/bin/true", Path.Combine(root, "update.log") })!;
    // Simulate one sharing violation, then exercise real filesystem copying.
    string scriptPath = Path.Combine(root, "update.ps1");
    File.WriteAllText(scriptPath, """
        $script:copyAttempts = 0
        function Copy-Item {
          [CmdletBinding()]
          param([Parameter(ValueFromPipeline=$true)]$InputObject, [string]$Destination, [switch]$Recurse, [switch]$Force)
          process {
            if ($script:copyAttempts++ -eq 0) { throw [IO.IOException]::new('Sharing violation fixture') }
            $InputObject | Microsoft.PowerShell.Management\Copy-Item -Destination $Destination -Recurse:$Recurse -Force:$Force
          }
        }
        """ + "\n" + script);
    using var launcherLock = OperatingSystem.IsWindows()
        ? File.Open(Path.Combine(target, "Shadowless.exe"), FileMode.Open, FileAccess.Read, FileShare.Read)
        : null;
    var start = new ProcessStartInfo(args.FirstOrDefault() ?? "powershell.exe") { UseShellExecute = false, CreateNoWindow = true };
    foreach (string arg in new[] { "-NoProfile", "-NonInteractive", "-File", scriptPath }) start.ArgumentList.Add(arg);
    using (var helper = Process.Start(start)!)
    {
        await Task.Delay(750);
        launcherLock?.Dispose();
        using var deadline = new CancellationTokenSource(TimeSpan.FromSeconds(10));
        try { await helper.WaitForExitAsync(deadline.Token); }
        catch (OperationCanceledException) { helper.Kill(entireProcessTree: true); throw new Exception("Portable update helper did not finish."); }
        if (helper.ExitCode != 0 || File.ReadAllText(Path.Combine(target, "Shadowless.exe")) != "updated")
            throw new Exception("Portable update failed to recover from a launcher sharing violation.");
    }

    AppDistribution.IsMicrosoftStore = true;
    if (await new UpdateService().CheckAsync(true, CancellationToken.None) is not null)
        throw new Exception("Store builds must not check GitHub.");
    try
    {
        await new UpdateService().PrepareAsync(null!, null, CancellationToken.None);
        throw new Exception("Store update installation was allowed.");
    }
    catch (InvalidOperationException e) when (e.Message == "Updates are managed by Microsoft Store.") { }
    Console.WriteLine("PASS: public update policy, package roots, release trust and Store isolation.");
}
finally { Directory.Delete(root, true); }

namespace Shadowless
{
    internal static class AppDistribution
    {
        internal static bool IsMicrosoftStore { get; set; }
    }
}

sealed class ReleasePages(string firstPage, string secondPage) : HttpMessageHandler
{
    private int _requests;
    protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
    {
        if (request.Headers.Authorization is not null)
            throw new Exception("Public update checks must not send repository credentials.");
        bool first = ++_requests == 1;
        if (!first && !request.RequestUri!.Query.Split('&').Contains("page=2"))
            throw new Exception("The updater did not advance the API page.");
        var response = new HttpResponseMessage(HttpStatusCode.OK) { Content = new StringContent(first ? firstPage : secondPage) };
        if (first)
            response.Headers.Add("Link", "<https://api.github.com/repos/Zefth051/Shadowless/releases?page=2>; rel=\"next\"");
        return Task.FromResult(response);
    }
}
