using Stello.App.Models;

namespace Stello.App.Services;

public interface ISettingsStore
{
    /// <summary>The saved settings, or the defaults if there are none or they cannot be read.</summary>
    AppSettings Load();

    /// <returns>False if the settings could not be written.</returns>
    bool Save(AppSettings settings);
}
