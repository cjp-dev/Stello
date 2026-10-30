// SPDX-License-Identifier: GPL-3.0-only
// Ported from Egaroucid src/engine/book.hpp and util.hpp (https://github.com/Nyanyan/Egaroucid), © Takuto Yamana,
// GPL-3.0-or-later. Only what playing from the book needs: reading .egbk3 files, looking positions up and choosing a
// move. Editing, expanding and the other file formats are left out.

namespace Stello.Egaroucid;

/// <summary>A move from the book and its value for the side to move (book.hpp <c>Book_value</c>).</summary>
internal readonly record struct BookValue(int Policy, int Value);

/// <summary>
/// Egaroucid's opening book (book.hpp): positions in their canonical orientation (the smallest of the 8 symmetries)
/// with the value of the game for the side to move, in discs.
/// </summary>
internal sealed class Book
{
    public const int BOOK_ACCURACY_LEVEL_INF = 10;
    public const int BOOK_LOSS_IGNORE_THRESHOLD = 8;

    private readonly Dictionary<(ulong Player, ulong Opponent), (sbyte Value, sbyte Level)> _positions;

    private Book(Dictionary<(ulong, ulong), (sbyte, sbyte)> positions)
    {
        _positions = positions;
    }

    public static Book Empty { get; } = new([]);

    public int Count => _positions.Count;

    /// <summary>
    /// Reads an .egbk3 file (book.hpp <c>import_egbk3</c>): "DICUORAGE", version 3, a uint32 count, then 25 bytes per
    /// position (player, opponent, value, level, number of lines, leaf value, leaf move, leaf level). Positions are
    /// stored in their canonical orientation; a position read twice keeps the value of the higher level.
    /// </summary>
    /// <exception cref="IOException">The file is not an .egbk3 book.</exception>
    public static Book Load(string path)
    {
        byte[] bytes = File.ReadAllBytes(path);
        if (bytes.Length < 14 || System.Text.Encoding.ASCII.GetString(bytes, 0, 9) != "DICUORAGE" || bytes[9] != 3)
        {
            throw new IOException($"{path} is not an Egaroucid .egbk3 book");
        }

        uint count = BitConverter.ToUInt32(bytes, 10);
        long available = (bytes.Length - 14) / 25;
        var positions = new Dictionary<(ulong, ulong), (sbyte, sbyte)>((int)Math.Min(count, available));
        for (long i = 0; i < Math.Min(count, available); i++)
        {
            int at = 14 + (int)(i * 25);
            var board = new Board(BitConverter.ToUInt64(bytes, at), BitConverter.ToUInt64(bytes, at + 8));
            sbyte value = (sbyte)bytes[at + 16];
            sbyte level = (sbyte)bytes[at + 17];
            if (value is < -Common.HW2 or > Common.HW2 || (board.Player & board.Opponent) != 0)
            {
                continue;
            }

            Board representative = RepresentativeBoard(board);
            var key = (representative.Player, representative.Opponent);
            if (!positions.TryGetValue(key, out (sbyte Value, sbyte Level) registered) || registered.Level <= level)
            {
                positions[key] = (value, level);
            }
        }

        return new Book(positions);
    }

    /// <summary>The smallest (player, then opponent) of the 8 symmetries of <paramref name="board"/> (util.hpp <c>representative_board</c>).</summary>
    public static Board RepresentativeBoard(Board b)
    {
        Board res = b;
        Board bt = new(Bits.BlackLineMirror(b.Player), Bits.BlackLineMirror(b.Opponent));
        Compare(ref res, bt);
        Board bv = new(Bits.VerticalMirror(b.Player), Bits.VerticalMirror(b.Opponent));
        Compare(ref res, bv);
        Board btv = new(Bits.VerticalMirror(bt.Player), Bits.VerticalMirror(bt.Opponent));
        Compare(ref res, btv);
        b = new Board(Bits.HorizontalMirror(b.Player), Bits.HorizontalMirror(b.Opponent));
        Compare(ref res, b);
        bt = new Board(Bits.HorizontalMirror(bt.Player), Bits.HorizontalMirror(bt.Opponent));
        Compare(ref res, bt);
        b = new Board(Bits.VerticalMirror(b.Player), Bits.VerticalMirror(b.Opponent));
        Compare(ref res, b);
        bt = new Board(Bits.VerticalMirror(bt.Player), Bits.VerticalMirror(bt.Opponent));
        Compare(ref res, bt);
        return res;

        static void Compare(ref Board res, in Board cmp)
        {
            if (res.Player > cmp.Player || (res.Player == cmp.Player && res.Opponent > cmp.Opponent))
            {
                res = cmp;
            }
        }
    }

