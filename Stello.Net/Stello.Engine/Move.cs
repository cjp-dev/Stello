using System.Diagnostics.CodeAnalysis;

namespace Stello.Engine;

/// <summary>A move in a game: a disc placed on a square, or a pass.</summary>
public readonly record struct Move
{
    public Move(Square square)
    {
        Square = square;
    }

    public static Move Pass => default;

    public Square? Square { get; }

    public bool IsPass => Square is null;

    public static bool TryParse(string? text, [NotNullWhen(true)] out Move? move)
    {
        if (string.Equals(text, "pass", StringComparison.OrdinalIgnoreCase))
        {
            move = Pass;
            return true;
        }

        move = Engine.Square.TryParse(text, out Square? square) ? new Move(square.Value) : null;
        return move is not null;
    }

    public override string ToString() => Square?.ToString() ?? "pass";
}
