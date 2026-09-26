using System.IO;
using System.Text.Json;
using Stello.App.Models;
using Stello.App.Services;

namespace Stello.Net.Services;

/// <summary>Settings as JSON, e.g. %AppData%\Stello\settings.json (replaces the binary C++ rev.cfg).</summary>
public sealed class JsonSettingsStore(string path) : ISettingsStore
{
    public AppSettings Load()
    {
        try
        {
            if (!File.Exists(path))
            {
                return AppSettings.Default;
            }

            AppSettings? settings = JsonSerializer.Deserialize(File.ReadAllText(path), AppSettingsJson.Default.AppSettings);
            return settings?.Normalize() ?? AppSettings.Default;
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or JsonException or NotSupportedException)
        {
            return AppSettings.Default;
        }
    }

    public bool Save(AppSettings settings)
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path))!);

            // Write to a temporary file first so a crash cannot leave half a settings file.
            string temporary = path + ".tmp";
            File.WriteAllText(temporary, JsonSerializer.Serialize(settings, AppSettingsJson.Default.AppSettings));
            File.Move(temporary, path, overwrite: true);
            return true;
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            return false;
        }
    }
}
