// SPDX-License-Identifier: GPL-3.0-only
// Ported from Egaroucid src/engine/midsearch.hpp, midsearch_nws.hpp and transposition_cutoff.hpp
// (https://github.com/Nyanyan/Egaroucid), © Takuto Yamana, GPL-3.0-or-later. The young-brothers-wait (YBWC) branches,
// taken only when a search is multi-threaded, are not ported yet (Plans/egaroucid-port-plan.md, phase 6).

using System.Runtime.CompilerServices;

namespace Stello.Egaroucid;

/// <summary>
/// The midgame search (midsearch.hpp, midsearch_nws.hpp): principal variation search with null-window searches, the
/// transposition table, enhanced transposition cutoff from depth 16 and Multi-ProbCut from depth 3. The same code runs
/// the endgame with more than 13 empty squares (or 9 with Multi-ProbCut).
/// </summary>
internal static class MidSearch
{
    public const int MID_ETC_DEPTH = 16;
    public const int MID_ETC_DEPTH_NWS = 16;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int MidEvaluateDiff(Search search) =>
        Evaluate.MidEvaluateDiff(search.Eval, search.NDiscs, search.Board.CountPlayer());

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static bool IsSearching(Search search, SearchingFlag searching) => search.Context.GlobalSearching && searching.Value;

    // ---- transposition_cutoff.hpp ----

    private static bool TranspositionCutoff(Search search, uint hashCode, int depth, ref int alpha, ref int beta, ref int v, Span<int> moves)
    {
        int lower = -Common.SCORE_MAX, upper = Common.SCORE_MAX;
        search.Context.TranspositionTable.Get(search, hashCode, depth, ref lower, ref upper, moves);
        if (upper == lower || upper <= alpha)
        {
            v = upper;
            return true;
        }

        if (beta <= lower)
        {
            v = lower;
            return true;
        }

        if (alpha < lower)
        {
            alpha = lower;
        }

        if (upper < beta)
        {
            beta = upper;
        }

        return false;
    }

    private static bool TranspositionCutoffNws(Search search, uint hashCode, int depth, int alpha, ref int v, Span<int> moves)
    {
        int lower = -Common.SCORE_MAX, upper = Common.SCORE_MAX;
        search.Context.TranspositionTable.Get(search, hashCode, depth, ref lower, ref upper, moves);
        if (upper == lower || upper <= alpha)
        {
            v = upper;
            return true;
        }

        if (alpha < lower)
        {
            v = lower;
            return true;
        }

        return false;
    }

    private static bool Etc(Search search, Span<FlipValue> moveList, int canput, int depth, ref int alpha, ref int beta, ref int v, out int nEtcDone)
    {
        nEtcDone = 0;
        TranspositionTable table = search.Context.TranspositionTable;
        for (int i = 0; i < canput; ++i)
        {
            int l = -Common.SCORE_MAX;
            int u = Common.SCORE_MAX;
            search.Move(moveList[i].Flip);
            if (table.HasNodeAnyLevelGetBounds(search, search.Board.Hash(), depth - 1, ref l, ref u))
            {
                moveList[i].Value = MoveOrdering.W_TT_BONUS;
            }

            search.Undo(moveList[i].Flip);
            if (beta <= -u)
            {
                v = -u;
                return true;
            }

            if (alpha <= -u && -u < beta)
            {
                alpha = -u;
                v = -u;
                if (-l <= v || u == l)
                {
                    moveList[i].Flip.Flipped = 0;
                    moveList[i].Value = -Common.INF;
                    ++nEtcDone;
                }
            }
            else if (-l <= alpha)
            {
                v = Math.Max(v, -l);
                moveList[i].Flip.Flipped = 0;
                moveList[i].Value = -Common.INF;
                ++nEtcDone;
            }
        }

        return false;
    }

