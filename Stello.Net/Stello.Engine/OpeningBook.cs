using System.Diagnostics.CodeAnalysis;
using System.Text;

namespace Stello.Engine;

/// <summary>A book move and its value from the book.</summary>
public readonly record struct BookMove(Square Square, int Value);

/// <summary>
/// The opening book: the book moves and their values for every book position. A position is stored once,
/// whatever move order and whichever of Black's four (symmetric) first moves led to it.
/// </summary>
/// <remarks>
/// A position is keyed by its canonical form: of the four symmetries that keep the start position, the one that
/// gives the smallest bitboards. Its moves are stored in that frame. All lines start after Black's first move,
/// written as d3. Files: <see cref="BookBinaryFormat"/> for the apps, <see cref="BookTextFormat"/> for the master
/// book in git, and the C++ "OPENING" file, which is only read (<see cref="LegacyBookFormat"/>).
/// </remarks>
public sealed class OpeningBook
{
    /// <summary>Bumped by hand when a change to the evaluation or the search changes the values it finds.</summary>
    internal const int EngineVersion = 1;

    internal const Player RootPlayer = Player.White;

    private static readonly Square[] FirstMoves = [Square.Parse("d3"), Square.Parse("c4"), Square.Parse("f5"), Square.Parse("e6")];

    private readonly Dictionary<BookKey, List<BookEntry>> _positions = [];

    private OpeningBook()
    {
    }

    // The symmetries that keep the start position (C++: convop). Each one is its own inverse.
    internal enum Symmetry
    {
        Identity,
        MainDiagonal,
        AntiDiagonal,
        HalfTurn,
    }

    /// <summary>The position after d3, where every line of the book starts.</summary>
    internal static Board RootBoard { get; } = Board.Initial.Play(Player.Black, Square.Parse("d3"));

    /// <summary>The number of book moves.</summary>
    public int NodeCount => _positions.Values.Sum(replies => replies.Count);

    /// <summary>The number of positions with book moves.</summary>
    public int PositionCount => _positions.Count;

    /// <summary>A book without any lines, to be filled by book learning.</summary>
    public static OpeningBook CreateEmpty() => new();

    /// <summary>Reads a book in the binary or the text format, or the C++ "OPENING" file.</summary>
    /// <exception cref="InvalidDataException">The data is not a valid opening book.</exception>
    public static OpeningBook Load(Stream stream)
    {
        using var buffer = new MemoryStream();
        stream.CopyTo(buffer);
        byte[] data = buffer.ToArray();

        if (BookBinaryFormat.IsBinary(data))
        {
            return BookBinaryFormat.Read(data);
        }

        return BookTextFormat.IsText(data)
            ? BookTextFormat.Read(new StringReader(Encoding.UTF8.GetString(data)))
            : LegacyBookFormat.Read(data);
    }

    /// <inheritdoc cref="Load(Stream)"/>
    public static OpeningBook Load(string path)
    {
        using FileStream stream = File.OpenRead(path);
        return Load(stream);
    }

    /// <summary>Writes the book in the binary format.</summary>
    public void Save(Stream stream) => BookBinaryFormat.Write(this, stream);

    /// <inheritdoc cref="Save(Stream)"/>
    public void Save(string path)
    {
        using FileStream stream = File.Create(path);
        Save(stream);
    }

    /// <summary>
    /// Finds a book move for <paramref name="player"/> (C++: getlib). Black's first move is chosen at random;
    /// later the first move in book order is used.
    /// </summary>
    public bool TryGetMove(Board board, Player player, Random random, out BookMove move)
    {
        ArgumentNullException.ThrowIfNull(random);

        if (board == Board.Initial && player == Player.Black)
        {
            move = new BookMove(FirstMoves[random.Next(FirstMoves.Length)], 0);
            return true;
        }

        if (TryGetReplies(board, player, out List<BookEntry>? replies, out Symmetry symmetry))
        {
            foreach (BookEntry reply in replies)
            {
                if (reply.Move.Square is { } square)
                {
                    move = new BookMove(Transform(square, symmetry), reply.Value);
                    return true;
                }
            }
        }

        move = default;
        return false;
    }

    /// <summary>The book moves in the position, and the symmetry between the board and the book's frame.</summary>
    internal bool TryGetReplies(Board board, Player player, [NotNullWhen(true)] out List<BookEntry>? replies, out Symmetry symmetry)
    {
        (BookKey key, symmetry) = Canonical(board, player);
        return _positions.TryGetValue(key, out replies);
    }

    internal bool Contains(Board board, Player player) => _positions.ContainsKey(Canonical(board, player).Key);

    internal bool TryGetReplies(BookKey key, [NotNullWhen(true)] out List<BookEntry>? replies) =>
        _positions.TryGetValue(key, out replies);

    internal List<BookEntry> GetOrAddReplies(Board board, Player player, out Symmetry symmetry)
    {
        (BookKey key, symmetry) = Canonical(board, player);
        if (!_positions.TryGetValue(key, out List<BookEntry>? replies))
        {
            replies = [];
            _positions.Add(key, replies);
        }

        return replies;
    }

    /// <summary>Maps a move between the board and the book's frame (both ways, as every symmetry is its own inverse).</summary>
    internal static Move Transform(Move move, Symmetry symmetry) =>
        move.Square is { } square ? new Move(Transform(square, symmetry)) : move;

    internal static (Board Board, Player Player) Play(Board board, Player player, Move move) =>
        move.Square is { } square ? (board.Play(player, square), player.Opponent()) : (board, player.Opponent());

    /// <summary>A legal square, or a pass when the player has no move but the opponent has.</summary>
    internal static bool IsLegal(Board board, Player player, Move move) =>
        move.Square is { } square
            ? board.IsLegal(player, square)
            : !board.HasLegalMove(player) && board.HasLegalMove(player.Opponent());

    internal static (BookKey Key, Symmetry Symmetry) Canonical(Board board, Player player)
    {
        Board best = board;
        Symmetry bestSymmetry = Symmetry.Identity;
        for (Symmetry symmetry = Symmetry.MainDiagonal; symmetry <= Symmetry.HalfTurn; symmetry++)
        {
            Board transformed = Transform(board, symmetry);
            if (transformed.Black < best.Black || (transformed.Black == best.Black && transformed.White < best.White))
            {
                best = transformed;
                bestSymmetry = symmetry;
            }
        }

        return (new BookKey(best.Black, best.White, player), bestSymmetry);
    }

    internal static Square Transform(Square square, Symmetry symmetry) => symmetry switch
    {
        Symmetry.MainDiagonal => Square.At(square.Row, square.Column),
        Symmetry.AntiDiagonal => Square.At(7 - square.Row, 7 - square.Column),
        Symmetry.HalfTurn => Square.At(7 - square.Column, 7 - square.Row),
        _ => square,
    };

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

/// <summary>A position in its canonical form (see <see cref="OpeningBook"/>).</summary>
internal readonly record struct BookKey(ulong Black, ulong White, Player ToMove);
