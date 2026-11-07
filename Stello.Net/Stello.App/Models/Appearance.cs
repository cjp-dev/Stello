namespace Stello.App.Models;

public enum BoardStyle
{
    Green,
    GoldenOak,
    ReddishWood,
}

/// <summary>How the board and pieces look in either app.</summary>
public sealed record Appearance(BoardStyle Board, bool Pieces3D, bool AnimateFlips)
{
    public static Appearance Default { get; } = new(BoardStyle.GoldenOak, Pieces3D: true, AnimateFlips: true);

    /// <summary>The look before these options existed.</summary>
    public static Appearance Classic { get; } = new(BoardStyle.Green, Pieces3D: false, AnimateFlips: false);

    public Appearance Normalize() => Enum.IsDefined(Board) ? this : Default;
}