    private static bool EtcNws(Search search, Span<FlipValue> moveList, int canput, int depth, int alpha, ref int v, out int nEtcDone)
    {
        nEtcDone = 0;
        TranspositionTable table = search.Context.TranspositionTable;
        for (int i = 0; i < canput; ++i)
        {
            int l = -Common.SCORE_MAX;
            int u = Common.SCORE_MAX;
            search.Move(moveList[i].Flip);
            if (table.HasNodeAnyLevelGetBounds(search, search.Board.Hash(), depth - 1, ref l, ref u))
            {
                moveList[i].Value = MoveOrdering.W_NWS_TT_BONUS;
            }

            search.Undo(moveList[i].Flip);
            if (alpha < -u)
            {
                v = -u;
                return true;
            }

            if (-alpha <= l)
            {
                if (v < -l)
                {
                    v = -l;
                }

                moveList[i].Flip.Flipped = 0;
                moveList[i].Value = -Common.INF;
                ++nEtcDone;
            }
        }

        return false;
    }

    // ---- midsearch_nws.hpp ----

    private static int NegaAlphaEval1Nws(Search search, int alpha, bool skipped)
    {
        ++search.NNodes;
        int v = -Common.SCORE_INF;
        ulong legal = search.Board.GetLegal();
        if (legal == 0)
        {
            if (skipped)
            {
                return EndSearch.EndEvaluate(search.Board);
            }

            search.Pass();
            v = -NegaAlphaEval1Nws(search, -alpha - 1, true);
            search.Pass();
            return v;
        }

        var flip = new Flip();
        for (int i = 0; i < SearchConst.N_STATIC_CELL_PRIORITY; ++i)
        {
            ulong l = legal & SearchConst.StaticCellPriority[i];
            for (int cell = Bits.FirstBit(ref l); l != 0; cell = Bits.NextBit(ref l))
            {
                flip.Calc(search.Board.Player, search.Board.Opponent, cell);
                search.Move(flip);
                ++search.NNodes;
                int g = -MidEvaluateDiff(search);
                search.Undo(flip);
                if (v < g)
                {
                    if (alpha < g)
                    {
                        return g;
                    }

                    v = g;
                }
            }
        }

        return v;
    }

    private static int NegaAlphaEval2Nws(Search search, int alpha, bool skipped, ulong legal, SearchingFlag searching)
    {
        if (!IsSearching(search, searching))
        {
            return SearchConst.SCORE_UNDEFINED;
        }

        ++search.NNodes;
        if (legal == Common.LEGAL_UNDEFINED)
        {
            legal = search.Board.GetLegal();
        }

        int v = -Common.SCORE_INF;
        if (legal == 0)
        {
            if (skipped)
            {
                return EndSearch.EndEvaluate(search.Board);
            }

            search.Pass();
            v = -NegaAlphaEval2Nws(search, -alpha - 1, true, Common.LEGAL_UNDEFINED, searching);
            search.Pass();
            return v;
        }

        uint hashCode = search.Board.Hash();
        Span<int> moves = [SearchConst.MOVE_UNDEFINED, SearchConst.MOVE_UNDEFINED];
        if (TranspositionCutoffNws(search, hashCode, 2, alpha, ref v, moves))
        {
            return v;
        }

        int g;
        var flip = new Flip();
        for (int i = 0; i < TranspositionTable.N_TRANSPOSITION_MOVES; ++i)
        {
            if (moves[i] != SearchConst.MOVE_UNDEFINED)
            {
                flip.Calc(search.Board.Player, search.Board.Opponent, moves[i]);
                search.Move(flip);
                g = -NegaAlphaEval1Nws(search, -alpha - 1, false);
                search.Undo(flip);
                if (!searching.Value)
                {
                    return SearchConst.SCORE_UNDEFINED;
                }

                legal ^= 1UL << moves[i];
                if (v < g)
                {
                    v = g;
                    if (alpha < g)
                    {
                        break;
                    }
                }
            }
        }

        for (int i = 0; i < SearchConst.N_STATIC_CELL_PRIORITY && v <= alpha; ++i)
        {
            ulong l = legal & SearchConst.StaticCellPriority[i];
            for (int cell = Bits.FirstBit(ref l); l != 0; cell = Bits.NextBit(ref l))
            {
                flip.Calc(search.Board.Player, search.Board.Opponent, cell);
                search.Move(flip);
                g = -NegaAlphaEval1Nws(search, -alpha - 1, false);
                search.Undo(flip);
                if (!searching.Value)
                {
                    return SearchConst.SCORE_UNDEFINED;
                }

                if (v < g)
                {
                    v = g;
                    if (alpha < g)
                    {
                        break;
                    }
                }
            }
        }

        return v;
    }

