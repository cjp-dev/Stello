using Microsoft.JSInterop;
using Stello.App.Models;
using Stello.App.Services;

namespace Stello.Web.Services;

/// <summary>
/// Plays procedural audio waves synthesized by SoundWaves in the browser via Web Audio API.
/// </summary>
public sealed class BrowserSoundService(IJSInProcessRuntime js) : ISoundService
{
    private bool _registered;

    public bool IsEnabled { get; set; } = true;

    public void Play(SoundType sound)
    {
        if (!IsEnabled)
        {
            return;
        }

        try
        {
            if (!_registered)
            {
                foreach (SoundType each in Enum.GetValues<SoundType>())
                {
                    js.InvokeVoid("stello.registerSound", each.ToString(), SoundWaves.Create(each));
                }

                _registered = true;
            }

            js.InvokeVoid("stello.playSound", sound.ToString());
        }
        catch (JSException)
        {
            // Functions silently if audio permissions are denied or autoplay restricted.
        }
    }
}

