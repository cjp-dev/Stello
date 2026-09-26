using Stello.App.Models;
using Stello.App.Services;

namespace Stello.Net.Tests;

internal sealed class FakeSettingsStore(AppSettings settings) : ISettingsStore
{
    public AppSettings Settings { get; private set; } = settings;

    public int Saves { get; private set; }

    public bool CanSave { get; set; } = true;

    public AppSettings Load() => Settings;

    public bool Save(AppSettings settings)
    {
        if (!CanSave)
        {
            return false;
        }

        Settings = settings;
        Saves++;
        return true;
    }
}