    private static int NegaAlphaOrderingNwsSimple(Search search, int alpha, int depth, bool skipped, ulong legal, SearchingFlag searching)
    {
        if (!IsSearching(search, searching))
        {
            return SearchConst.SCORE_UNDEFINED;
        }

        if (depth == 2)
        {
            return NegaAlphaEval2Nws(search, alpha, skipped, legal, searching);
        }

        if (depth == 1)
        {
            return NegaAlphaEval1Nws(search, alpha, skipped);
        }

        if (depth == 0)
        {
            ++search.NNodes;
            return MidEvaluateDiff(search);
        }

        ++search.NNodes;
        if (legal == Common.LEGAL_UNDEFINED)
        {
            legal = search.Board.GetLegal();
        }

        int v = -Common.SCORE_INF;
        if (legal == 0)
        {
            if (skipped)
            {
                return EndSearch.EndEvaluate(search.Board);
            }

            search.Pass();
            v = -NegaAlphaOrderingNwsSimple(search, -alpha - 1, depth, true, Common.LEGAL_UNDEFINED, searching);
            search.Pass();
            return v;
        }

        uint hashCode = search.Board.Hash();
        Span<int> moves = [SearchConst.MOVE_UNDEFINED, SearchConst.MOVE_UNDEFINED];
        if (TranspositionCutoffNws(search, hashCode, depth, alpha, ref v, moves))
        {
            return v;
        }

        if (search.MpcLevel < Level.MPC_100 && depth >= MultiProbCut.USE_MPC_MIN_DEPTH)
        {
            if (MultiProbCut.Mpc(search, alpha, alpha + 1, depth, legal, false, ref v, searching))
            {
                return v;
            }
        }

        int bestMove = SearchConst.MOVE_UNDEFINED;
        int g;
        int canput = Bits.PopCount(legal);
        Span<FlipValue> moveList = stackalloc FlipValue[SearchConst.MAX_N_BRANCHES];
        int idx = 0;
        int ttMovesIdx0 = -1;
        for (int cell = Bits.FirstBit(ref legal); legal != 0; cell = Bits.NextBit(ref legal))
        {
            moveList[idx].Reset();
            moveList[idx].Flip.Calc(search.Board.Player, search.Board.Opponent, cell);
            if (moveList[idx].Flip.Flipped == search.Board.Opponent)
            {
                return Common.SCORE_MAX;
            }

            if (cell == moves[0])
            {
                ttMovesIdx0 = idx;
            }

            ++idx;
        }

        // C++ compiles the ETC block here too (the preprocessor reads the constexpr depths as 0), but depth <= 4 never
        // reaches MID_ETC_DEPTH_NWS; only the "no flips: stop" test in the loop below remains.
        if (ttMovesIdx0 != -1 && moveList[ttMovesIdx0].Flip.Flipped != 0)
        {
            search.Move(moveList[ttMovesIdx0].Flip);
            g = -NegaAlphaOrderingNwsSimple(search, -alpha - 1, depth - 1, false, moveList[ttMovesIdx0].NLegal, searching);
            search.Undo(moveList[ttMovesIdx0].Flip);
            if (v < g)
            {
                v = g;
                bestMove = moveList[ttMovesIdx0].Flip.Pos;
            }

            moveList[ttMovesIdx0].Flip.Flipped = 0;
            moveList[ttMovesIdx0].Value = -Common.INF;
        }

        if (v <= alpha)
        {
            MoveOrdering.MoveListEvaluateNws(search, moveList, canput, moves, depth, alpha, searching);
            for (int moveIdx = 0; moveIdx < canput && searching.Value; ++moveIdx)
            {
                MoveOrdering.SwapNextBestMove(moveList, moveIdx, canput);
                if (moveList[moveIdx].Flip.Flipped == 0)
                {
                    break;
                }

                search.Move(moveList[moveIdx].Flip);
                g = -NegaAlphaOrderingNwsSimple(search, -alpha - 1, depth - 1, false, moveList[moveIdx].NLegal, searching);
                search.Undo(moveList[moveIdx].Flip);
                if (v < g)
                {
                    v = g;
                    bestMove = moveList[moveIdx].Flip.Pos;
                    if (alpha < v)
                    {
                        break;
                    }
                }
            }
        }

        if (searching.Value && search.Context.GlobalSearching)
        {
            search.Context.TranspositionTable.Reg(search, hashCode, depth, alpha, alpha + 1, v, bestMove);
        }

        return v;
    }

