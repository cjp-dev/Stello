// SPDX-License-Identifier: GPL-3.0-only
// Ported from Egaroucid src/engine/board.hpp (https://github.com/Nyanyan/Egaroucid), © Takuto Yamana, GPL-3.0-or-later.

using System.Runtime.CompilerServices;
using System.Text;

namespace Stello.Egaroucid;

/// <summary>
/// A position from the side to move's point of view (board.hpp): <see cref="Player"/> is the side to move. Bit 63 is
/// a1, bit 56 is h1 and bit 0 is h8.
/// </summary>
internal struct Board : IEquatable<Board>
{
    public ulong Player;
    public ulong Opponent;

    public Board(ulong player, ulong opponent)
    {
        Player = player;
        Opponent = opponent;
    }

    /// <summary>The start position, black to move.</summary>
    public static Board Initial => new(0x0000000810000000UL, 0x0000001008000000UL);

    /// <summary>The hash code used by the transposition table (board.hpp <c>hash</c>, the table version).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public readonly uint Hash()
    {
        uint[] p = HashTables.RandPlayer;
        uint[] o = HashTables.RandOpponent;
        uint h1 = p[(int)(Player & 0xFFFF)];
        uint h2 = o[(int)(Opponent & 0xFFFF)];
        h1 ^= p[65536 + (int)((Player >> 16) & 0xFFFF)];
        h2 ^= o[65536 + (int)((Opponent >> 16) & 0xFFFF)];
        h1 ^= p[2 * 65536 + (int)((Player >> 32) & 0xFFFF)];
        h2 ^= o[2 * 65536 + (int)((Opponent >> 32) & 0xFFFF)];
        h1 ^= p[3 * 65536 + (int)(Player >> 48)];
        h2 ^= o[3 * 65536 + (int)(Opponent >> 48)];
        return h1 ^ h2;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public readonly ulong GetLegal() => Flip.CalcLegal(Player, Opponent);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public void MoveBoard(in Flip flip)
    {
        Player ^= flip.Flipped;
        Opponent ^= flip.Flipped;
        Player ^= 1UL << flip.Pos;
        (Player, Opponent) = (Opponent, Player);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public readonly Board MoveCopy(in Flip flip) =>
        new(Opponent ^ flip.Flipped, Player ^ flip.Flipped ^ (1UL << flip.Pos));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public void Pass() => (Player, Opponent) = (Opponent, Player);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public void UndoBoard(in Flip flip)
    {
        (Player, Opponent) = (Opponent, Player);
        Player ^= 1UL << flip.Pos;
        Player ^= flip.Flipped;
        Opponent ^= flip.Flipped;
    }

    /// <summary>The final score for the side to move, empty squares to the winner (board.hpp <c>score_player</c>).</summary>
    public readonly int ScorePlayer()
    {
        int e = Bits.PopCount(~(Player | Opponent));
        int score = Bits.PopCount(Player) * 2 + e;
        score += (((score >> 6) & 1) + (((score + Common.HW2_M1) >> 7) & 1) - 1) * e;
        return score - Common.HW2;
    }

    public readonly int CountPlayer() => Bits.PopCount(Player);

    public readonly int CountOpponent() => Bits.PopCount(Opponent);

    public readonly int NDiscs() => Bits.PopCount(Player | Opponent);

    public readonly bool IsEnd() => Flip.CalcLegal(Player, Opponent) == 0 && Flip.CalcLegal(Opponent, Player) == 0;

    /// <summary>
    /// Reads 64 squares a1..h8 followed by the side to move (board.hpp <c>from_str</c>): X, B, 0 or * is black, O, W or
    /// 1 is white, anything else is empty. White space is ignored.
    /// </summary>
    /// <exception cref="FormatException">The text is not a position.</exception>
    public static Board FromString(string text)
    {
        var compact = new StringBuilder(text.Length);
        foreach (char c in text)
        {
            if (!char.IsWhiteSpace(c))
            {
                compact.Append(c);
            }
        }

        if (compact.Length != Common.HW2 + 1)
        {
            throw new FormatException($"Expected 64 squares and the side to move: {text}");
        }

        ulong black = 0, white = 0;
        for (int i = 0; i < Common.HW2; i++)
        {
            if (Common.IsBlackLikeChar(compact[i]))
            {
                black |= 1UL << (Common.HW2_M1 - i);
            }
            else if (Common.IsWhiteLikeChar(compact[i]))
            {
                white |= 1UL << (Common.HW2_M1 - i);
            }
        }

        char side = compact[Common.HW2];
        if (Common.IsWhiteLikeChar(side))
        {
            return new Board(white, black);
        }

        return Common.IsBlackLikeChar(side) ? new Board(black, white) : throw new FormatException($"Unknown side to move: {text}");
    }

    /// <summary>The 64 squares a1..h8 and the side to move, as read by <see cref="FromString"/>.</summary>
    public readonly string ToString(bool blackToMove)
    {
        var text = new StringBuilder(66);
        for (int i = 0; i < Common.HW2; i++)
        {
            ulong bit = 1UL << (Common.HW2_M1 - i);
            bool own = (Player & bit) != 0, other = (Opponent & bit) != 0;
            text.Append(own ? (blackToMove ? 'X' : 'O') : other ? (blackToMove ? 'O' : 'X') : '-');
        }

        return text.Append(blackToMove ? " X" : " O").ToString();
    }

    public readonly bool Equals(Board other) => Player == other.Player && Opponent == other.Opponent;

    public override readonly bool Equals(object? obj) => obj is Board other && Equals(other);

    public override readonly int GetHashCode() => HashCode.Combine(Player, Opponent);

    public static bool operator ==(Board a, Board b) => a.Equals(b);

    public static bool operator !=(Board a, Board b) => !a.Equals(b);
}
