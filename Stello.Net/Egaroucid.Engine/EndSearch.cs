// SPDX-License-Identifier: GPL-3.0-only
// Ported from Egaroucid src/engine/endsearch_common.hpp, endsearch_last_generic.hpp, endsearch_nws_last_generic.hpp,
// endsearch_nws.hpp, stability_cutoff.hpp and last_flip_generic.hpp (https://github.com/Nyanyan/Egaroucid),
// © Takuto Yamana, GPL-3.0-or-later.

using System.Runtime.CompilerServices;

namespace Stello.Egaroucid;

/// <summary>
/// The exact endgame search (the generic C++ code): from 13 empty squares a null-window search with its own small
/// table and move ordering by the move-ordering evaluation, from 10 a simpler ordering by parity and mobility, from 6 a
/// fast search ordered by squares that are alone in their region, and special code for the last 4 squares.
/// </summary>
internal static class EndSearch
{
    private static readonly ulong[] AdjacentSq =
    [
        0x0000000000000302UL, 0x0000000000000705UL, 0x0000000000000e0aUL, 0x0000000000001c14UL,
        0x0000000000003828UL, 0x0000000000007050UL, 0x000000000000e0a0UL, 0x000000000000c040UL,
        0x0000000000030203UL, 0x0000000000070507UL, 0x00000000000e0a0eUL, 0x00000000001c141cUL,
        0x0000000000382838UL, 0x0000000000705070UL, 0x0000000000e0a0e0UL, 0x0000000000c040c0UL,
        0x0000000003020300UL, 0x0000000007050700UL, 0x000000000e0a0e00UL, 0x000000001c141c00UL,
        0x0000000038283800UL, 0x0000000070507000UL, 0x00000000e0a0e000UL, 0x00000000c040c000UL,
        0x0000000302030000UL, 0x0000000705070000UL, 0x0000000e0a0e0000UL, 0x0000001c141c0000UL,
        0x0000003828380000UL, 0x0000007050700000UL, 0x000000e0a0e00000UL, 0x000000c040c00000UL,
        0x0000030203000000UL, 0x0000070507000000UL, 0x00000e0a0e000000UL, 0x00001c141c000000UL,
        0x0000382838000000UL, 0x0000705070000000UL, 0x0000e0a0e0000000UL, 0x0000c040c0000000UL,
        0x0003020300000000UL, 0x0007050700000000UL, 0x000e0a0e00000000UL, 0x001c141c00000000UL,
        0x0038283800000000UL, 0x0070507000000000UL, 0x00e0a0e000000000UL, 0x00c040c000000000UL,
        0x0302030000000000UL, 0x0705070000000000UL, 0x0e0a0e0000000000UL, 0x1c141c0000000000UL,
        0x3828380000000000UL, 0x7050700000000000UL, 0xe0a0e00000000000UL, 0xc040c00000000000UL,
        0x0203000000000000UL, 0x0507000000000000UL, 0x0a0e000000000000UL, 0x141c000000000000UL,
        0x2838000000000000UL, 0x5070000000000000UL, 0xa0e0000000000000UL, 0x40c0000000000000UL,
    ];

    /// <summary>The final score for the side to move (<c>end_evaluate</c>).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int EndEvaluate(in Board board) => board.ScorePlayer();

    /// <summary>The final score with <paramref name="e"/> empty squares (<c>end_evaluate(board, e)</c>).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int EndEvaluate(in Board board, int e)
    {
        int score = board.CountPlayer() * 2 + e;
        score += (((score >> 6) & 1) + (((score + Common.HW2_M1) >> 7) & 1) - 1) * e;
        return score - Common.HW2;
    }

    /// <summary>The same for an odd number of empty squares (<c>end_evaluate_odd</c>).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int EndEvaluateOdd(in Board board, int e)
    {
        int score = board.CountPlayer() * 2 + e;
        score += (((score >> 5) & 2) - 1) * e;
        return score - Common.HW2;
    }