    /// <summary>The null-window search of the midgame (<c>nega_alpha_ordering_nws</c>).</summary>
    public static int NegaAlphaOrderingNws(Search search, int alpha, int depth, bool skipped, ulong legal, bool isEndSearch, SearchingFlag searching)
    {
        if (!IsSearching(search, searching))
        {
            return SearchConst.SCORE_UNDEFINED;
        }

        if (isEndSearch)
        {
            if (depth <= SearchConst.MID_TO_END_DEPTH_MPC || (search.MpcLevel == Level.MPC_100 && depth <= SearchConst.MID_TO_END_DEPTH))
            {
                return EndSearch.NegaAlphaEndNws(search, alpha, skipped, legal);
            }
        }
        else if (depth <= SearchConst.MID_SIMPLE_ORDERING_DEPTH)
        {
            return NegaAlphaOrderingNwsSimple(search, alpha, depth, skipped, legal, searching);
        }

        int v = -Common.SCORE_INF;
        ++search.NNodes;
        if (legal == Common.LEGAL_UNDEFINED)
        {
            legal = search.Board.GetLegal();
        }

        if (legal == 0)
        {
            if (skipped)
            {
                return EndSearch.EndEvaluate(search.Board);
            }

            search.Pass();
            v = -NegaAlphaOrderingNws(search, -alpha - 1, depth, true, Common.LEGAL_UNDEFINED, isEndSearch, searching);
            search.Pass();
            return v;
        }

        uint hashCode = search.Board.Hash();
        Span<int> moves = [SearchConst.MOVE_UNDEFINED, SearchConst.MOVE_UNDEFINED];
        if (TranspositionCutoffNws(search, hashCode, depth, alpha, ref v, moves))
        {
            return v;
        }

        if (search.MpcLevel < Level.MPC_100 && depth >= MultiProbCut.USE_MPC_MIN_DEPTH)
        {
            if (MultiProbCut.Mpc(search, alpha, alpha + 1, depth, legal, isEndSearch, ref v, searching))
            {
                return v;
            }
        }

        int bestMove = SearchConst.MOVE_UNDEFINED;
        int g;
        int canput = Bits.PopCount(legal);
        Span<FlipValue> moveList = stackalloc FlipValue[SearchConst.MAX_N_BRANCHES];
        int idx = 0;
        int ttMovesIdx0 = -1;
        for (int cell = Bits.FirstBit(ref legal); legal != 0; cell = Bits.NextBit(ref legal))
        {
            moveList[idx].Reset();
            moveList[idx].Flip.Calc(search.Board.Player, search.Board.Opponent, cell);
            if (moveList[idx].Flip.Flipped == search.Board.Opponent)
            {
                return Common.SCORE_MAX;
            }

            if (cell == moves[0])
            {
                ttMovesIdx0 = idx;
            }

            ++idx;
        }

        int nEtcDone = 0;
        if (depth >= MID_ETC_DEPTH_NWS)
        {
            if (EtcNws(search, moveList, canput, depth, alpha, ref v, out nEtcDone))
            {
                return v;
            }
        }

        if (ttMovesIdx0 != -1 && moveList[ttMovesIdx0].Flip.Flipped != 0)
        {
            search.Move(moveList[ttMovesIdx0].Flip);
            g = -NegaAlphaOrderingNws(search, -alpha - 1, depth - 1, false, moveList[ttMovesIdx0].NLegal, isEndSearch, searching);
            search.Undo(moveList[ttMovesIdx0].Flip);
            if (v < g)
            {
                v = g;
                bestMove = moveList[ttMovesIdx0].Flip.Pos;
            }

            moveList[ttMovesIdx0].Flip.Flipped = 0;
            moveList[ttMovesIdx0].Value = -Common.INF;
        }

        if (v <= alpha)
        {
            MoveOrdering.MoveListEvaluateNws(search, moveList, canput, moves, depth, alpha, searching);
            if (search.UseMultiThread)
            {
                throw new NotSupportedException("The multi-threaded search (YBWC) is not ported yet");
            }

            for (int moveIdx = 0; moveIdx < canput - nEtcDone && IsSearchingFlag(searching); ++moveIdx)
            {
                MoveOrdering.SwapNextBestMove(moveList, moveIdx, canput);
                if (moveList[moveIdx].Flip.Flipped == 0)
                {
                    break;
                }

                search.Move(moveList[moveIdx].Flip);
                g = -NegaAlphaOrderingNws(search, -alpha - 1, depth - 1, false, moveList[moveIdx].NLegal, isEndSearch, searching);
                search.Undo(moveList[moveIdx].Flip);
                if (v < g)
                {
                    v = g;
                    bestMove = moveList[moveIdx].Flip.Pos;
                    if (alpha < v)
                    {
                        break;
                    }
                }
            }
        }

        if (IsSearching(search, searching))
        {
            search.Context.TranspositionTable.Reg(search, hashCode, depth, alpha, alpha + 1, v, bestMove);
        }

        return v;
    }

