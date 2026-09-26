namespace Stello.Net.Models;

/// <summary>Everything saved between sessions (C++: the revdef struct in rev.cfg).</summary>
public sealed record AppSettings(GameSettings Game, bool ShowAnalysis, WindowPlacement? Window)
{
    public static AppSettings Default { get; } = new(GameSettings.Default, ShowAnalysis: true, Window: null);

    public AppSettings Normalize() => this with
    {
        Game = (Game ?? GameSettings.Default).Normalize(),
        Window = Window is { Width: > 0, Height: > 0 } ? Window : null,
    };
}

/// <summary>Window position and size in device-independent units; the size is the restored size when maximised.</summary>
public sealed record WindowPlacement(double Left, double Top, double Width, double Height, bool Maximized);