    /// <summary>The value of <paramref name="board"/> for the side to move, if it is in the book (book.hpp <c>get_value</c>).</summary>
    public bool TryGetValue(in Board board, out int value)
    {
        Board representative = RepresentativeBoard(board);
        if (_positions.TryGetValue((representative.Player, representative.Opponent), out (sbyte Value, sbyte Level) elem))
        {
            value = elem.Value;
            return true;
        }

        value = SearchConst.SCORE_UNDEFINED;
        return false;
    }

    /// <summary>
    /// The book moves of <paramref name="b"/> with their values (book.hpp <c>get_all_moves_with_value</c>). After a move
    /// the opponent may have to pass, or the game may be over; the value is then read from the right position.
    /// </summary>
    public List<BookValue> GetAllMovesWithValue(in Board b)
    {
        var policies = new List<BookValue>();
        ulong legal = b.GetLegal();
        var flip = new Flip();
        for (int cell = Bits.FirstBit(ref legal); legal != 0; cell = Bits.NextBit(ref legal))
        {
            flip.Calc(b.Player, b.Opponent, cell);
            var moved = new Board(b.Opponent ^ flip.Flipped, b.Player ^ flip.Flipped ^ (1UL << cell));
            bool passed = moved.GetLegal() == 0;
            bool isEnd = passed && Flip.CalcLegal(moved.Opponent, moved.Player) == 0;
            Board lookup = moved;
            if (!isEnd && passed)
            {
                lookup.Pass();
            }

            if (TryGetValue(lookup, out int value))
            {
                policies.Add(new BookValue(cell, passed && !isEnd ? value : -value));
            }
            else if (isEnd)
            {
                Board end = moved;
                end.Pass();
                if (TryGetValue(end, out value))
                {
                    policies.Add(new BookValue(cell, value));
                }
            }
        }

        return policies;
    }

    /// <summary>
    /// Chooses a book move among <paramref name="useLegal"/> (book.hpp <c>get_random</c>). With
    /// <paramref name="accLevel"/> 0 it is one of the best moves, drawn uniformly; higher levels also allow worse moves
    /// (up to 2 × level discs worse), weighted by how good they are. Returns policy −1 when there is no usable book
    /// move. C++ draws from a clock-seeded generator; here <paramref name="random"/> is given, so games can be repeated.
    /// </summary>
    public BookValue GetRandom(in Board board, int accLevel, ulong useLegal, Random random)
    {
        List<BookValue> children = ChildValues(board, useLegal);
        var valuePolicies = children.Select(child => ((double)child.Value, child.Policy)).ToList();
        double bestScore = -Common.INF;
        foreach (BookValue child in children)
        {
            if (child.Value > bestScore)
            {
                bestScore = child.Value;
            }
        }

        TryGetValue(board, out int boardValue);
        if (valuePolicies.Count == 0 || bestScore < boardValue - BOOK_LOSS_IGNORE_THRESHOLD)
        {
            return new BookValue(-1, -Common.INF);
        }

        double acceptableMinValue = bestScore - 2.0 * accLevel - 0.5;
        double sumExpValues = 0.0;
        for (int i = 0; i < valuePolicies.Count; i++)
        {
            (double weight, int policy) = valuePolicies[i];
            weight = weight < acceptableMinValue
                ? 0.0
                : Math.Pow((Math.Exp(weight - bestScore) + 1.5) / 3.0, BOOK_ACCURACY_LEVEL_INF - accLevel);
            valuePolicies[i] = (weight, policy);
            sumExpValues += weight;
        }

        double rnd = random.NextDouble();
        double s = 0.0;
        foreach ((double weight, int policy) in valuePolicies)
        {
            s += weight / sumExpValues;
            if (s >= rnd)
            {
                return new BookValue(policy, children.Find(memo => memo.Policy == policy).Value);
            }
        }

        int last = valuePolicies[^1].Policy;
        return new BookValue(last, children.Find(memo => memo.Policy == last).Value);
    }

    /// <summary>
    /// The values of the moves in <paramref name="useLegal"/> whose position is in the book, for the side to move, as
    /// <c>get_random</c> reads them: when the opponent must pass after a move, the position after the pass is read.
    /// </summary>
    public List<BookValue> ChildValues(in Board board, ulong useLegal)
    {
        var children = new List<BookValue>();
        ulong legal = board.GetLegal();
        Board b = board;
        var flip = new Flip();
        for (int cell = Bits.FirstBit(ref legal); legal != 0; cell = Bits.NextBit(ref legal))
        {
            if (((1UL << cell) & useLegal) == 0)
            {
                continue;
            }

            flip.Calc(b.Player, b.Opponent, cell);
            b.MoveBoard(flip);
            int sgn = -1;
            if (b.GetLegal() == 0)
            {
                sgn = 1;
                b.Pass();
            }

            if (TryGetValue(b, out int registeredValue))
            {
                children.Add(new BookValue(cell, sgn * registeredValue));
            }

            if (sgn == 1)
            {
                b.Pass();
            }

            b.UndoBoard(flip);
        }

        return children;
    }
}