    // In C++ the NWS loop tests is_searching(searchings), the flags of this search only (not global_searching).
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static bool IsSearchingFlag(SearchingFlag searching) => searching.Value;

    // ---- midsearch.hpp ----

    /// <summary>A depth-1 search with the static evaluation of the children (<c>nega_alpha_eval1</c>).</summary>
    public static int NegaAlphaEval1(Search search, int alpha, int beta, bool skipped)
    {
        ++search.NNodes;
        int v = -Common.SCORE_INF;
        ulong legal = search.Board.GetLegal();
        if (legal == 0)
        {
            if (skipped)
            {
                return EndSearch.EndEvaluate(search.Board);
            }

            search.Pass();
            v = -NegaAlphaEval1(search, -beta, -alpha, true);
            search.Pass();
            return v;
        }

        var flip = new Flip();
        for (int i = 0; i < SearchConst.N_STATIC_CELL_PRIORITY; ++i)
        {
            ulong l = legal & SearchConst.StaticCellPriority[i];
            for (int cell = Bits.FirstBit(ref l); l != 0; cell = Bits.NextBit(ref l))
            {
                flip.Calc(search.Board.Player, search.Board.Opponent, cell);
                search.Move(flip);
                ++search.NNodes;
                int g = -MidEvaluateDiff(search);
                search.Undo(flip);
                if (v < g)
                {
                    if (alpha < g)
                    {
                        if (beta <= g)
                        {
                            return g;
                        }

                        alpha = g;
                    }

                    v = g;
                }
            }
        }

        return v;
    }

    /// <summary>The principal variation search (<c>nega_scout</c>).</summary>
    public static int NegaScout(Search search, int alpha, int beta, int depth, bool skipped, ulong legal, bool isEndSearch, SearchingFlag searching)
    {
        if (!IsSearching(search, searching))
        {
            return SearchConst.SCORE_UNDEFINED;
        }

        if (alpha + 1 == beta)
        {
            return NegaAlphaOrderingNws(search, alpha, depth, skipped, legal, isEndSearch, searching);
        }

        if (isEndSearch && search.NDiscs == Common.HW2 - 4)
        {
            return -EndSearch.Last4(search, -beta, -alpha);
        }

        if (search.NDiscs == Common.HW2)
        {
            return EndSearch.EndEvaluate(search.Board);
        }

        if (!isEndSearch)
        {
            if (depth == 1)
            {
                return NegaAlphaEval1(search, alpha, beta, skipped);
            }

            if (depth == 0)
            {
                ++search.NNodes;
                return MidEvaluateDiff(search);
            }
        }

        ++search.NNodes;
        int firstAlpha = alpha;
        int firstBeta = beta;
        if (isEndSearch)
        {
            int stabRes = EndSearch.StabilityCut(search, ref alpha, ref beta);
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
                return EndSearch.EndEvaluate(search.Board);
            }

            search.Pass();
            v = -NegaScout(search, -beta, -alpha, depth, true, Common.LEGAL_UNDEFINED, isEndSearch, searching);
            search.Pass();
            return v;
        }

