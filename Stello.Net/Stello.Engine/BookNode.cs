namespace Stello.Engine;

/// <summary>A node in the opening book tree (C++: booktree). Moves use the legacy square numbers; 0 is a pass.</summary>
internal sealed class BookNode(short move, short value, short flag)
{
    public short Move { get; } = move;

    public short Value { get; set; } = value;

    public short Flag { get; set; } = flag;

    /// <summary>The replies, in book order (C++: barn, then the sosk chain).</summary>
    public List<BookNode> Children { get; } = [];
}
