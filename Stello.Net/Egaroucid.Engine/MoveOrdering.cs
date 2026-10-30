// SPDX-License-Identifier: GPL-3.0-only
// Ported from Egaroucid src/engine/move_ordering.hpp (https://github.com/Nyanyan/Egaroucid), © Takuto Yamana,
// GPL-3.0-or-later. Killer and history moves are switched off in Egaroucid (USE_KILLER_MOVE_MO false) and not ported.

using System.Runtime.CompilerServices;

namespace Stello.Egaroucid;

/// <summary>Move ordering (move_ordering.hpp): the order in which a node tries its moves.</summary>
internal static class MoveOrdering
{
    public const int W_WIPEOUT = 100000000;
    public const int W_1ST_MOVE = 10000000;
    public const int W_2ND_MOVE = 1000000;
    public const int MO_OFFSET_L_PM = 38;

    public const int W_MOBILITY = 35;
    public const int W_POTENTIAL_MOBILITY = 17;
    public const int W_TT_BONUS = 485;
    public const int W_VALUE = 269;
    public const int W_VALUE_DEEP_ADDITIONAL = 94;
    public const int W_NWS_MOBILITY = 17;
    public const int W_NWS_TT_BONUS = 204;
    public const int W_NWS_VALUE = 7;
    public const int W_NWS_VALUE_DEEP_ADDITIONAL = 25;
    public const int W_END_NWS_MOBILITY = 40;
    public const int W_END_NWS_VALUE = 12;
    public const int W_END_NWS_SIMPLE_MOBILITY = 18;
    public const int W_END_NWS_SIMPLE_PARITY = 17;

    public const int MOVE_ORDERING_VALUE_OFFSET_ALPHA = 12;
    public const int MOVE_ORDERING_VALUE_OFFSET_BETA = 8;
    public const int MOVE_ORDERING_NWS_VALUE_OFFSET_ALPHA = 16;
    public const int MOVE_ORDERING_NWS_VALUE_OFFSET_BETA = 6;
    public const int MOVE_ORDERING_MPC_LEVEL = Level.MPC_74;

    /// <summary>The number of corner moves (generic <c>get_corner_n_moves</c>).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int GetCornerNMoves(ulong legal)
    {
        int res = (int)((legal & 0b10000001UL) + ((legal >> 56) & 0b10000001UL));
        return (res & 0b11) + (res >> 7);
    }

    /// <summary>Twice the number of moves, plus the corner moves (<c>get_weighted_n_moves</c>).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int GetWeightedNMoves(ulong legal) => Bits.PopCount(legal) * 2 + GetCornerNMoves(legal);

    /// <summary>The number of moves with corners counted twice (generic <c>get_n_moves_cornerX2</c>).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int GetNMovesCornerX2(ulong legal)
    {
        ulong b = legal;
        ulong c = b & 0x0100000000000001UL;
        b -= (b >> 1) & 0x1555555555555515UL;
        b = (b & 0x3333333333333333UL) + ((b >> 2) & 0x3333333333333333UL);
        b += c;
        b += b >> 4;
        b &= 0x0f0f0f0f0f0f0f0fUL;
        b *= 0x0101010101010101UL;
        return (int)(b >> 56);
    }

    /// <summary>The empty squares next to <paramref name="discs"/> (<c>get_potential_mobility</c>).</summary>
    public static int GetPotentialMobility(ulong discs, ulong empties)
    {
        ulong hmask = discs & 0x7E7E7E7E7E7E7E7EUL;
        ulong vmask = discs & 0x00FFFFFFFFFFFF00UL;
        ulong hvmask = discs & 0x007E7E7E7E7E7E00UL;
        ulong res =
            (hmask << 1) | (hmask >> 1) |
            (vmask << Common.HW) | (vmask >> Common.HW) |
            (hvmask << Common.HW_M1) | (hvmask >> Common.HW_M1) |
            (hvmask << Common.HW_P1) | (hvmask >> Common.HW_P1);
        return Bits.PopCount(empties & res);
    }

    private static void MoveEvaluate(Search search, ref FlipValue flipValue, int alpha, int beta, int depth, SearchingFlag searching)
    {
        flipValue.Value = 0;
        search.Move(flipValue.Flip);
        flipValue.NLegal = search.Board.GetLegal();
        flipValue.Value += (MO_OFFSET_L_PM - GetWeightedNMoves(flipValue.NLegal)) * W_MOBILITY;
        flipValue.Value += (MO_OFFSET_L_PM - GetPotentialMobility(search.Board.Opponent, ~(search.Board.Player | search.Board.Opponent))) * W_POTENTIAL_MOBILITY;
        switch (depth)
        {
            case 0:
                flipValue.Value += (Common.SCORE_MAX - MidSearch.MidEvaluateDiff(search)) * W_VALUE;
                break;
            case 1:
                flipValue.Value += (Common.SCORE_MAX - MidSearch.NegaAlphaEval1(search, alpha, beta, false)) * (W_VALUE + W_VALUE_DEEP_ADDITIONAL);
                break;
            default:
                int mpcLevel = search.MpcLevel;
                search.MpcLevel = MOVE_ORDERING_MPC_LEVEL;
                flipValue.Value += (Common.SCORE_MAX - MidSearch.NegaScout(search, alpha, beta, depth, false, flipValue.NLegal, false, searching)) * (W_VALUE + depth * W_VALUE_DEEP_ADDITIONAL);
                search.MpcLevel = mpcLevel;
                break;
        }

        search.Undo(flipValue.Flip);
    }

