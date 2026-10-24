using Stello.App.Models;

namespace Stello.App.Services;

public interface ISoundService
{
    bool IsEnabled { get; set; }
    void Play(SoundType sound);
}

/// <summary>
/// A no-op sound service used when audio is disabled or unavailable.
/// </summary>
public sealed class NullSoundService : ISoundService
{
    public static NullSoundService Instance { get; } = new();

    public bool IsEnabled { get; set; }

    public void Play(SoundType sound)
    {
    }
}

