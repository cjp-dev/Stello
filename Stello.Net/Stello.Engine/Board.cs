using System.Numerics;
using System.Text;

namespace Stello.Engine;

/// <summary>Immutable Othello position stored as one bitboard per player (bit n = <see cref="Square.Index"/> n).</summary>
public readonly record struct Board
{
    private const ulong NotColumnA = 0xFEFE_FEFE_FEFE_FEFEUL;
    private const ulong NotColumnH = 0x7F7F_7F7F_7F7F_7F7FUL;

    // E, W, N, S, NE, NW, SE, SW; the mask removes discs that wrapped to the other edge.
    private static readonly int[] Shifts = [1, -1, 8, -8, 9, 7, -7, -9];
    private static readonly ulong[] ShiftMasks = [NotColumnA, NotColumnH, ulong.MaxValue, ulong.MaxValue, NotColumnA, NotColumnH, NotColumnA, NotColumnH];

    public Board(ulong black, ulong white)
    {
        if ((black & white) != 0)
        {
            throw new ArgumentException("A square cannot hold both a black and a white disc.");
        }

        Black = black;
        White = white;
    }

    // C++ init_game(): 44/55 LIGHT, 45/54 DARK.
    public static Board Initial { get; } = new(
        Square.Parse("e4").Bit | Square.Parse("d5").Bit,
        Square.Parse("d4").Bit | Square.Parse("e5").Bit);

    public ulong Black { get; }

    public ulong White { get; }

    public ulong Empty => ~(Black | White);

    public int EmptyCount => BitOperations.PopCount(Empty);

    public Player? this[Square square] =>
        (Black & square.Bit) != 0 ? Player.Black
        : (White & square.Bit) != 0 ? Player.White
        : null;

    public ulong Discs(Player player) => player == Player.Black ? Black : White;

    public int Count(Player player) => BitOperations.PopCount(Discs(player));

    public ulong LegalMoves(Player player)
    {
        ulong own = Discs(player);
        ulong opponent = Discs(player.Opponent());
        ulong empty = Empty;
        ulong moves = 0;

        for (int d = 0; d < Shifts.Length; d++)
        {
            ulong run = Shift(own, d) & opponent;
            for (int i = 0; i < 5; i++)
            {
                run |= Shift(run, d) & opponent;
            }

            moves |= Shift(run, d) & empty;
        }

        return moves;
    }

    public bool HasLegalMove(Player player) => LegalMoves(player) != 0;

    public bool IsGameOver => !HasLegalMove(Player.Black) && !HasLegalMove(Player.White);

    public bool IsLegal(Player player, Square square) => (LegalMoves(player) & square.Bit) != 0;

    /// <summary>Discs that would be flipped if <paramref name="player"/> plays on <paramref name="square"/>; 0 if the move is illegal.</summary>
    public ulong Flips(Player player, Square square)
    {
        if ((Empty & square.Bit) == 0)
        {
            return 0;
        }

        ulong own = Discs(player);
        ulong opponent = Discs(player.Opponent());
        ulong flips = 0;

        for (int d = 0; d < Shifts.Length; d++)
        {
            ulong line = 0;
            ulong next = Shift(square.Bit, d);
            while ((next & opponent) != 0)
            {
                line |= next;
                next = Shift(next, d);
            }

            if ((next & own) != 0)
            {
                flips |= line;
            }
        }

        return flips;
    }

    public Board Play(Player player, Square square)
    {
        ulong flips = Flips(player, square);
        if (flips == 0)
        {
            throw new InvalidOperationException($"{square} is not a legal move for {player}.");
        }

        ulong placed = flips | square.Bit;
        return player == Player.Black
            ? new Board(Black | placed, White & ~flips)
            : new Board(Black & ~flips, White | placed);
    }

    /// <summary>
    /// Parses 64 squares in index order (a1, b1, ... h8). 'X' = black, 'O' = white, '-' or '.' = empty.
    /// Whitespace is ignored.
    /// </summary>
    public static Board Parse(string text)
    {
        ulong black = 0;
        ulong white = 0;
        int index = 0;

        foreach (char c in text)
        {
            if (char.IsWhiteSpace(c))
            {
                continue;
            }

            if (index == 64)
            {
                throw new FormatException("A board must have exactly 64 squares.");
            }

            switch (c)
            {
                case 'X' or 'x':
                    black |= 1UL << index;
                    break;
                case 'O' or 'o':
                    white |= 1UL << index;
                    break;
                case '-' or '.':
                    break;
                default:
                    throw new FormatException($"Invalid square character '{c}'.");
            }

            index++;
        }

        if (index != 64)
        {
            throw new FormatException("A board must have exactly 64 squares.");
        }

        return new Board(black, white);
    }

    /// <summary>Eight lines, row 1 first, in the format read by <see cref="Parse"/>.</summary>
    public override string ToString()
    {
        var text = new StringBuilder(72);
        for (int index = 0; index < 64; index++)
        {
            text.Append(this[new Square(index)] switch
            {
                Player.Black => 'X',
                Player.White => 'O',
                _ => '-',
            });

            if ((index & 7) == 7 && index != 63)
            {
                text.Append('\n');
            }
        }

        return text.ToString();
    }

    private static ulong Shift(ulong bits, int direction)
    {
        int shift = Shifts[direction];
        ulong shifted = shift > 0 ? bits << shift : bits >> -shift;
        return shifted & ShiftMasks[direction];
    }
}