        uint hashCode = search.Board.Hash();
        Span<int> moves = [SearchConst.MOVE_UNDEFINED, SearchConst.MOVE_UNDEFINED];
        if (TranspositionCutoff(search, hashCode, depth, ref alpha, ref beta, ref v, moves))
        {
            return v;
        }

        int bestMove = SearchConst.MOVE_UNDEFINED;
        int canput = Bits.PopCount(legal);
        Span<FlipValue> moveList = stackalloc FlipValue[SearchConst.MAX_N_BRANCHES];
        int idx = 0;
        int ttMovesIdx0 = -1;
        ulong legalCopy = legal;
        for (int cell = Bits.FirstBit(ref legalCopy); legalCopy != 0; cell = Bits.NextBit(ref legalCopy))
        {
            moveList[idx].Reset();
            moveList[idx].Flip.Calc(search.Board.Player, search.Board.Opponent, cell);
            if (moveList[idx].Flip.Flipped == search.Board.Opponent)
            {
                return Common.SCORE_MAX;
            }

            if (cell == moves[0])
            {
                ttMovesIdx0 = idx;
            }

            ++idx;
        }

        int nEtcDone = 0;
        if (depth >= MID_ETC_DEPTH)
        {
            if (Etc(search, moveList, canput, depth, ref alpha, ref beta, ref v, out nEtcDone))
            {
                return v;
            }
        }

        if (search.MpcLevel < Level.MPC_100 && depth >= MultiProbCut.USE_MPC_MIN_DEPTH)
        {
            if (MultiProbCut.Mpc(search, alpha, beta, depth, legal, isEndSearch, ref v, searching))
            {
                return v;
            }
        }

        int g;
        if (ttMovesIdx0 != -1 && moveList[ttMovesIdx0].Flip.Flipped != 0)
        {
            search.Move(moveList[ttMovesIdx0].Flip);
            g = -NegaScout(search, -beta, -alpha, depth - 1, false, moveList[ttMovesIdx0].NLegal, isEndSearch, searching);
            search.Undo(moveList[ttMovesIdx0].Flip);
            if (v < g)
            {
                v = g;
                bestMove = moveList[ttMovesIdx0].Flip.Pos;
                if (alpha < v)
                {
                    alpha = v;
                }
            }

            moveList[ttMovesIdx0].Flip.Flipped = 0;
            moveList[ttMovesIdx0].Value = -Common.INF;
        }

        if (alpha < beta)
        {
            MoveOrdering.MoveListEvaluate(search, moveList, canput, moves, depth, alpha, beta, searching);
            if (search.UseMultiThread)
            {
                throw new NotSupportedException("The multi-threaded search (YBWC) is not ported yet");
            }

            for (int moveIdx = 0; moveIdx < canput - nEtcDone && searching.Value; ++moveIdx)
            {
                MoveOrdering.SwapNextBestMove(moveList, moveIdx, canput);
                if (moveList[moveIdx].Flip.Flipped == 0)
                {
                    break;
                }

                search.Move(moveList[moveIdx].Flip);
                if (v == -Common.SCORE_INF)
                {
                    g = -NegaScout(search, -beta, -alpha, depth - 1, false, moveList[moveIdx].NLegal, isEndSearch, searching);
                }
                else
                {
                    g = -NegaAlphaOrderingNws(search, -alpha - 1, depth - 1, false, moveList[moveIdx].NLegal, isEndSearch, searching);
                    if (alpha < g && g < beta)
                    {
                        g = -NegaScout(search, -beta, -g, depth - 1, false, moveList[moveIdx].NLegal, isEndSearch, searching);
                    }
                }

                search.Undo(moveList[moveIdx].Flip);
                if (v < g)
                {
                    v = g;
                    bestMove = moveList[moveIdx].Flip.Pos;
                    if (alpha < v)
                    {
                        if (beta <= v)
                        {
                            break;
                        }

                        alpha = v;
                    }
                }
            }
        }

