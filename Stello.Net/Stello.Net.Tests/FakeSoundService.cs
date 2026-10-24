using Stello.App.Models;
using Stello.App.Services;

namespace Stello.Net.Tests;

internal sealed class FakeSoundService : ISoundService
{
    public bool IsEnabled { get; set; } = true;

    public List<SoundType> PlayedSounds { get; } = [];

    public void Play(SoundType sound)
    {
        if (IsEnabled)
        {
            PlayedSounds.Add(sound);
        }
    }
}

