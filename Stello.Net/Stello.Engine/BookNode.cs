namespace Stello.Engine;

/// <summary>Book learning state of a node (C++: CALCULATED, EXACT, INEXACT in Book.h).</summary>
[Flags]
internal enum BookFlags : short
{
    None = 0,

    /// <summary>The value comes from a search of this leaf.</summary>
    Calculated = 0x1,

    /// <summary>The search solved the position exactly.</summary>
    Exact = 0x2,

    /// <summary>The search solved the position for win/loss/draw.</summary>
    Inexact = 0x4,
}

/// <summary>A node in the opening book tree (C++: booktree). Moves use the legacy square numbers; 0 is a pass.</summary>
/// <remarks>The value is from the point of view of the player who makes the move.</remarks>
internal sealed class BookNode(short move, short value, BookFlags flag)
{
    public short Move { get; } = move;

    public short Value { get; set; } = value;

    public BookFlags Flag { get; set; } = flag;

    /// <summary>The replies, in book order (C++: barn, then the sosk chain).</summary>
    public List<BookNode> Children { get; } = [];
}