        if (searching.Value && search.Context.GlobalSearching)
        {
            search.Context.TranspositionTable.Reg(search, hashCode, depth, firstAlpha, firstBeta, v, bestMove);
        }

        return v;
    }

    /// <summary>
    /// The root search over the moves in <paramref name="legal"/> (<c>first_nega_scout_legal</c>); the moves found by the
    /// clog search are taken with their exact values. Returns the value and the best move.
    /// </summary>
    public static (int Value, int Move) FirstNegaScoutLegal(Search search, int alpha, int beta, int depth, bool isEndSearch, IReadOnlyList<ClogResult> clogs, ulong legal, SearchingFlag searching)
    {
        ++search.NNodes;
        int g, v = -Common.SCORE_INF, firstAlpha = alpha;
        if (legal == 0)
        {
            return (SearchConst.SCORE_UNDEFINED, -1);
        }

        bool isAllLegal = legal == search.Board.GetLegal();
        int bestMove = SearchConst.MOVE_UNDEFINED;
        foreach (ClogResult clog in clogs)
        {
            if ((legal & (1UL << clog.Pos)) != 0)
            {
                if (v < clog.Val)
                {
                    v = clog.Val;
                    bestMove = clog.Pos;
                    if (alpha < v)
                    {
                        alpha = v;
                    }
                }

                legal &= ~(1UL << clog.Pos);
            }
        }

        uint hashCode = search.Board.Hash();
        if (alpha < beta && legal != 0)
        {
            int canput = Bits.PopCount(legal);
            Span<FlipValue> moveList = stackalloc FlipValue[SearchConst.MAX_N_BRANCHES];
            int idx = 0;
            for (int cell = Bits.FirstBit(ref legal); legal != 0; cell = Bits.NextBit(ref legal))
            {
                moveList[idx].Reset();
                moveList[idx].Flip.Calc(search.Board.Player, search.Board.Opponent, cell);
                if (moveList[idx].Flip.Flipped == search.Board.Opponent)
                {
                    return (Common.SCORE_MAX, cell);
                }

                ++idx;
            }

            Span<int> moves = [SearchConst.MOVE_UNDEFINED, SearchConst.MOVE_UNDEFINED];
            search.Context.TranspositionTable.GetMovesAnyLevel(search.Board, hashCode, moves);
            MoveOrdering.MoveListEvaluate(search, moveList, canput, moves, depth, alpha, beta, searching);
            if (search.UseMultiThread)
            {
                throw new NotSupportedException("The multi-threaded search (YBWC) is not ported yet");
            }

            for (int moveIdx = 0; moveIdx < canput && searching.Value; ++moveIdx)
            {
                MoveOrdering.SwapNextBestMove(moveList, moveIdx, canput);
                search.Move(moveList[moveIdx].Flip);
                if (v == -Common.SCORE_INF)
                {
                    g = -NegaScout(search, -beta, -alpha, depth - 1, false, moveList[moveIdx].NLegal, isEndSearch, searching);
                }
                else
                {
                    g = -NegaAlphaOrderingNws(search, -alpha - 1, depth - 1, false, moveList[moveIdx].NLegal, isEndSearch, searching);
                    if (alpha < g && g < beta)
                    {
                        g = -NegaScout(search, -beta, -g, depth - 1, false, moveList[moveIdx].NLegal, isEndSearch, searching);
                    }
                }

                search.Undo(moveList[moveIdx].Flip);
                if (v < g)
                {
                    v = g;
                    bestMove = moveList[moveIdx].Flip.Pos;
                    if (alpha < v)
                    {
                        if (beta <= v)
                        {
                            break;
                        }

                        alpha = v;
                    }
                }
            }
        }

        if (searching.Value && search.Context.GlobalSearching && isAllLegal)
        {
            search.Context.TranspositionTable.Reg(search, hashCode, depth, firstAlpha, beta, v, bestMove);
        }

        return (v, bestMove);
    }
}
