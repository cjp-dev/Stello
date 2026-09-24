using System.Diagnostics.CodeAnalysis;

namespace Stello.Engine;

/// <summary>A book move and its value from the book.</summary>
public readonly record struct BookMove(Square Square, int Value);

/// <summary>
/// The opening book from the C++ version (file "OPENING"). Lines are stored after Black's first move,
/// normalised to d3; the other first moves are found by symmetry.
/// </summary>
/// <remarks>
/// File layout (little-endian, as written by Put_book/savebook in Book.cpp):
/// int32 node count (C++ allocation counter, can be larger than the real number of nodes), then the root
/// sibling chain. A chain is an int16 number of nodes; each node is int16 move, int16 value, int16 flag,
/// followed by the chain of its replies.
/// </remarks>
public sealed class OpeningBook
{
    private const int MaxChainLength = 64;
    private const int MaxDepth = 64;

    // C++ savebook writes 0 instead of this value for the first node in a chain.
    private const short ClearedValue = 32600;

    private static readonly Square NormalisedFirstMove = Square.Parse("d3");
    private static readonly Square[] FirstMoves = [Square.Parse("d3"), Square.Parse("c4"), Square.Parse("f5"), Square.Parse("e6")];

    private readonly int _headerNodeCount;
    private readonly Dictionary<(ulong Black, ulong White, Player ToMove), List<BookNode>> _positions = [];

    private OpeningBook(List<BookNode> root, int headerNodeCount)
    {
        Root = root;
        _headerNodeCount = headerNodeCount;
        Rebuild();
    }

    // The symmetries that keep the start position (C++: convop).
    internal enum Symmetry
    {
        Identity,
        MainDiagonal,
        AntiDiagonal,
        HalfTurn,
    }

    /// <summary>White's replies to d3.</summary>
    internal List<BookNode> Root { get; }

    public int NodeCount { get; private set; }

    /// <summary>A book without any lines, to be filled by book learning.</summary>
    public static OpeningBook CreateEmpty() => new([], 0);

    /// <exception cref="InvalidDataException">The data is not a valid opening book.</exception>
    public static OpeningBook Load(Stream stream)
    {
        using var reader = new BinaryReader(stream, System.Text.Encoding.UTF8, leaveOpen: true);
        try
        {
            int headerNodeCount = reader.ReadInt32();
            List<BookNode> root = ReadChain(reader, depth: 0);
            return new OpeningBook(root, headerNodeCount);
        }
        catch (EndOfStreamException exception)
        {
            throw new InvalidDataException("The opening book file is truncated.", exception);
        }
    }

    public static OpeningBook Load(string path)
    {
        using FileStream stream = File.OpenRead(path);
        return Load(stream);
    }

    public void Save(Stream stream)
    {
        using var writer = new BinaryWriter(stream, System.Text.Encoding.UTF8, leaveOpen: true);
        writer.Write(Math.Max(_headerNodeCount, NodeCount));
        WriteChain(writer, Root);
    }

    public void Save(string path)
    {
        using FileStream stream = File.Create(path);
        Save(stream);
    }

    /// <summary>
    /// Finds a book move for <paramref name="player"/> (C++: getlib). Black's first move is chosen at random;
    /// later the first legal reply in book order is used.
    /// </summary>
    public bool TryGetMove(Board board, Player player, Random random, out BookMove move)
    {
        ArgumentNullException.ThrowIfNull(random);

        if (board == Board.Initial && player == Player.Black)
        {
            move = new BookMove(FirstMoves[random.Next(FirstMoves.Length)], 0);
            return true;
        }

        foreach (Symmetry symmetry in Enum.GetValues<Symmetry>())
        {
            if (!TryFindReplies(board, player, symmetry, out List<BookNode>? replies))
            {
                continue;
            }

            foreach (BookNode reply in replies)
            {
                if (!IsSquare(reply.Move))
                {
                    continue;
                }

                // Every symmetry is its own inverse.
                Square square = Transform(Square.FromLegacy(reply.Move), symmetry);
                if (board.IsLegal(player, square))
                {
                    move = new BookMove(square, reply.Value);
                    return true;
                }
            }
        }

        move = default;
        return false;
    }