    private static void MoveEvaluateNws(Search search, ref FlipValue flipValue, int alpha, int beta, int depth, SearchingFlag searching)
    {
        flipValue.Value = 0;
        search.Move(flipValue.Flip);
        flipValue.NLegal = search.Board.GetLegal();
        flipValue.Value += (MO_OFFSET_L_PM - GetWeightedNMoves(flipValue.NLegal)) * W_NWS_MOBILITY;
        switch (depth)
        {
            case 0:
                flipValue.Value += (Common.SCORE_MAX - MidSearch.MidEvaluateDiff(search)) * W_NWS_VALUE;
                break;
            case 1:
                flipValue.Value += (Common.SCORE_MAX - MidSearch.NegaAlphaEval1(search, alpha, beta, false)) * (W_NWS_VALUE + W_NWS_VALUE_DEEP_ADDITIONAL);
                break;
            default:
                int mpcLevel = search.MpcLevel;
                search.MpcLevel = MOVE_ORDERING_MPC_LEVEL;
                flipValue.Value += (Common.SCORE_MAX - MidSearch.NegaScout(search, alpha, beta, depth, false, flipValue.NLegal, false, searching)) * (W_NWS_VALUE + depth * W_NWS_VALUE_DEEP_ADDITIONAL);
                search.MpcLevel = mpcLevel;
                break;
        }

        search.Undo(flipValue.Flip);
    }

    /// <summary>Moves the best of <paramref name="moveList"/>[strt..siz) to strt; the first of equal values wins (<c>swap_next_best_move</c>).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void SwapNextBestMove(Span<FlipValue> moveList, int strt, int siz)
    {
        if (strt == siz - 1)
        {
            return;
        }

        int topIdx = strt;
        int bestValue = moveList[strt].Value;
        for (int i = strt + 1; i < siz; ++i)
        {
            if (bestValue < moveList[i].Value)
            {
                bestValue = moveList[i].Value;
                topIdx = i;
            }
        }

        if (topIdx != strt)
        {
            (moveList[strt], moveList[topIdx]) = (moveList[topIdx], moveList[strt]);
        }
    }

    /// <summary>Gives every move a value (<c>move_list_evaluate</c>). Moves already done (no flips) are left alone.</summary>
    public static void MoveListEvaluate(Search search, Span<FlipValue> moveList, int canput, ReadOnlySpan<int> moves, int depth, int alpha, int beta, SearchingFlag searching)
    {
        if (canput == 1)
        {
            return;
        }

        int evalAlpha = -Math.Min(Common.SCORE_MAX, beta + MOVE_ORDERING_VALUE_OFFSET_BETA);
        int evalBeta = -Math.Max(-Common.SCORE_MAX, alpha - MOVE_ORDERING_VALUE_OFFSET_ALPHA);
        int evalDepth = depth >> 3;
        if (depth >= 25 && search.MpcLevel < Level.MPC_100)
        {
            evalDepth = ((depth / 3) & 0b11111110) + (depth & 1);
        }

        for (int i = 0; i < canput; ++i)
        {
            if (moveList[i].Flip.Flipped != 0)
            {
                if (moveList[i].Flip.Pos == moves[0])
                {
                    moveList[i].Value = W_1ST_MOVE;
                }
                else if (moveList[i].Flip.Pos == moves[1])
                {
                    moveList[i].Value = W_2ND_MOVE;
                }
                else
                {
                    MoveEvaluate(search, ref moveList[i], evalAlpha, evalBeta, evalDepth, searching);
                }
            }
        }
    }

    /// <summary>The same for a null-window search (<c>move_list_evaluate_nws</c>).</summary>
    public static void MoveListEvaluateNws(Search search, Span<FlipValue> moveList, int canput, ReadOnlySpan<int> moves, int depth, int alpha, SearchingFlag searching)
    {
        if (canput <= 1)
        {
            return;
        }

        int evalAlpha = -Math.Min(Common.SCORE_MAX, alpha + MOVE_ORDERING_NWS_VALUE_OFFSET_BETA);
        int evalBeta = -Math.Max(-Common.SCORE_MAX, alpha - MOVE_ORDERING_NWS_VALUE_OFFSET_ALPHA);
        int evalDepth = depth >> 4;
        for (int i = 0; i < canput; ++i)
        {
            if (moveList[i].Flip.Flipped != 0)
            {
                if (moveList[i].Flip.Pos == moves[0])
                {
                    moveList[i].Value = W_1ST_MOVE;
                }
                else if (moveList[i].Flip.Pos == moves[1])
                {
                    moveList[i].Value = W_2ND_MOVE;
                }
                else
                {
                    MoveEvaluateNws(search, ref moveList[i], evalAlpha, evalBeta, evalDepth, searching);
                }
            }
        }
    }
}
