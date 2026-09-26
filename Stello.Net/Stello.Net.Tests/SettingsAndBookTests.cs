using System.IO;
using Stello.App.Models;
using Stello.Engine;
using Stello.Net.Services;

namespace Stello.Net.Tests;

public sealed class JsonSettingsStoreTests : IDisposable
{
    private readonly string _directory = Path.Combine(Path.GetTempPath(), $"stello-{Guid.NewGuid():N}");

    private string SettingsFile => Path.Combine(_directory, "settings.json");

    public void Dispose()
    {
        if (Directory.Exists(_directory))
        {
            Directory.Delete(_directory, recursive: true);
        }
    }

    [Fact]
    public void Load_WithoutFileGivesDefaults()
    {
        Assert.Equal(AppSettings.Default, new JsonSettingsStore(SettingsFile).Load());
    }

    [Fact]
    public void SaveAndLoad_RoundTrip()
    {
        var store = new JsonSettingsStore(SettingsFile);
        var settings = new AppSettings(
            new GameSettings(TimeControlMode.TimePerMove, 12, 30, 15),
            ShowAnalysis: false,
            new WindowPlacement(100, 50, 900, 700, Maximized: true));

        Assert.True(store.Save(settings));

        Assert.Equal(settings, new JsonSettingsStore(SettingsFile).Load());
        Assert.False(File.Exists(SettingsFile + ".tmp"));
    }

    [Fact]
    public void Save_WritesReadableJson()
    {
        new JsonSettingsStore(SettingsFile).Save(AppSettings.Default);

        string json = File.ReadAllText(SettingsFile);

        Assert.Contains("\"Mode\": \"TimePerGame\"", json);
        Assert.DoesNotContain("GameTime", json);
    }

    [Fact]
    public void Load_DamagedFileGivesDefaults()
    {
        Directory.CreateDirectory(_directory);
        File.WriteAllText(SettingsFile, "{ not json");

        Assert.Equal(AppSettings.Default, new JsonSettingsStore(SettingsFile).Load());
    }

    [Fact]
    public void Load_KeepsValuesInRange()
    {
        Directory.CreateDirectory(_directory);
        File.WriteAllText(SettingsFile, """
            {
              "Game": { "Mode": "Solve", "Depth": 99, "SecondsPerMove": 0, "MinutesPerGame": -5 },
              "ShowAnalysis": true,
              "Window": { "Left": 0, "Top": 0, "Width": 0, "Height": 0, "Maximized": false }
            }
            """);

        AppSettings settings = new JsonSettingsStore(SettingsFile).Load();

        Assert.Equal(new GameSettings(TimeControlMode.TimePerGame, 20, 1, 1), settings.Game);
        Assert.Null(settings.Window);
    }

    [Fact]
    public void Load_MissingGameSettingsGivesDefaults()
    {
        Directory.CreateDirectory(_directory);
        File.WriteAllText(SettingsFile, """{ "ShowAnalysis": false }""");

        AppSettings settings = new JsonSettingsStore(SettingsFile).Load();

        Assert.Equal(GameSettings.Default, settings.Game);
        Assert.False(settings.ShowAnalysis);
    }

    [Fact]
    public void Save_FailsWhenTheFileCannotBeWritten()
    {
        Directory.CreateDirectory(SettingsFile);

        Assert.False(new JsonSettingsStore(SettingsFile).Save(AppSettings.Default));
    }
}

public sealed class BookLoaderTests : IDisposable
{
    private static readonly string ShippedBook = Path.Combine(AppContext.BaseDirectory, "Data", "OPENING");
    private readonly string _directory = Path.Combine(Path.GetTempPath(), $"stello-{Guid.NewGuid():N}");

    public BookLoaderTests() => Directory.CreateDirectory(_directory);

    public void Dispose() => Directory.Delete(_directory, recursive: true);

    [Fact]
    public void Load_SkipsMissingFiles()
    {
        OpeningBook? book = BookLoader.Load([Path.Combine(_directory, "OPENING"), ShippedBook], out string? notice);

        Assert.NotNull(book);
        Assert.Null(notice);
    }

    [Fact]
    public void Load_FallsBackWhenTheUserBookIsDamaged()
    {
        string userBook = Path.Combine(_directory, "OPENING");
        File.WriteAllBytes(userBook, [1, 2, 3]);

        OpeningBook? book = BookLoader.Load([userBook, ShippedBook], out string? notice);

        Assert.NotNull(book);
        Assert.Contains("could not be read", notice);
    }

    [Fact]
    public void Load_WithoutBookGivesNotice()
    {
        OpeningBook? book = BookLoader.Load([Path.Combine(_directory, "OPENING")], out string? notice);

        Assert.Null(book);
        Assert.Equal("No opening book was found.", notice);
    }
}