    /// <summary>Recomputes the node count and the position index after the tree was changed.</summary>
    internal void Rebuild()
    {
        NodeCount = CountNodes(Root);
        _positions.Clear();
        Index(Root, Board.Initial.Play(Player.Black, NormalisedFirstMove), Player.White);
    }

    /// <summary>The book's replies in the position, and the symmetry that maps the board to the book's frame.</summary>
    internal bool TryFindReplies(Board board, Player player, out List<BookNode> replies, out Symmetry symmetry)
    {
        foreach (Symmetry candidate in Enum.GetValues<Symmetry>())
        {
            if (TryFindReplies(board, player, candidate, out List<BookNode>? found))
            {
                replies = found;
                symmetry = candidate;
                return true;
            }
        }

        replies = [];
        symmetry = Symmetry.Identity;
        return false;
    }

    /// <summary>The symmetry that maps one of Black's first moves to d3 (C++: convop).</summary>
    internal static Symmetry FirstMoveSymmetry(Square firstMove) =>
        Enum.GetValues<Symmetry>().First(s => Transform(firstMove, s) == NormalisedFirstMove);

    internal static bool IsSquare(short legacy) => legacy is >= 11 and <= 88 && legacy % 10 is >= 1 and <= 8;

    internal static Square Transform(Square square, Symmetry symmetry) => symmetry switch
    {
        Symmetry.MainDiagonal => Square.At(square.Row, square.Column),
        Symmetry.AntiDiagonal => Square.At(7 - square.Row, 7 - square.Column),
        Symmetry.HalfTurn => Square.At(7 - square.Column, 7 - square.Row),
        _ => square,
    };

    private bool TryFindReplies(Board board, Player player, Symmetry symmetry, [NotNullWhen(true)] out List<BookNode>? replies)
    {
        Board transformed = Transform(board, symmetry);
        return _positions.TryGetValue((transformed.Black, transformed.White, player), out replies);
    }

    private static List<BookNode> ReadChain(BinaryReader reader, int depth)
    {
        short count = reader.ReadInt16();
        if (count is < 0 or > MaxChainLength || (count > 0 && depth >= MaxDepth))
        {
            throw new InvalidDataException("The opening book file is not valid.");
        }

        var chain = new List<BookNode>(count);
        for (int i = 0; i < count; i++)
        {
            var node = new BookNode(reader.ReadInt16(), reader.ReadInt16(), (BookFlags)reader.ReadInt16());
            node.Children.AddRange(ReadChain(reader, depth + 1));
            chain.Add(node);
        }

        return chain;
    }

    private static void WriteChain(BinaryWriter writer, List<BookNode> chain)
    {
        writer.Write((short)chain.Count);
        for (int i = 0; i < chain.Count; i++)
        {
            BookNode node = chain[i];
            writer.Write(node.Move);
            writer.Write(i == 0 && node.Value == ClearedValue ? (short)0 : node.Value);
            writer.Write((short)node.Flag);
            WriteChain(writer, node.Children);
        }
    }

    private static int CountNodes(List<BookNode> chain) =>
        chain.Sum(node => 1 + CountNodes(node.Children));

    // Maps every book position to its replies; the first line that reaches a position wins.
    private void Index(List<BookNode> replies, Board board, Player player)
    {
        if (replies.Count == 0)
        {
            return;
        }

        _positions.TryAdd((board.Black, board.White, player), replies);
        Player opponent = player.Opponent();

        foreach (BookNode node in replies)
        {
            if (node.Move == 0)
            {
                if (!board.HasLegalMove(player))
                {
                    Index(node.Children, board, opponent);
                }

                continue;
            }

            // Lines with moves that are not legal cannot be reached and are skipped.
            if (IsSquare(node.Move) && board.IsLegal(player, Square.FromLegacy(node.Move)))
            {
                Index(node.Children, board.Play(player, Square.FromLegacy(node.Move)), opponent);
            }
        }
    }

    internal static Board Transform(Board board, Symmetry symmetry) => symmetry == Symmetry.Identity
        ? board
        : new Board(Transform(board.Black, symmetry), Transform(board.White, symmetry));

    private static ulong Transform(ulong bits, Symmetry symmetry)
    {
        ulong result = 0;
        foreach (Square square in Square.InMask(bits))
        {
            result |= Transform(square, symmetry).Bit;
        }

        return result;
    }
}
