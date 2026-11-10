using System.Text.Json;

if (args.Length != 4 || args[0] != "--launcher-probe")
    throw new ArgumentException("Expected launcher probe, text, empty argument and result path.");

File.WriteAllText(args[3], JsonSerializer.Serialize(new
{
    Arguments = args,
    WorkingDirectory = Environment.CurrentDirectory,
    ProcessPath = Environment.ProcessPath,
}));
return 17;