    /// <summary>The empty squares with no empty neighbour (<c>empty1_bb</c>).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ulong Empty1Bb(ulong player, ulong opponent)
    {
        ulong bn = ~(player | opponent);
        ulong t0 = (bn & 0x7f7f7f7f7f7f7f7fUL) << 1;
        ulong t1 = (bn & 0xfefefefefefefefeUL) >> 1;
        ulong e1 = t0 | t1;
        t0 = bn | e1;
        t1 = t0 << 8;
        t0 >>= 8;
        return ~(e1 | t0 | t1) & bn;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static bool Is1Empty(int place, ulong b) => (AdjacentSq[place] & b) == 0;

    /// <summary>
    /// The discs <paramref name="player"/> flips by playing <paramref name="place"/> when every other square is the
    /// opponent's (<c>count_last_flip</c>); the C++ tables give the same numbers.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static int CountLastFlip(ulong player, int place)
    {
        ulong bit = 1UL << place;
        var flip = new Flip();
        return Bits.PopCount(flip.Calc(player & ~bit, ~player & ~bit, place));
    }

    // ---- stability_cutoff.hpp ----

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int StabilityCut(Search search, ref int alpha, ref int beta)
    {
        if (beta >= SearchConst.StabilityThreshold[search.NDiscs])
        {
            int nBeta = Common.HW2 - 2 * Bits.PopCount(Stability.CalcStability(search.Board.Opponent, search.Board.Player));
            if (nBeta <= alpha)
            {
                return nBeta;
            }

            if (nBeta < beta)
            {
                beta = nBeta;
            }
        }

        return SearchConst.SCORE_UNDEFINED;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int StabilityCutNws(Search search, int alpha)
    {
        if (alpha >= SearchConst.StabilityThresholdNws[search.NDiscs])
        {
            int nBeta = Common.HW2 - 2 * Bits.PopCount(Stability.CalcStability(search.Board.Opponent, search.Board.Player));
            if (nBeta <= alpha)
            {
                return nBeta;
            }
        }

        return SearchConst.SCORE_UNDEFINED;
    }

    // ---- endsearch_last_generic.hpp ----

    /// <summary>The score with one empty square <paramref name="p0"/>; <paramref name="player"/> is the side to move (<c>last1</c>).</summary>
    public static int Last1(Search search, ulong player, int alpha, int p0)
    {
        ++search.NNodes;
        int nFlip = CountLastFlip(player, p0);
        int score = 2 * (Bits.PopCount(player) + nFlip + 1) - Common.HW2;
        if (nFlip == 0)
        {
            ++search.NNodes;
            int score2 = score - 2;
            if (score <= 0)
            {
                score = score2;
            }

            if (score > alpha)
            {
                nFlip = CountLastFlip(~player, p0);
                if (nFlip != 0)
                {
                    score = score2 - 2 * nFlip;
                }
            }
        }

        return score;
    }

    private static int Last2(Search search, int alpha, int beta, int p0, int p1, Board board)
    {
        ++search.NNodes;
        int v;
        var flip = new Flip();
        ulong[] around = SearchConst.BitAround;
        if ((around[p0] & board.Opponent) != 0 && flip.Calc(board.Player, board.Opponent, p0) != 0)
        {
            v = Last1(search, board.Opponent ^ flip.Flipped, alpha, p1);
            if (v > alpha && (around[p1] & board.Opponent) != 0 && flip.Calc(board.Player, board.Opponent, p1) != 0)
            {
                int g = Last1(search, board.Opponent ^ flip.Flipped, alpha, p0);
                if (v > g)
                {
                    v = g;
                }
            }
        }
        else if ((around[p1] & board.Opponent) != 0 && flip.Calc(board.Player, board.Opponent, p1) != 0)
        {
            v = Last1(search, board.Opponent ^ flip.Flipped, alpha, p0);
        }
        else
        {
            ++search.NNodes;
            alpha = -beta;
            if (flip.Calc(board.Opponent, board.Player, p0) != 0)
            {
                v = Last1(search, board.Player ^ flip.Flipped, alpha, p1);
                if (v > alpha && flip.Calc(board.Opponent, board.Player, p1) != 0)
                {
                    int g = Last1(search, board.Player ^ flip.Flipped, alpha, p0);
                    if (v > g)
                    {
                        v = g;
                    }
                }
            }
            else if (flip.Calc(board.Opponent, board.Player, p1) != 0)
            {
                v = Last1(search, board.Player ^ flip.Flipped, alpha, p0);
            }
            else
            {
                v = EndEvaluate(board, 2);
            }

            v = -v;
        }

        return v;
    }

    private static int Last3(Search search, int alpha, int beta, int p0, int p1, int p2, Board board)
    {
        // As in C++, the parity test reads the empty squares of search.Board (the position with 4 empty squares).
        ulong empties = ~(search.Board.Player | search.Board.Opponent);
        if (Is1Empty(p2, empties))
        {
            (p2, p0) = (p0, p2);
        }
        else if (Is1Empty(p1, empties))
        {
            (p1, p0) = (p0, p1);
        }

        var flip = new Flip();
        ulong[] around = SearchConst.BitAround;
        int v = -Common.SCORE_INF;
        int pol = 1;
        do
        {
            ++search.NNodes;
            if ((around[p0] & board.Opponent) != 0 && flip.Calc(board.Player, board.Opponent, p0) != 0)
            {
                v = Last2(search, alpha, beta, p1, p2, board.MoveCopy(flip));
                if (beta <= v)
                {
                    return v * pol;
                }

                if (alpha < v)
                {
                    alpha = v;
                }
            }

            int g;
            if ((around[p1] & board.Opponent) != 0 && flip.Calc(board.Player, board.Opponent, p1) != 0)
            {
                g = Last2(search, alpha, beta, p2, p0, board.MoveCopy(flip));
                if (beta <= g)
                {
                    return g * pol;
                }

                if (v < g)
                {
                    v = g;
                }

                if (alpha < g)
                {
                    alpha = g;
                }
            }

            if ((around[p2] & board.Opponent) != 0 && flip.Calc(board.Player, board.Opponent, p2) != 0)
            {
                g = Last2(search, alpha, beta, p1, p0, board.MoveCopy(flip));
                if (v < g)
                {
                    v = g;
                }

                return v * pol;
            }

            if (v > -Common.SCORE_INF)
            {
                return v * pol;
            }

            board.Pass();
            int t = alpha;
            alpha = -beta;
            beta = -t;
        }
        while ((pol = -pol) < 0);

        return EndEvaluateOdd(board, 3);
    }

    private static void Last4Squares(Search search, out int p0, out int p1, out int p2, out int p3)
    {
        ulong empties = ~(search.Board.Player | search.Board.Opponent);
        ulong e1 = Empty1Bb(search.Board.Player, search.Board.Opponent);
        if (e1 == 0)
        {
            e1 = empties;
        }

        empties &= ~e1;
        p0 = Bits.FirstBit(ref e1);
        if ((e1 &= e1 - 1) == 0)
        {
            e1 = empties;
        }

        p1 = Bits.FirstBit(ref e1);
        if ((e1 &= e1 - 1) == 0)
        {
            e1 = empties;
        }

        p2 = Bits.FirstBit(ref e1);
        p3 = (e1 &= e1 - 1) != 0 ? Bits.FirstBit(ref e1) : Bits.FirstBit(ref empties);
    }

    /// <summary>The exact score with 4 empty squares, for the window (alpha, beta) (<c>last4</c>).</summary>
    public static int Last4(Search search, int alpha, int beta)
    {
        Last4Squares(search, out int p0, out int p1, out int p2, out int p3);
        var flip = new Flip();
        ulong[] around = SearchConst.BitAround;
        Board board4 = search.Board;
        int v = Common.SCORE_INF;
        int pol = 1;
        do
        {
            ++search.NNodes;
            if ((around[p0] & board4.Opponent) != 0 && flip.Calc(board4.Player, board4.Opponent, p0) != 0)
            {
                v = Last3(search, alpha, beta, p1, p2, p3, board4.MoveCopy(flip));
                if (alpha >= v)
                {
                    return v * pol;
                }

                if (beta > v)
                {
                    beta = v;
                }
            }

            int g;
            if ((around[p1] & board4.Opponent) != 0 && flip.Calc(board4.Player, board4.Opponent, p1) != 0)
            {
                g = Last3(search, alpha, beta, p0, p2, p3, board4.MoveCopy(flip));
                if (alpha >= g)
                {
                    return g * pol;
                }

                if (v > g)
                {
                    v = g;
                }

                if (beta > g)
                {
                    beta = g;
                }
            }

            if ((around[p2] & board4.Opponent) != 0 && flip.Calc(board4.Player, board4.Opponent, p2) != 0)
            {
                g = Last3(search, alpha, beta, p0, p1, p3, board4.MoveCopy(flip));
                if (alpha >= g)
                {
                    return g * pol;
                }

                if (v > g)
                {
                    v = g;
                }

                if (beta > g)
                {
                    beta = g;
                }
            }

            if ((around[p3] & board4.Opponent) != 0 && flip.Calc(board4.Player, board4.Opponent, p3) != 0)
            {
                g = Last3(search, alpha, beta, p0, p1, p2, board4.MoveCopy(flip));
                if (v > g)
                {
                    v = g;
                }

                return v * pol;
            }

            if (v < Common.SCORE_INF)
            {
                return v * pol;
            }

            board4.Pass();
            int t = alpha;
            alpha = -beta;
            beta = -t;
        }
        while ((pol = -pol) < 0);

        return -EndEvaluate(search.Board, 4);
    }

    // ---- endsearch_nws_last_generic.hpp ----

    private static int Last2Nws(Search search, int alpha, int p0, int p1, Board board)
    {
        ++search.NNodes;
        int v;
        var flip = new Flip();
        ulong[] around = SearchConst.BitAround;
        if ((around[p0] & board.Opponent) != 0 && flip.Calc(board.Player, board.Opponent, p0) != 0)
        {
            v = Last1(search, board.Opponent ^ flip.Flipped, alpha, p1);
            if (v > alpha && (around[p1] & board.Opponent) != 0 && flip.Calc(board.Player, board.Opponent, p1) != 0)
            {
                int g = Last1(search, board.Opponent ^ flip.Flipped, alpha, p0);
                if (v > g)
                {
                    v = g;
                }
            }
        }
        else if ((around[p1] & board.Opponent) != 0 && flip.Calc(board.Player, board.Opponent, p1) != 0)
        {
            v = Last1(search, board.Opponent ^ flip.Flipped, alpha, p0);
        }
        else
        {
            ++search.NNodes;
            alpha = -alpha - 1;
            if (flip.Calc(board.Opponent, board.Player, p0) != 0)
            {
                v = Last1(search, board.Player ^ flip.Flipped, alpha, p1);
                if (v > alpha && flip.Calc(board.Opponent, board.Player, p1) != 0)
                {
                    int g = Last1(search, board.Player ^ flip.Flipped, alpha, p0);
                    if (v > g)
                    {
                        v = g;
                    }
                }
            }
            else if (flip.Calc(board.Opponent, board.Player, p1) != 0)
            {
                v = Last1(search, board.Player ^ flip.Flipped, alpha, p0);
            }
            else
            {
                v = EndEvaluate(board, 2);
            }

            v = -v;
        }

        return v;
    }

    private static int Last3Nws(Search search, int alpha, int p0, int p1, int p2, Board board)
    {
        ulong empties = ~(search.Board.Player | search.Board.Opponent);
        if (Is1Empty(p2, empties))
        {
            (p2, p0) = (p0, p2);
        }
        else if (Is1Empty(p1, empties))
        {
            (p1, p0) = (p0, p1);
        }

        var flip = new Flip();
        ulong[] around = SearchConst.BitAround;
        int v = -Common.SCORE_INF;
        int pol = 1;
        do
        {
            ++search.NNodes;
            if ((around[p0] & board.Opponent) != 0 && flip.Calc(board.Player, board.Opponent, p0) != 0)
            {
                v = Last2Nws(search, alpha, p1, p2, board.MoveCopy(flip));
                if (alpha < v)
                {
                    return v * pol;
                }
            }

            int g;
            if ((around[p1] & board.Opponent) != 0 && flip.Calc(board.Player, board.Opponent, p1) != 0)
            {
                g = Last2Nws(search, alpha, p2, p0, board.MoveCopy(flip));
                if (alpha < g)
                {
                    return g * pol;
                }

                if (v < g)
                {
                    v = g;
                }
            }

            if ((around[p2] & board.Opponent) != 0 && flip.Calc(board.Player, board.Opponent, p2) != 0)
            {
                g = Last2Nws(search, alpha, p1, p0, board.MoveCopy(flip));
                if (v < g)
                {
                    v = g;
                }

                return v * pol;
            }

            if (v > -Common.SCORE_INF)
            {
                return v * pol;
            }

            board.Pass();
            alpha = -alpha - 1;
        }
        while ((pol = -pol) < 0);

        return EndEvaluateOdd(board, 3);
    }

    /// <summary>The null-window search with 4 empty squares (<c>last4_nws</c>).</summary>
    public static int Last4Nws(Search search, int alpha)
    {
        Last4Squares(search, out int p0, out int p1, out int p2, out int p3);
        var flip = new Flip();
        ulong[] around = SearchConst.BitAround;
        Board board4 = search.Board;
        int v = Common.SCORE_INF;
        int pol = 1;
        do
        {
            ++search.NNodes;
            if ((around[p0] & board4.Opponent) != 0 && flip.Calc(board4.Player, board4.Opponent, p0) != 0)
            {
                v = Last3Nws(search, alpha, p1, p2, p3, board4.MoveCopy(flip));
                if (alpha >= v)
                {
                    return v * pol;
                }
            }

            int g;
            if ((around[p1] & board4.Opponent) != 0 && flip.Calc(board4.Player, board4.Opponent, p1) != 0)
            {
                g = Last3Nws(search, alpha, p0, p2, p3, board4.MoveCopy(flip));
                if (alpha >= g)
                {
                    return g * pol;
                }

                if (v > g)
                {
                    v = g;
                }
            }

            if ((around[p2] & board4.Opponent) != 0 && flip.Calc(board4.Player, board4.Opponent, p2) != 0)
            {
                g = Last3Nws(search, alpha, p0, p1, p3, board4.MoveCopy(flip));
                if (alpha >= g)
                {
                    return g * pol;
                }

                if (v > g)
                {
                    v = g;
                }
            }

            if ((around[p3] & board4.Opponent) != 0 && flip.Calc(board4.Player, board4.Opponent, p3) != 0)
            {
                g = Last3Nws(search, alpha, p0, p1, p2, board4.MoveCopy(flip));
                if (v > g)
                {
                    v = g;
                }

                return v * pol;
            }

            if (v < Common.SCORE_INF)
            {
                return v * pol;
            }

            board4.Pass();
            alpha = -alpha - 1;
        }
        while ((pol = -pol) < 0);

        return -EndEvaluate(search.Board, 4);
    }

    // ---- endsearch_nws.hpp ----

    /// <summary>The null-window search from 5 or 6 empty squares (<c>nega_alpha_end_fast_nws</c>).</summary>
    public static int NegaAlphaEndFastNws(Search search, int alpha, bool skipped)
    {
        ++search.NNodes;
        if (!skipped)
        {
            int stabRes = StabilityCutNws(search, alpha);
            if (stabRes != SearchConst.SCORE_UNDEFINED)
            {
                return stabRes;
            }
        }

        ulong legal = search.Board.GetLegal();
        if (legal == 0)
        {
            if (skipped)
            {
                return EndEvaluate(search.Board);
            }

            search.PassNoeval();
            int vPass = -NegaAlphaEndFastNws(search, -alpha - 1, true);
            search.PassNoeval();
            return vPass;
        }

        Board board0 = search.Board;
        int v = -Common.SCORE_INF;
        int g;
        var flip = new Flip();
        ulong prioritymoves = legal & Empty1Bb(search.Board.Player, search.Board.Opponent);
        if (prioritymoves == 0)
        {
            prioritymoves = legal;
        }

        if (search.NDiscs == 59)
        {
            do
            {
                legal ^= prioritymoves;
                for (int cell = Bits.FirstBit(ref prioritymoves); prioritymoves != 0; cell = Bits.NextBit(ref prioritymoves))
                {
                    flip.Calc(board0.Player, board0.Opponent, cell);
                    search.Board = board0.MoveCopy(flip);
                    g = Last4Nws(search, alpha);
                    if (alpha < g)
                    {
                        search.Board = board0;
                        return g;
                    }

                    if (v < g)
                    {
                        v = g;
                    }
                }
            }
            while ((prioritymoves = legal) != 0);
        }
        else
        {
            ++search.NDiscs;
            do
            {
                legal ^= prioritymoves;
                for (int cell = Bits.FirstBit(ref prioritymoves); prioritymoves != 0; cell = Bits.NextBit(ref prioritymoves))
                {
                    flip.Calc(board0.Player, board0.Opponent, cell);
                    search.Board = board0.MoveCopy(flip);
                    g = -NegaAlphaEndFastNws(search, -alpha - 1, false);
                    if (alpha < g)
                    {
                        --search.NDiscs;
                        search.Board = board0;
                        return g;
                    }

                    if (v < g)
                    {
                        v = g;
                    }
                }
            }
            while ((prioritymoves = legal) != 0);

            --search.NDiscs;
        }

        search.Board = board0;
        return v;
    }

    /// <summary>The null-window search from 7 to 10 empty squares (<c>nega_alpha_end_simple_nws</c>).</summary>
    public static int NegaAlphaEndSimpleNws(Search search, int alpha, bool skipped, ulong legal)
    {
        if (search.NDiscs >= Common.HW2 - SearchConst.END_FAST_DEPTH)
        {
            return NegaAlphaEndFastNws(search, alpha, skipped);
        }

        ++search.NNodes;
        if (!skipped)
        {
            int stabRes = StabilityCutNws(search, alpha);
            if (stabRes != SearchConst.SCORE_UNDEFINED)
            {
                return stabRes;
            }
        }

        if (legal == Common.LEGAL_UNDEFINED)
        {
            legal = search.Board.GetLegal();
        }

        int v = -Common.SCORE_INF;
        if (legal == 0)
        {
            if (skipped)
            {
                return EndEvaluate(search.Board);
            }

            search.PassNoeval();
            v = -NegaAlphaEndSimpleNws(search, -alpha - 1, true, Common.LEGAL_UNDEFINED);
            search.PassNoeval();
            return v;
        }

        int canput = Bits.PopCount(legal);
        Span<FlipValue> moveList = stackalloc FlipValue[SearchConst.END_SIMPLE_DEPTH];
        int idx = 0;
        for (int cell = Bits.FirstBit(ref legal); legal != 0; cell = Bits.NextBit(ref legal))
        {
            moveList[idx].Reset();
            moveList[idx].Flip.Calc(search.Board.Player, search.Board.Opponent, cell);
            if (moveList[idx].Flip.Flipped == search.Board.Opponent)
            {
                return Common.SCORE_MAX;
            }

            ++idx;
        }

        int g;
        ulong done = 0;
        LocalTranspositionTable table = search.Context.LocalTable;
        if (canput > 1)
        {
            for (int i = 0; i < canput; ++i)
            {
                ref FlipValue flipValue = ref moveList[i];
                flipValue.Value = 0;
                if ((search.Parity & SearchConst.CellDiv4[flipValue.Flip.Pos]) != 0)
                {
                    flipValue.Value += MoveOrdering.W_END_NWS_SIMPLE_PARITY;
                }

                search.MoveNoeval(flipValue.Flip);
                Board nboard = search.Board;
                ref LocalTranspositionTable.Entry tt = ref table.Get(nboard, search.NDiscs);
                if (tt.Cmp(nboard))
                {
                    if (alpha < tt.Lower)
                    {
                        v = tt.Lower;
                        search.UndoNoeval(flipValue.Flip);
                        return v;
                    }

                    if (tt.Upper <= alpha)
                    {
                        if (v < tt.Upper)
                        {
                            v = tt.Upper;
                        }

                        done |= 1UL << flipValue.Flip.Pos;
                        search.UndoNoeval(flipValue.Flip);
                        continue;
                    }
                }

                flipValue.NLegal = search.Board.GetLegal();
                int nm = MoveOrdering.GetNMovesCornerX2(flipValue.NLegal);
                if (nm <= 1)
                {
                    g = -NegaAlphaEndSimpleNws(search, -alpha - 1, false, flipValue.NLegal);
                    search.UndoNoeval(flipValue.Flip);
                    if (v < g)
                    {
                        v = g;
                        if (alpha < v)
                        {
                            tt.SetScore(nboard, v, 64);
                            return v;
                        }
                    }

                    tt.SetScore(nboard, -64, g);
                    done |= 1UL << flipValue.Flip.Pos;
                    continue;
                }

                flipValue.Value += (MoveOrdering.MO_OFFSET_L_PM - nm) * MoveOrdering.W_END_NWS_SIMPLE_MOBILITY;
                search.UndoNoeval(flipValue.Flip);
            }
        }

        for (int moveIdx = 0; moveIdx < canput; ++moveIdx)
        {
            MoveOrdering.SwapNextBestMove(moveList, moveIdx, canput);
            if (((1UL << moveList[moveIdx].Flip.Pos) & done) != 0)
            {
                continue;
            }

            search.MoveNoeval(moveList[moveIdx].Flip);
            Board nboard = search.Board;
            ref LocalTranspositionTable.Entry tt = ref table.Get(nboard, search.NDiscs);
            g = -NegaAlphaEndSimpleNws(search, -alpha - 1, false, moveList[moveIdx].NLegal);
            search.UndoNoeval(moveList[moveIdx].Flip);
            if (v < g)
            {
                v = g;
                if (alpha < v)
                {
                    tt.SetScore(nboard, v, 64);
                    break;
                }
            }

            tt.SetScore(nboard, -64, g);
        }

        return v;
    }

    /// <summary>The null-window search from 11 to 13 empty squares (<c>nega_alpha_end_nws</c>).</summary>
    public static int NegaAlphaEndNws(Search search, int alpha, bool skipped, ulong legal)
    {
        if (search.NDiscs >= Common.HW2 - SearchConst.END_SIMPLE_DEPTH)
        {
            return NegaAlphaEndSimpleNws(search, alpha, skipped, legal);
        }

        ++search.NNodes;
        if (!skipped)
        {
            int stabRes = StabilityCutNws(search, alpha);
            if (stabRes != SearchConst.SCORE_UNDEFINED)
            {
                return stabRes;
            }
        }

        if (legal == Common.LEGAL_UNDEFINED)
        {
            legal = search.Board.GetLegal();
        }

        int v = -Common.SCORE_INF;
        if (legal == 0)
        {
            if (skipped)
            {
                return EndEvaluate(search.Board);
            }

            search.PassEndsearch();
            v = -NegaAlphaEndNws(search, -alpha - 1, true, Common.LEGAL_UNDEFINED);
            search.PassEndsearch();
            return v;
        }

        int g;
        int canput = Bits.PopCount(legal);
        Span<FlipValue> moveList = stackalloc FlipValue[SearchConst.MAX_N_BRANCHES];
        int idx = 0;
        for (int cell = Bits.FirstBit(ref legal); legal != 0; cell = Bits.NextBit(ref legal))
        {
            moveList[idx].Reset();
            moveList[idx].Flip.Calc(search.Board.Player, search.Board.Opponent, cell);
            if (moveList[idx].Flip.Flipped == search.Board.Opponent)
            {
                return Common.SCORE_MAX;
            }

            ++idx;
        }

        ulong done = 0;
        LocalTranspositionTable table = search.Context.LocalTable;
        if (canput > 1)
        {
            for (int i = 0; i < canput; ++i)
            {
                ref FlipValue flipValue = ref moveList[i];
                flipValue.Value = 0;
                search.MoveEndsearch(flipValue.Flip);
                Board nboard = search.Board;
                ref LocalTranspositionTable.Entry tt = ref table.Get(nboard, search.NDiscs);
                if (tt.Cmp(nboard))
                {
                    if (alpha < tt.Lower)
                    {
                        v = tt.Lower;
                        search.UndoEndsearch(flipValue.Flip);
                        return v;
                    }

                    if (tt.Upper <= alpha)
                    {
                        if (v < tt.Upper)
                        {
                            v = tt.Upper;
                        }

                        done |= 1UL << flipValue.Flip.Pos;
                        search.UndoEndsearch(flipValue.Flip);
                        continue;
                    }
                }

                flipValue.NLegal = search.Board.GetLegal();
                int nm = MoveOrdering.GetNMovesCornerX2(flipValue.NLegal);
                if (nm <= 1)
                {
                    g = -NegaAlphaEndNws(search, -alpha - 1, false, flipValue.NLegal);
                    search.UndoEndsearch(flipValue.Flip);
                    if (v < g)
                    {
                        v = g;
                        if (alpha < v)
                        {
                            tt.SetScore(nboard, v, 64);
                            return v;
                        }
                    }

                    tt.SetScore(nboard, -64, g);
                    done |= 1UL << flipValue.Flip.Pos;
                    continue;
                }

                flipValue.Value += (MoveOrdering.MO_OFFSET_L_PM - nm) * MoveOrdering.W_END_NWS_MOBILITY;
                flipValue.Value += (MoveOrdering.MO_OFFSET_L_PM - Evaluate.MidEvaluateMoveOrderingEnd(search.Eval)) * MoveOrdering.W_END_NWS_VALUE;
                search.UndoEndsearch(flipValue.Flip);
            }
        }

        for (int moveIdx = 0; moveIdx < canput; ++moveIdx)
        {
            MoveOrdering.SwapNextBestMove(moveList, moveIdx, canput);
            if (((1UL << moveList[moveIdx].Flip.Pos) & done) != 0)
            {
                continue;
            }

            search.MoveEndsearch(moveList[moveIdx].Flip);
            Board nboard = search.Board;
            ref LocalTranspositionTable.Entry tt = ref table.Get(nboard, search.NDiscs);
            g = -NegaAlphaEndNws(search, -alpha - 1, false, moveList[moveIdx].NLegal);
            search.UndoEndsearch(moveList[moveIdx].Flip);
            if (v < g)
            {
                v = g;
                if (alpha < v)
                {
                    tt.SetScore(nboard, v, 64);
                    break;
                }
            }

            tt.SetScore(nboard, -64, g);
        }

        return v;
    }
}
